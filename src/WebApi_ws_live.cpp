// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi_ws_live.h"
#include "Datastore.h"
#include "RuntimeData.h"
#include "Utils.h"
#include "WebApi.h"
#include <battery/Controller.h>
#include <battery/Stats.h>
#include <gridcharger/Controller.h>
#include <gridcharger/Stats.h>
#include <powermeter/Controller.h>
#include "defaults.h"
#include <solarcharger/Controller.h>
#include <AsyncJson.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <new>

#undef TAG
static const char* TAG = "webapi";

#ifndef PIN_MAPPING_REQUIRED
    #define PIN_MAPPING_REQUIRED 0
#endif

WebApiWsLiveClass::WebApiWsLiveClass()
    : _ws("/livedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::sendDataTaskCb, this))
    , _powerHistoryTask(POWER_HISTORY_INTERVAL_MINUTES * TASK_MINUTE, TASK_FOREVER, std::bind(&WebApiWsLiveClass::powerHistoryTaskCb, this))
{
}

void WebApiWsLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.on("/api/livedata/status", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiWsLiveClass::onLivedataStatus, this, _1)));
    // ESPAsyncWebServer treats plain URI matchers as backward-compatible prefixes.
    // Register the more specific config endpoint first so it is not swallowed by
    // /api/livedata/power-history.
    server.on("/api/livedata/power-history/config", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiWsLiveClass::onPowerHistoryConfigGet, this, _1)));
    server.on("/api/livedata/power-history/config", HTTP_POST, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiWsLiveClass::onPowerHistoryConfigPost, this, _1)));
    server.on("/api/livedata/power-history", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiWsLiveClass::onPowerHistoryStatus, this, _1)));
    server.on("/api/livedata/daily-yield", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiWsLiveClass::onDailyYieldHistory, this, _1)));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();

    scheduler.addTask(_powerHistoryTask);
    configurePowerHistory();
    _simpleDigestAuth.setUsername(AUTH_USERNAME);
    _simpleDigestAuth.setRealm("live websocket");
    _simpleDigestAuth.setAuthType(AsyncAuthType::AUTH_DIGEST);

    reload();
}

void WebApiWsLiveClass::configurePowerHistory()
{
    _powerHistoryTask.disable();

    const auto& config = Configuration.get().PowerHistory;
    const uint8_t intervalMinutes = std::clamp<uint8_t>(config.IntervalMinutes, 1, 60);
    uint64_t serials[INV_MAX_COUNT] = {};
    uint8_t inverterCount = 0;
    for (uint8_t i = 0; i < INV_MAX_COUNT; ++i) {
        if (config.InverterSerials[i] != 0) {
            serials[inverterCount++] = config.InverterSerials[i];
        }
    }

    const uint8_t seriesCount = inverterCount + (config.InverterTotalEnabled ? 1 : 0);
    const bool hasSource = config.PowerMeterEnabled || seriesCount > 0;
    const uint16_t capacity = 24U * 60U / intervalMinutes;
    std::unique_ptr<uint32_t[]> timestamps;
    std::unique_ptr<int32_t[]> gridPower;
    std::unique_ptr<int16_t[]> inverterPower;
    if (config.Enabled && hasSource) {
        timestamps.reset(new (std::nothrow) uint32_t[capacity]);
        if (config.PowerMeterEnabled) {
            gridPower.reset(new (std::nothrow) int32_t[capacity]);
        }
        if (seriesCount > 0) {
            inverterPower.reset(new (std::nothrow) int16_t[capacity * seriesCount]);
        }
    }

    const bool allocationSucceeded = timestamps
        && (!config.PowerMeterEnabled || gridPower)
        && (seriesCount == 0 || inverterPower);

    {
        std::lock_guard<std::mutex> lock(_powerHistoryMutex);
        _powerHistoryTimestamps = std::move(timestamps);
        _powerHistoryGridPower = std::move(gridPower);
        _powerHistoryInverterPower = std::move(inverterPower);
        std::copy(std::begin(serials), std::end(serials), std::begin(_powerHistorySerials));
        _powerHistoryWriteIndex = 0;
        _powerHistoryCount = 0;
        _powerHistoryCapacity = allocationSucceeded ? capacity : 0;
        _powerHistoryInverterCount = inverterCount;
        _powerHistorySeriesCount = seriesCount;
        _powerHistoryIntervalMinutes = intervalMinutes;
        _powerHistoryPowerMeterEnabled = config.PowerMeterEnabled;
        _powerHistoryInverterTotalEnabled = config.InverterTotalEnabled;
        _powerHistoryEnabled = config.Enabled && hasSource && allocationSucceeded;
    }

    if (config.Enabled && hasSource && !allocationSucceeded) {
        ESP_LOGE(TAG, "Could not allocate power history buffer");
        return;
    }
    if (_powerHistoryEnabled) {
        _powerHistoryTask.setInterval(intervalMinutes * TASK_MINUTE);
        _powerHistoryTask.enable();
    }
}

