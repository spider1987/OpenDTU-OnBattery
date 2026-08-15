// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Hoymiles.h>
#include <TaskSchedulerDeclarations.h>
#include <memory>

class WebApiWsLiveClass {
public:
    WebApiWsLiveClass();
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void reload();

private:
    static void generateInverterCommonJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv);
    static void generateInverterChannelJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv);
    static void generateCommonJsonResponse(JsonVariant& root);

    void generateOnBatteryJsonResponse(JsonVariant& root, bool all);
    void sendOnBatteryStats();

    static void addField(JsonObject& root, std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, String topic = "");
    static void addTotalField(JsonObject& root, const String& name, const float value, const String& unit, const uint8_t digits);

    void onLivedataStatus(AsyncWebServerRequest* request);
    void onPowerHistoryStatus(AsyncWebServerRequest* request);
    void onPowerHistoryConfigGet(AsyncWebServerRequest* request);
    void onPowerHistoryConfigPost(AsyncWebServerRequest* request);
    void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

    AsyncWebSocket _ws;
    AsyncAuthenticationMiddleware _simpleDigestAuth;

    uint32_t _lastPublishOnBatteryFull = 0;
    uint32_t _lastPublishSolarCharger = 0;
    uint32_t _lastPublishGridCharger = 0;
    uint32_t _lastPublishBattery = 0;
    uint32_t _lastPublishPowerMeter = 0;

    uint32_t _lastPublishStats[INV_MAX_COUNT] = { 0 };

    std::mutex _mutex;

    static constexpr int16_t POWER_HISTORY_INVALID_INVERTER = INT16_MIN;
    static constexpr int32_t POWER_HISTORY_INVALID_GRID = INT32_MIN;

    std::unique_ptr<uint32_t[]> _powerHistoryTimestamps;
    std::unique_ptr<int32_t[]> _powerHistoryGridPower;
    std::unique_ptr<int16_t[]> _powerHistoryInverterPower;
    uint64_t _powerHistorySerials[INV_MAX_COUNT] = {};
    uint16_t _powerHistoryWriteIndex = 0;
    uint16_t _powerHistoryCount = 0;
    uint16_t _powerHistoryCapacity = 0;
    uint8_t _powerHistoryInverterCount = 0;
    uint8_t _powerHistorySeriesCount = 0;
    uint8_t _powerHistoryIntervalMinutes = 5;
    bool _powerHistoryEnabled = false;
    bool _powerHistoryPowerMeterEnabled = false;
    bool _powerHistoryInverterTotalEnabled = false;
    std::mutex _powerHistoryMutex;

    void configurePowerHistory();

    Task _wsCleanupTask;
    void wsCleanupTaskCb();

    Task _sendDataTask;
    void sendDataTaskCb();

    Task _powerHistoryTask;
    void powerHistoryTaskCb();
};
