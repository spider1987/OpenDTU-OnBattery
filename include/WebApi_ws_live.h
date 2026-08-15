// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Hoymiles.h>
#include <TaskSchedulerDeclarations.h>

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

    // The classic ESP32 has a considerably smaller contiguous DRAM region than
    // ESP32-S3. Three-minute samples retain 24 h while keeping enough runtime heap.
#if CONFIG_IDF_TARGET_ESP32
    static constexpr uint8_t POWER_HISTORY_SAMPLE_MINUTES = 3;
#else
    static constexpr uint8_t POWER_HISTORY_SAMPLE_MINUTES = 1;
#endif
    static constexpr uint16_t POWER_HISTORY_MAX_POINTS = 24 * 60 / POWER_HISTORY_SAMPLE_MINUTES;
    static constexpr int16_t POWER_HISTORY_INVALID_INVERTER = INT16_MIN;
    static constexpr int32_t POWER_HISTORY_INVALID_GRID = INT32_MIN;

    struct PowerHistoryPoint {
        uint32_t Timestamp = 0;
        int32_t GridPower = POWER_HISTORY_INVALID_GRID;
        int16_t InverterPower[INV_MAX_COUNT] = {};
    };

    PowerHistoryPoint _powerHistory[POWER_HISTORY_MAX_POINTS];
    uint64_t _powerHistorySerials[INV_MAX_COUNT] = {};
    uint16_t _powerHistoryWriteIndex = 0;
    uint16_t _powerHistoryCount = 0;
    uint8_t _powerHistoryInverterCount = 0;
    std::mutex _powerHistoryMutex;

    Task _wsCleanupTask;
    void wsCleanupTaskCb();

    Task _sendDataTask;
    void sendDataTaskCb();

    Task _powerHistoryTask;
    void powerHistoryTaskCb();
};