void WebApiWsLiveClass::powerHistoryTaskCb()
{
    const auto timestamp = static_cast<uint32_t>(std::time(nullptr));
    // Do not create points with an invalid clock. The graph starts as soon as NTP is synchronized.
    if (timestamp < 1609459200U) {
        return;
    }

    std::lock_guard<std::mutex> lock(_powerHistoryMutex);
    if (!_powerHistoryEnabled || _powerHistoryCapacity == 0) {
        return;
    }

    const uint16_t pointIndex = _powerHistoryWriteIndex;
    _powerHistoryTimestamps[pointIndex] = timestamp;
    if (_powerHistoryPowerMeterEnabled) {
        _powerHistoryGridPower[pointIndex] = Configuration.get().PowerMeter.Enabled
            ? static_cast<int32_t>(std::lround(PowerMeter.getPowerTotal()))
            : POWER_HISTORY_INVALID_GRID;
    }

    auto readInverterPower = [](const std::shared_ptr<InverterAbstract>& inverter, int32_t& power) {
        if (inverter == nullptr || inverter->Statistics()->getLastUpdate() == 0) {
            return false;
        }

        float measuredPower = 0.0f;
        bool hasPower = false;
        for (auto channel : inverter->Statistics()->getChannelsByType(TYPE_AC)) {
            if (!inverter->Statistics()->hasChannelFieldValue(TYPE_AC, channel, FLD_PAC)) {
                continue;
            }
            measuredPower += inverter->Statistics()->getChannelFieldValue(TYPE_AC, channel, FLD_PAC);
            hasPower = true;
        }
        power = static_cast<int32_t>(std::lround(measuredPower));
        return hasPower;
    };

    for (uint8_t i = 0; i < _powerHistoryInverterCount; ++i) {
        auto& value = _powerHistoryInverterPower[pointIndex * _powerHistorySeriesCount + i];
        value = POWER_HISTORY_INVALID_INVERTER;
        auto inverter = Hoymiles.getInverterBySerial(_powerHistorySerials[i]);
        int32_t power = 0;
        if (readInverterPower(inverter, power)) {
            value = static_cast<int16_t>(std::clamp<int32_t>(power, -32767, 32767));
        }
    }

    if (_powerHistoryInverterTotalEnabled) {
        auto& totalValue = _powerHistoryInverterPower[
            pointIndex * _powerHistorySeriesCount + _powerHistoryInverterCount];
        totalValue = POWER_HISTORY_INVALID_INVERTER;
        int32_t totalPower = 0;
        bool hasTotalPower = false;
        for (uint8_t i = 0; i < Hoymiles.getNumInverters(); ++i) {
            int32_t inverterPower = 0;
            if (readInverterPower(Hoymiles.getInverterByPos(i), inverterPower)) {
                totalPower += inverterPower;
                hasTotalPower = true;
            }
        }
        if (hasTotalPower) {
            totalValue = static_cast<int16_t>(std::clamp<int32_t>(totalPower, -32767, 32767));
        }
    }

    _powerHistoryWriteIndex = (_powerHistoryWriteIndex + 1) % _powerHistoryCapacity;
    _powerHistoryCount = std::min<uint16_t>(_powerHistoryCount + 1, _powerHistoryCapacity);
}

void WebApiWsLiveClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) {
        return;
    }

    _ws.enable(false);
    _simpleDigestAuth.setPassword(config.Security.Password);
    _ws.addMiddleware(&_simpleDigestAuth);
    _ws.closeAll();
    _ws.enable(true);
}

void WebApiWsLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsLiveClass::generateOnBatteryJsonResponse(JsonVariant& root, bool all)
{
    auto const& config = Configuration.get();
    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;

    auto solarChargerAge = SolarCharger.getStats()->getAgeMillis();
    if (all || (solarChargerAge > 0 && (millis() - _lastPublishSolarCharger) > solarChargerAge)) {
        auto solarchargerObj = root["solarcharger"].to<JsonObject>();
        solarchargerObj["enabled"] = config.SolarCharger.Enabled;

        if (config.SolarCharger.Enabled) {
            float power = 0;
            auto outputPower = SolarCharger.getStats()->getOutputPowerWatts();
            auto panelPower = SolarCharger.getStats()->getPanelPowerWatts();

            if (outputPower) {
                power = *outputPower;
            }

            if (power == 0 && panelPower) {
                power = *panelPower;
            }

            addTotalField(solarchargerObj, "power", power, "W", 1);

            auto yieldDay = SolarCharger.getStats()->getYieldDay();
            if (yieldDay) {
                addTotalField(solarchargerObj, "yieldDay", *yieldDay, "Wh", 0);
            }

            auto yieldTotal = SolarCharger.getStats()->getYieldTotal();
            if (yieldTotal) {
                addTotalField(solarchargerObj, "yieldTotal", *yieldTotal, "kWh", 2);
            }
        }

        if (!all) { _lastPublishSolarCharger = millis(); }
    }

    auto gridChargerStats = GridCharger.getStats();
    if (all || (gridChargerStats->getLastUpdate() - _lastPublishGridCharger) < halfOfAllMillis ) {
        auto gridChargerObj = root["gridcharger"].to<JsonObject>();
        gridChargerObj["enabled"] = config.GridCharger.Enabled;

        if (config.GridCharger.Enabled) {
            auto oInputPower = gridChargerStats->getInputPower();
            float pwr = oInputPower.value_or(0.0f);
            addTotalField(gridChargerObj, "Power", pwr, "W", 2);
        }

        if (!all) { _lastPublishGridCharger = millis(); }
    }

    auto spStats = Battery.getStats();
    if (all || spStats->updateAvailable(_lastPublishBattery)) {
        auto batteryObj = root["battery"].to<JsonObject>();
        batteryObj["enabled"] = config.Battery.Enabled;

        if (config.Battery.Enabled) {
            if (spStats->isSoCValid()) {
                addTotalField(batteryObj, "soc", spStats->getSoC(), "%", spStats->getSoCPrecision());
            }

            if (spStats->isVoltageValid()) {
                addTotalField(batteryObj, "voltage", spStats->getVoltage(), "V", 2);
            }

            if (spStats->isCurrentValid()) {
                addTotalField(batteryObj, "current", spStats->getChargeCurrent(), "A", spStats->getChargeCurrentPrecision());
            }

            if (spStats->isVoltageValid() && spStats->isCurrentValid()) {
                addTotalField(batteryObj, "power", spStats->getVoltage() * spStats->getChargeCurrent(), "W", 1);
            }
        }

        if (!all) { _lastPublishBattery = millis(); }
    }

    if (all || (PowerMeter.getLastUpdate() - _lastPublishPowerMeter) < halfOfAllMillis) {
        auto powerMeterObj = root["power_meter"].to<JsonObject>();
        powerMeterObj["enabled"] = config.PowerMeter.Enabled;

        if (config.PowerMeter.Enabled) {
            addTotalField(powerMeterObj, "Power", PowerMeter.getPowerTotal(), "W", 1);
        }

        if (!all) { _lastPublishPowerMeter = millis(); }
    }
}

