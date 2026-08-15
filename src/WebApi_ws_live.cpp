// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi_ws_live.h"
#include "Datastore.h"
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

#undef TAG
static const char* TAG = "webapi";

#ifndef PIN_MAPPING_REQUIRED
    #define PIN_MAPPING_REQUIRED 0
#endif

WebApiWsLiveClass::WebApiWsLiveClass()
    : _ws("/livedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::sendDataTaskCb, this))
    , _powerHistoryTask(POWER_HISTORY_SAMPLE_MINUTES * TASK_MINUTE, TASK_FOREVER, std::bind(&WebApiWsLiveClass::powerHistoryTaskCb, this))
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
    server.on("/api/livedata/power-history", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiWsLiveClass::onPowerHistoryStatus, this, _1)));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();

    scheduler.addTask(_powerHistoryTask);
    _powerHistoryTask.enable();
    _simpleDigestAuth.setUsername(AUTH_USERNAME);
    _simpleDigestAuth.setRealm("live websocket");
    _simpleDigestAuth.setAuthType(AsyncAuthType::AUTH_DIGEST);

    reload();
}

void WebApiWsLiveClass::powerHistoryTaskCb()
{
    const auto timestamp = static_cast<uint32_t>(std::time(nullptr));
    // Do not create points with an invalid clock. The graph starts as soon as NTP is synchronized.
    if (timestamp < 1609459200U) {
        return;
    }

    const uint8_t inverterCount = std::min<uint8_t>(Hoymiles.getNumInverters(), INV_MAX_COUNT);
    uint64_t serials[INV_MAX_COUNT] = {};
    for (uint8_t i = 0; i < inverterCount; ++i) {
        auto inverter = Hoymiles.getInverterByPos(i);
        if (inverter != nullptr) {
            serials[i] = inverter->serial();
        }
    }

    std::lock_guard<std::mutex> lock(_powerHistoryMutex);

    bool inverterLayoutChanged = inverterCount != _powerHistoryInverterCount;
    for (uint8_t i = 0; i < inverterCount && !inverterLayoutChanged; ++i) {
        inverterLayoutChanged = serials[i] != _powerHistorySerials[i];
    }

    if (inverterLayoutChanged) {
        _powerHistoryWriteIndex = 0;
        _powerHistoryCount = 0;
        _powerHistoryInverterCount = inverterCount;
        std::copy(std::begin(serials), std::end(serials), std::begin(_powerHistorySerials));
    }

    auto& point = _powerHistory[_powerHistoryWriteIndex];
    point.Timestamp = timestamp;
    point.GridPower = Configuration.get().PowerMeter.Enabled
        ? static_cast<int32_t>(std::lround(PowerMeter.getPowerTotal()))
        : POWER_HISTORY_INVALID_GRID;
    std::fill(std::begin(point.InverterPower), std::end(point.InverterPower), POWER_HISTORY_INVALID_INVERTER);

    for (uint8_t i = 0; i < inverterCount; ++i) {
        auto inverter = Hoymiles.getInverterByPos(i);
        if (inverter == nullptr || inverter->Statistics()->getLastUpdate() == 0) {
            continue;
        }

        float power = 0.0f;
        bool hasPower = false;
        for (auto channel : inverter->Statistics()->getChannelsByType(TYPE_AC)) {
            if (!inverter->Statistics()->hasChannelFieldValue(TYPE_AC, channel, FLD_PAC)) {
                continue;
            }
            power += inverter->Statistics()->getChannelFieldValue(TYPE_AC, channel, FLD_PAC);
            hasPower = true;
        }

        if (hasPower) {
            const auto rounded = static_cast<int32_t>(std::lround(power));
            point.InverterPower[i] = static_cast<int16_t>(std::clamp<int32_t>(rounded, -32767, 32767));
        }
    }

    _powerHistoryWriteIndex = (_powerHistoryWriteIndex + 1) % POWER_HISTORY_MAX_POINTS;
    _powerHistoryCount = std::min<uint16_t>(_powerHistoryCount + 1, POWER_HISTORY_MAX_POINTS);
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

    // Keep the payload below roughly 360 points, independent of the platform's sample interval.
    const uint16_t samplesInRange = hours * 60U / POWER_HISTORY_SAMPLE_MINUTES;
    const uint8_t bucketSize = std::max<uint8_t>(1, (samplesInRange + 359) / 360);
    const uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    const uint32_t cutoff = now > hours * 60U * 60U ? now - hours * 60U * 60U : 0;

    std::lock_guard<std::mutex> lock(_powerHistoryMutex);
    auto* response = request->beginResponseStream("application/json", 32 * 1024);
    response->addHeader("Cache-Control", "no-store");
    response->printf("{\"sample_interval\":%u,\"hours\":%u,\"inverters\":[", POWER_HISTORY_SAMPLE_MINUTES * 60U, hours);

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
    int64_t inverterSums[INV_MAX_COUNT] = {};
    uint8_t inverterCounts[INV_MAX_COUNT] = {};
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
        response->printf("[%" PRIu32 ",", bucketTimestamp);
        if (gridCount == 0) {
            response->print("null");
        } else {
            response->print(static_cast<int32_t>(std::lround(static_cast<double>(gridSum) / gridCount)));
        }
        for (uint8_t inverterIndex = 0; inverterIndex < _powerHistoryInverterCount; ++inverterIndex) {
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

    const uint16_t oldestIndex = (_powerHistoryWriteIndex + POWER_HISTORY_MAX_POINTS - _powerHistoryCount) % POWER_HISTORY_MAX_POINTS;
    for (uint16_t offset = 0; offset < _powerHistoryCount; ++offset) {
        const auto& point = _powerHistory[(oldestIndex + offset) % POWER_HISTORY_MAX_POINTS];
        if (point.Timestamp < cutoff) {
            continue;
        }

        bucketTimestamp = point.Timestamp;
        ++bucketSamples;
        if (point.GridPower != POWER_HISTORY_INVALID_GRID) {
            gridSum += point.GridPower;
            ++gridCount;
        }
        for (uint8_t inverterIndex = 0; inverterIndex < _powerHistoryInverterCount; ++inverterIndex) {
            if (point.InverterPower[inverterIndex] == POWER_HISTORY_INVALID_INVERTER) {
                continue;
            }
            inverterSums[inverterIndex] += point.InverterPower[inverterIndex];
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