void WebApiWsLiveClass::sendOnBatteryStats()
{
    JsonDocument root;
    JsonVariant var = root;

    bool all = (millis() - _lastPublishOnBatteryFull) > 10 * 1000;
    if (all) { _lastPublishOnBatteryFull = millis(); }
    generateOnBatteryJsonResponse(var, all);

    if (root.isNull()) { return; }

    if (Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        String buffer;
        serializeJson(root, buffer);

        _ws.textAll(buffer);;
    }
}

void WebApiWsLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    sendOnBatteryStats();

    // Loop all inverters
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }

        const uint32_t lastUpdateInternal = inv->Statistics()->getLastUpdateFromInternal();
        if (!((lastUpdateInternal > 0 && lastUpdateInternal > _lastPublishStats[i]) || (millis() - _lastPublishStats[i] > (10 * 1000)))) {
            continue;
        }

        _lastPublishStats[i] = millis();

        try {
            std::lock_guard<std::mutex> lock(_mutex);
            JsonDocument root;
            JsonVariant var = root;

            auto invArray = var["inverters"].to<JsonArray>();
            auto invObject = invArray.add<JsonObject>();

            generateCommonJsonResponse(var);
            generateInverterCommonJsonResponse(invObject, inv);
            generateInverterChannelJsonResponse(invObject, inv);

            if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
                continue;
            }

            String buffer;
            serializeJson(root, buffer);

            _ws.textAll(buffer);

        } catch (const std::bad_alloc& bad_alloc) {
            ESP_LOGE(TAG, "Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".", bad_alloc.what());
        } catch (const std::exception& exc) {
            ESP_LOGE(TAG, "Unknown exception in /api/livedata/status. Reason: \"%s\".", exc.what());
        }
    }
}

void WebApiWsLiveClass::generateCommonJsonResponse(JsonVariant& root)
{
    auto totalObj = root["total"].to<JsonObject>();
    addTotalField(totalObj, "Power", Datastore.getTotalAcPowerEnabled(), "W", Datastore.getTotalAcPowerDigits());
    addTotalField(totalObj, "YieldDay", Datastore.getTotalAcYieldDayEnabled(), "Wh", Datastore.getTotalAcYieldDayDigits());
    addTotalField(totalObj, "YieldTotal", Datastore.getTotalAcYieldTotalEnabled(), "kWh", Datastore.getTotalAcYieldTotalDigits());
    totalObj["DailyYieldHistoryEnabled"] = Configuration.get().PowerHistory.DailyYieldEnabled;

    JsonObject hintObj = root["hints"].to<JsonObject>();
    struct tm timeinfo;
    hintObj["time_sync"] = !getLocalTime(&timeinfo, 5);
    hintObj["radio_problem"] = (Hoymiles.getRadioNrf()->isInitialized() && (!Hoymiles.getRadioNrf()->isConnected() || !Hoymiles.getRadioNrf()->isPVariant())) || (Hoymiles.getRadioCmt()->isInitialized() && (!Hoymiles.getRadioCmt()->isConnected()));
    hintObj["default_password"] = strcmp(Configuration.get().Security.Password, ACCESS_POINT_PASSWORD) == 0;

    hintObj["pin_mapping_issue"] = PIN_MAPPING_REQUIRED && !PinMapping.isMappingSelected();
}

void WebApiWsLiveClass::generateInverterCommonJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    root["serial"] = inv->serialString();
    root["name"] = inv->name();
    root["order"] = inv_cfg->Order;
    root["data_age"] = (millis() - inv->Statistics()->getLastUpdate()) / 1000;
    root["data_age_ms"] = millis() - inv->Statistics()->getLastUpdate();
    root["poll_enabled"] = inv->getEnablePolling();
    root["reachable"] = inv->isReachable();
    root["producing"] = inv->isProducing();
    root["limit_relative"] = inv->SystemConfigPara()->getLimitPercent();
    if (inv->DevInfo()->getMaxPower() > 0) {
        root["limit_absolute"] = inv->SystemConfigPara()->getLimitPercent() * inv->DevInfo()->getMaxPower() / 100.0;
    } else {
        root["limit_absolute"] = -1;
    }
    root["radio_stats"]["tx_request"] = inv->RadioStats.TxRequestData;
    root["radio_stats"]["tx_re_request"] = inv->RadioStats.TxReRequestFragment;
    root["radio_stats"]["rx_success"] = inv->RadioStats.RxSuccess;
    root["radio_stats"]["rx_fail_nothing"] = inv->RadioStats.RxFailNoAnswer;
    root["radio_stats"]["rx_fail_partial"] = inv->RadioStats.RxFailPartialAnswer;
    root["radio_stats"]["rx_fail_corrupt"] = inv->RadioStats.RxFailCorruptData;
    root["radio_stats"]["rssi"] = inv->getLastRssi();
}

void WebApiWsLiveClass::generateInverterChannelJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    // Loop all channels
    for (auto& t : inv->Statistics()->getChannelTypes()) {
        auto chanTypeObj = root[inv->Statistics()->getChannelTypeName(t)].to<JsonObject>();
        for (auto& c : inv->Statistics()->getChannelsByType(t)) {
            if (t == TYPE_DC) {
                chanTypeObj[String(static_cast<uint8_t>(c))]["name"]["u"] = inv_cfg->channel[c].Name;
            }
            addField(chanTypeObj, inv, t, c, FLD_PAC);
            addField(chanTypeObj, inv, t, c, FLD_UAC);
            addField(chanTypeObj, inv, t, c, FLD_IAC);
            if (t == TYPE_INV) {
                addField(chanTypeObj, inv, t, c, FLD_PDC, "Power DC");
            } else {
                addField(chanTypeObj, inv, t, c, FLD_PDC);
            }
            addField(chanTypeObj, inv, t, c, FLD_UDC);
            addField(chanTypeObj, inv, t, c, FLD_IDC);
            addField(chanTypeObj, inv, t, c, FLD_YD);
            addField(chanTypeObj, inv, t, c, FLD_YT);
            addField(chanTypeObj, inv, t, c, FLD_F);
            addField(chanTypeObj, inv, t, c, FLD_T);
            addField(chanTypeObj, inv, t, c, FLD_PF);
            addField(chanTypeObj, inv, t, c, FLD_Q);
            addField(chanTypeObj, inv, t, c, FLD_EFF);
            if (t == TYPE_DC && inv->Statistics()->getStringMaxPower(c) > 0) {
                addField(chanTypeObj, inv, t, c, FLD_IRR);
                chanTypeObj[String(c)][inv->Statistics()->getChannelFieldName(t, c, FLD_IRR)]["max"] = inv->Statistics()->getStringMaxPower(c);
            }
        }
    }

    if (inv->Statistics()->hasChannelFieldValue(TYPE_INV, CH0, FLD_EVT_LOG)) {
        root["events"] = inv->EventLog()->getEntryCount();
    } else {
        root["events"] = -1;
    }
}

void WebApiWsLiveClass::addField(JsonObject& root, std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, String topic)
{
    if (inv->Statistics()->hasChannelFieldValue(type, channel, fieldId)) {
        String chanName;
        if (topic == "") {
            chanName = inv->Statistics()->getChannelFieldName(type, channel, fieldId);
        } else {
            chanName = topic;
        }
        String chanNum;
        chanNum = channel;
        root[chanNum][chanName]["v"] = inv->Statistics()->getChannelFieldValue(type, channel, fieldId);
        root[chanNum][chanName]["u"] = inv->Statistics()->getChannelFieldUnit(type, channel, fieldId);
        root[chanNum][chanName]["d"] = inv->Statistics()->getChannelFieldDigits(type, channel, fieldId);
    }
}

void WebApiWsLiveClass::addTotalField(JsonObject& root, const String& name, const float value, const String& unit, const uint8_t digits)
{
    root[name]["v"] = value;
    root[name]["u"] = unit;
    root[name]["d"] = digits;
}

void WebApiWsLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        ESP_LOGD(TAG, "Websocket: [%s][%" PRIu32 "] connect", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        ESP_LOGD(TAG, "Websocket: [%s][%" PRIu32 "] disconnect", server->url(), client->id());
    }
}

void WebApiWsLiveClass::onPowerHistoryStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    uint8_t hours = 24;
    if (request->hasParam("hours") && request->getParam("hours")->value().toInt() == 12) {
        hours = 12;
    }

    const uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    const uint32_t cutoff = now > hours * 60U * 60U ? now - hours * 60U * 60U : 0;

    std::lock_guard<std::mutex> lock(_powerHistoryMutex);
    // Keep the complete JSON response inside one small contiguous buffer. A 32 KiB
    // response buffer is too large for a fragmented classic ESP32 heap and causes
    // operator new[] to abort. Account conservatively for escaped inverter names
    // and the number of numeric series when selecting the output resolution.
    static constexpr size_t responseBufferSize = 8U * 1024U;
    const size_t metadataBudget = 512U
        + _powerHistoryInverterCount * (96U + INV_MAX_NAME_STRLEN * 6U);
    const size_t pointBudget = 16U
        + (_powerHistoryPowerMeterEnabled ? 12U : 0U)
        + _powerHistorySeriesCount * 12U;
    const size_t pointSpace = responseBufferSize > metadataBudget
        ? responseBufferSize - metadataBudget
        : 1024U;
    const uint16_t responsePointLimit = std::clamp<uint16_t>(
        static_cast<uint16_t>(pointSpace / std::max<size_t>(pointBudget, 1U)), 12U, 240U);
    const uint16_t oldestIndex = _powerHistoryCapacity == 0
        ? 0
        : (_powerHistoryWriteIndex + _powerHistoryCapacity - _powerHistoryCount) % _powerHistoryCapacity;
    uint16_t samplesInRange = 0;
    for (uint16_t offset = 0; offset < _powerHistoryCount; ++offset) {
        const uint16_t pointIndex = (oldestIndex + offset) % _powerHistoryCapacity;
        if (_powerHistoryTimestamps[pointIndex] >= cutoff) {
            ++samplesInRange;
        }
    }
    const uint8_t bucketSize = std::max<uint8_t>(
        1, (samplesInRange + responsePointLimit - 1) / responsePointLimit);

    AsyncResponseStream* response;
    try {
        response = request->beginResponseStream("application/json", responseBufferSize);
    } catch (const std::bad_alloc&) {
        ESP_LOGE(TAG, "Could not allocate power history response buffer");
        request->send(503, "text/plain", "Not enough memory for power history response");
        return;
    }
    response->addHeader("Cache-Control", "no-store");
    response->printf("{\"enabled\":%s,\"power_meter\":%s,\"inverter_total\":%s,\"sample_interval\":%u,\"display_interval\":%u,\"stored_points\":%u,\"hours\":%u,\"inverters\":[",
        _powerHistoryEnabled ? "true" : "false",
        _powerHistoryPowerMeterEnabled ? "true" : "false",
        _powerHistoryInverterTotalEnabled ? "true" : "false",
        _powerHistoryIntervalMinutes * 60U,
        _powerHistoryIntervalMinutes * bucketSize * 60U,
        samplesInRange,
        hours);

    for (uint8_t i = 0; i < _powerHistoryInverterCount; ++i) {
        if (i > 0) {
            response->print(',');
        }

        JsonDocument inverterDocument;
        auto inverterObject = inverterDocument.to<JsonObject>();
        auto inverter = Hoymiles.getInverterBySerial(_powerHistorySerials[i]);
        if (inverter != nullptr) {
            inverterObject["serial"] = inverter->serialString();
            inverterObject["name"] = inverter->name();
        } else {
            char serial[17];
            snprintf(serial, sizeof(serial), "%" PRIx64, _powerHistorySerials[i]);
            inverterObject["serial"] = serial;
            inverterObject["name"] = inverterObject["serial"];
        }
        serializeJson(inverterDocument, *response);
    }

    response->print("],\"points\":[");

    int64_t gridSum = 0;
    uint8_t gridCount = 0;
    int64_t inverterSums[INV_MAX_COUNT + 1] = {};
    uint8_t inverterCounts[INV_MAX_COUNT + 1] = {};
    uint8_t bucketSamples = 0;
    uint32_t bucketTimestamp = 0;
    bool firstPoint = true;

    auto writeBucket = [&]() {
        if (bucketSamples == 0) {
            return;
        }
        if (!firstPoint) {
            response->print(',');
        }
        firstPoint = false;
        response->printf("[%" PRIu32, bucketTimestamp);
        if (_powerHistoryPowerMeterEnabled) {
            response->print(',');
            if (gridCount == 0) {
                response->print("null");
            } else {
                response->print(static_cast<int32_t>(std::lround(static_cast<double>(gridSum) / gridCount)));
            }
        }
        for (uint8_t inverterIndex = 0; inverterIndex < _powerHistorySeriesCount; ++inverterIndex) {
            response->print(',');
            if (inverterCounts[inverterIndex] == 0) {
                response->print("null");
            } else {
                response->print(static_cast<int32_t>(std::lround(static_cast<double>(inverterSums[inverterIndex]) / inverterCounts[inverterIndex])));
            }
        }
        response->print(']');

        gridSum = 0;
        gridCount = 0;
        std::fill(std::begin(inverterSums), std::end(inverterSums), 0);
        std::fill(std::begin(inverterCounts), std::end(inverterCounts), 0);
        bucketSamples = 0;
    };

    for (uint16_t offset = 0; offset < _powerHistoryCount; ++offset) {
        const uint16_t pointIndex = (oldestIndex + offset) % _powerHistoryCapacity;
        const uint32_t timestamp = _powerHistoryTimestamps[pointIndex];
        if (timestamp < cutoff) {
            continue;
        }

        bucketTimestamp = timestamp;
        ++bucketSamples;
        if (_powerHistoryPowerMeterEnabled && _powerHistoryGridPower[pointIndex] != POWER_HISTORY_INVALID_GRID) {
            gridSum += _powerHistoryGridPower[pointIndex];
            ++gridCount;
        }
        for (uint8_t inverterIndex = 0; inverterIndex < _powerHistorySeriesCount; ++inverterIndex) {
            const int16_t value = _powerHistoryInverterPower[pointIndex * _powerHistorySeriesCount + inverterIndex];
            if (value == POWER_HISTORY_INVALID_INVERTER) {
                continue;
            }
            inverterSums[inverterIndex] += value;
            ++inverterCounts[inverterIndex];
        }

        if (bucketSamples >= bucketSize) {
            writeBucket();
        }
    }
    writeBucket();

    response->print("]}");
    request->send(response);
}

void WebApiWsLiveClass::onPowerHistoryConfigGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const auto& historyConfig = Configuration.get().PowerHistory;
    root["enabled"] = historyConfig.Enabled;
    root["power_meter_enabled"] = historyConfig.PowerMeterEnabled;
    root["power_meter_available"] = Configuration.get().PowerMeter.Enabled;
    root["inverter_total_enabled"] = historyConfig.InverterTotalEnabled;
    root["interval_minutes"] = historyConfig.IntervalMinutes;
    root["daily_yield_enabled"] = historyConfig.DailyYieldEnabled;
    root["daily_yield_days"] = historyConfig.DailyYieldDays;

    auto inverterArray = root["inverters"].to<JsonArray>();
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); ++i) {
        auto inverter = Hoymiles.getInverterByPos(i);
        if (inverter == nullptr) {
            continue;
        }

        auto item = inverterArray.add<JsonObject>();
        item["serial"] = inverter->serialString();
        item["name"] = inverter->name();
        item["enabled"] = std::find(std::begin(historyConfig.InverterSerials), std::end(historyConfig.InverterSerials), inverter->serial())
            != std::end(historyConfig.InverterSerials);
    }
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiWsLiveClass::onPowerHistoryConfigPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig().PowerHistory;
        config.Enabled = root["enabled"] | false;
        config.PowerMeterEnabled = root["power_meter_enabled"] | false;
        config.InverterTotalEnabled = root["inverter_total_enabled"] | false;
        config.IntervalMinutes = std::clamp<uint8_t>(root["interval_minutes"] | POWER_HISTORY_INTERVAL_MINUTES, 1, 60);
        config.DailyYieldEnabled = root["daily_yield_enabled"] | false;
        const uint8_t dailyYieldDays = root["daily_yield_days"] | DAILY_YIELD_HISTORY_DAYS;
        config.DailyYieldDays = dailyYieldDays <= 7 ? 7 : (dailyYieldDays <= 14 ? 14 : 30);
        std::fill(std::begin(config.InverterSerials), std::end(config.InverterSerials), 0);

        uint8_t inverterIndex = 0;
        for (JsonVariant item : root["inverters"].as<JsonArray>()) {
            if (!(item["enabled"] | false) || inverterIndex >= INV_MAX_COUNT) {
                continue;
            }
            const char* serial = item["serial"].as<const char*>();
            if (serial != nullptr) {
                const uint64_t parsedSerial = strtoull(serial, nullptr, 16);
                if (parsedSerial != 0) {
                    config.InverterSerials[inverterIndex++] = parsedSerial;
                }
            }
        }
    }

    auto& retMsg = response->getRoot();
    WebApi.writeConfig(retMsg);
    configurePowerHistory();
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiWsLiveClass::onDailyYieldHistory(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    const auto& config = Configuration.get().PowerHistory;
    const uint8_t retentionDays = config.DailyYieldDays <= 7 ? 7 : (config.DailyYieldDays <= 14 ? 14 : 30);
    RuntimeClass::DailyYieldRecord records[RuntimeClass::DAILY_YIELD_MAX_DAYS];
    const uint8_t recordCount = config.DailyYieldEnabled
        ? RuntimeData.getDailyYieldHistory(records, retentionDays)
        : 0;

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    root["enabled"] = config.DailyYieldEnabled;
    root["retention_days"] = retentionDays;
    JsonArray history = root["records"].to<JsonArray>();
    for (uint8_t i = 0; i < recordCount; ++i) {
        JsonObject item = history.add<JsonObject>();
        item["day"] = records[i].Day;
        item["yield_wh"] = records[i].YieldWh;
        item["today"] = records[i].IsToday;
    }
    response->addHeader("Cache-Control", "no-store");
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiWsLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();
        auto invArray = root["inverters"].to<JsonArray>();
        auto serial = WebApi.parseSerialFromRequest(request);

        if (serial > 0) {
            auto inv = Hoymiles.getInverterBySerial(serial);
            if (inv != nullptr) {
                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
                generateInverterChannelJsonResponse(invObject, inv);
            }
        } else {
            // Loop all inverters
            for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
                auto inv = Hoymiles.getInverterByPos(i);
                if (inv == nullptr) {
                    continue;
                }

                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
            }
        }

        generateCommonJsonResponse(root);

        generateOnBatteryJsonResponse(root, true);

        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    } catch (const std::bad_alloc& bad_alloc) {
        ESP_LOGE(TAG, "Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".", bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        ESP_LOGE(TAG, "Unknown exception in /api/livedata/status. Reason: \"%s\".", exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
