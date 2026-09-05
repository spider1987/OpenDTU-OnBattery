// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2024-2026 Thomas Basler and others
 */
#include "RestartHelper.h"
#include "Display_Graphic.h"
#include "Led_Single.h"
#include <Arduino.h>
#include <Esp.h>
#include <Preferences.h>
#include <algorithm>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_timer.h>

#undef TAG
static const char* TAG = "restart";

namespace {

constexpr uint32_t RESTART_DATA_MAGIC = 0x52535444; // RSTD
constexpr uint16_t RESTART_DATA_VERSION = 1;
constexpr uint32_t RTC_SNAPSHOT_MAGIC = 0x52544344; // RTCD
constexpr time_t MINIMUM_VALID_EPOCH = 1609459200; // 2021-01-01
constexpr char NVS_NAMESPACE[] = "restartdiag";
constexpr char NVS_STATE_KEY[] = "state";

struct RestartSnapshot {
    uint32_t BootNumber = 0;
    int64_t Timestamp = 0;
    uint32_t Uptime = 0;
    uint32_t FreeHeap = 0;
    uint32_t MinimumFreeHeap = 0;
    uint32_t LargestFreeHeapBlock = 0;
    RestartReason Reason = RestartReason::Unknown;
    bool Valid = false;
};

struct PersistentRestartData {
    uint32_t Magic = RESTART_DATA_MAGIC;
    uint16_t Version = RESTART_DATA_VERSION;
    uint8_t RecordCount = 0;
    uint8_t Reserved = 0;
    uint32_t BootNumber = 0;
    RestartSnapshot Pending = {};
    RestartRecord Records[RestartHelperClass::RESTART_HISTORY_SIZE] = {};
};

struct RtcRestartSnapshot {
    uint32_t Magic;
    uint32_t BootNumber;
    int64_t Timestamp;
    uint32_t Uptime;
    uint32_t FreeHeap;
    uint32_t MinimumFreeHeap;
    uint32_t LargestFreeHeapBlock;
    uint32_t Checksum;
};

RTC_NOINIT_ATTR RtcRestartSnapshot rtcSnapshot;

int64_t getCurrentTimestamp()
{
    const time_t now = time(nullptr);
    return now >= MINIMUM_VALID_EPOCH ? static_cast<int64_t>(now) : 0;
}

uint32_t getUptimeSeconds()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
}

uint32_t calculateRtcChecksum(const RtcRestartSnapshot& snapshot)
{
    return snapshot.Magic
        ^ snapshot.BootNumber
        ^ static_cast<uint32_t>(snapshot.Timestamp)
        ^ static_cast<uint32_t>(snapshot.Timestamp >> 32)
        ^ snapshot.Uptime
        ^ snapshot.FreeHeap
        ^ snapshot.MinimumFreeHeap
        ^ snapshot.LargestFreeHeapBlock
        ^ 0xA5C31F27;
}

bool isRtcSnapshotValid(uint32_t bootNumber)
{
    return rtcSnapshot.Magic == RTC_SNAPSHOT_MAGIC
        && rtcSnapshot.BootNumber == bootNumber
        && rtcSnapshot.Checksum == calculateRtcChecksum(rtcSnapshot);
}

RestartSnapshot captureCurrentSnapshot(uint32_t bootNumber, RestartReason reason)
{
    RestartSnapshot snapshot;
    snapshot.BootNumber = bootNumber;
    snapshot.Timestamp = getCurrentTimestamp();
    snapshot.Uptime = getUptimeSeconds();
    snapshot.FreeHeap = ESP.getFreeHeap();
    snapshot.MinimumFreeHeap = ESP.getMinFreeHeap();
    snapshot.LargestFreeHeapBlock = ESP.getMaxAllocHeap();
    snapshot.Reason = reason;
    snapshot.Valid = true;
    return snapshot;
}

bool loadPersistentData(PersistentRestartData& data)
{
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, true)) {
        ESP_LOGE(TAG, "Failed to open restart diagnostics in NVS");
        return false;
    }

    const size_t storedSize = preferences.getBytesLength(NVS_STATE_KEY);
    const size_t readSize = storedSize == sizeof(data)
        ? preferences.getBytes(NVS_STATE_KEY, &data, sizeof(data))
        : 0;
    preferences.end();

    if (readSize != sizeof(data)
        || data.Magic != RESTART_DATA_MAGIC
        || data.Version != RESTART_DATA_VERSION
        || data.RecordCount > RestartHelperClass::RESTART_HISTORY_SIZE) {
        data = {};
        return false;
    }
    return true;
}

bool savePersistentData(const PersistentRestartData& data)
{
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        ESP_LOGE(TAG, "Failed to open restart diagnostics in NVS");
        return false;
    }

    const size_t written = preferences.putBytes(NVS_STATE_KEY, &data, sizeof(data));
    preferences.end();
    if (written != sizeof(data)) {
        ESP_LOGE(TAG, "Failed to persist restart diagnostics");
        return false;
    }
    return true;
}

} // namespace

RestartHelperClass RestartHelper;

RestartHelperClass::RestartHelperClass()
    : _rebootTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&RestartHelperClass::loop, this))
    , _bootCaptureTask(10 * TASK_SECOND, TASK_ONCE, std::bind(&RestartHelperClass::captureBootRecord, this))
    , _snapshotTask(1 * TASK_MINUTE, TASK_FOREVER, std::bind(&RestartHelperClass::snapshotLoop, this))
{
}

void RestartHelperClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_rebootTask);
    scheduler.addTask(_bootCaptureTask);
    scheduler.addTask(_snapshotTask);
    // Reading and writing NVS during the power-supply ramp can make marginal
    // classic ESP32 boards fail their cold start. Capture the boot only after
    // the system has been running on a stable supply for a few seconds.
    _bootCaptureTask.enable();
    _snapshotTask.enable();
}

void RestartHelperClass::triggerRestart(RestartReason reason)
{
    if (_bootNumber == 0) {
        _bootCaptureTask.disable();
        captureBootRecord();
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_restartPending) {
            ESP_LOGW(TAG, "Restart already pending, keeping original request reason");
            return;
        }
        _restartPending = true;
    }

    persistRestartRequest(reason);
    _rebootTask.enable();
    _rebootTask.restart();
}

void RestartHelperClass::loop()
{
    if (_rebootTask.isFirstIteration()) {
        LedSingle.turnAllOff();
        Display.setStatus(false);
    } else {
        ESP.restart();
    }
}

void RestartHelperClass::snapshotLoop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    const RestartSnapshot snapshot = captureCurrentSnapshot(_bootNumber, RestartReason::Unknown);
    rtcSnapshot.Magic = RTC_SNAPSHOT_MAGIC;
    rtcSnapshot.BootNumber = snapshot.BootNumber;
    rtcSnapshot.Timestamp = snapshot.Timestamp;
    rtcSnapshot.Uptime = snapshot.Uptime;
    rtcSnapshot.FreeHeap = snapshot.FreeHeap;
    rtcSnapshot.MinimumFreeHeap = snapshot.MinimumFreeHeap;
    rtcSnapshot.LargestFreeHeapBlock = snapshot.LargestFreeHeapBlock;
    rtcSnapshot.Checksum = calculateRtcChecksum(rtcSnapshot);
}

void RestartHelperClass::captureBootRecord()
{
    PersistentRestartData data;
    loadPersistentData(data);

    const uint32_t previousBootNumber = data.BootNumber;
    const esp_reset_reason_t espReason = esp_reset_reason();
    if (espReason == ESP_RST_POWERON) {
        // RTC_NOINIT is deliberately retained across software resets, but is
        // undefined after a real power cycle.
        rtcSnapshot = {};
    }
    const bool requestedRestart = data.Pending.Valid
        && data.Pending.BootNumber == previousBootNumber
        && espReason == ESP_RST_SW;
    const bool rtcDataValid = isRtcSnapshotValid(previousBootNumber);

    RestartRecord record;
    record.BootNumber = previousBootNumber + 1;
    record.EspResetReason = static_cast<uint8_t>(espReason);
    record.RequestedReason = requestedRestart ? data.Pending.Reason : RestartReason::Unknown;

    if (requestedRestart) {
        record.Timestamp = data.Pending.Timestamp;
        record.PreviousUptime = data.Pending.Uptime;
        record.FreeHeap = data.Pending.FreeHeap;
        record.MinimumFreeHeap = data.Pending.MinimumFreeHeap;
        record.LargestFreeHeapBlock = data.Pending.LargestFreeHeapBlock;
        record.MetricsValid = true;
    } else if (rtcDataValid) {
        record.Timestamp = rtcSnapshot.Timestamp;
        record.PreviousUptime = rtcSnapshot.Uptime;
        record.FreeHeap = rtcSnapshot.FreeHeap;
        record.MinimumFreeHeap = rtcSnapshot.MinimumFreeHeap;
        record.LargestFreeHeapBlock = rtcSnapshot.LargestFreeHeapBlock;
        record.MetricsValid = true;
    }

    const uint8_t copyCount = std::min<uint8_t>(
        data.RecordCount, RESTART_HISTORY_SIZE - 1);
    for (uint8_t i = copyCount; i > 0; --i) {
        data.Records[i] = data.Records[i - 1];
    }
    data.Records[0] = record;
    data.RecordCount = copyCount + 1;
    data.BootNumber = record.BootNumber;
    data.Pending = {};
    savePersistentData(data);

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _bootNumber = data.BootNumber;
        _historyCount = data.RecordCount;
        std::copy_n(data.Records, _historyCount, _history.begin());
    }

    snapshotLoop();

    ESP_LOGI(TAG, "========== LAST RESET ==========");
    ESP_LOGI(TAG, "Reset Reason: %s (%s)",
        getEspResetReasonDescription(record.EspResetReason),
        getEspResetReasonName(record.EspResetReason));
    ESP_LOGI(TAG, "Restart Request: %s", getRestartReasonName(record.RequestedReason));
    if (record.MetricsValid) {
        ESP_LOGI(TAG, "Previous Uptime: %" PRIu32 " s", record.PreviousUptime);
        ESP_LOGI(TAG, "Free Heap before restart: %" PRIu32, record.FreeHeap);
        ESP_LOGI(TAG, "Minimum Free Heap: %" PRIu32, record.MinimumFreeHeap);
        ESP_LOGI(TAG, "Largest Free Heap Block: %" PRIu32, record.LargestFreeHeapBlock);
    } else {
        ESP_LOGI(TAG, "Previous runtime metrics: unavailable");
    }
    ESP_LOGI(TAG, "================================");
}

void RestartHelperClass::persistRestartRequest(RestartReason reason)
{
    PersistentRestartData data;
    loadPersistentData(data);
    data.Pending = captureCurrentSnapshot(_bootNumber, reason);
    savePersistentData(data);
    snapshotLoop();

    ESP_LOGW(TAG, "Restart requested: %s", getRestartReasonName(reason));
}

RestartRecord RestartHelperClass::getLastRestart() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _historyCount > 0 ? _history[0] : RestartRecord {};
}

uint8_t RestartHelperClass::getRestartHistory(RestartRecord* records, uint8_t maxRecords) const
{
    if (records == nullptr || maxRecords == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    const uint8_t count = std::min(_historyCount, maxRecords);
    std::copy_n(_history.begin(), count, records);
    return count;
}

bool RestartHelperClass::clearHistory()
{
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        ESP_LOGE(TAG, "Failed to open restart diagnostics in NVS");
        return false;
    }

    const bool success = preferences.clear();
    preferences.end();
    if (!success) {
        ESP_LOGE(TAG, "Failed to clear restart diagnostics");
        return false;
    }

    _bootCaptureTask.disable();
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _history.fill({});
        _historyCount = 0;
        _bootNumber = 0;
    }
    rtcSnapshot = {};
    ESP_LOGI(TAG, "Restart diagnostics cleared");
    return true;
}

const char* RestartHelperClass::getEspResetReasonName(uint8_t reason)
{
    switch (static_cast<esp_reset_reason_t>(reason)) {
    case ESP_RST_POWERON: return "ESP_RST_POWERON";
    case ESP_RST_EXT: return "ESP_RST_EXT";
    case ESP_RST_SW: return "ESP_RST_SW";
    case ESP_RST_PANIC: return "ESP_RST_PANIC";
    case ESP_RST_INT_WDT: return "ESP_RST_INT_WDT";
    case ESP_RST_TASK_WDT: return "ESP_RST_TASK_WDT";
    case ESP_RST_WDT: return "ESP_RST_WDT";
    case ESP_RST_DEEPSLEEP: return "ESP_RST_DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "ESP_RST_BROWNOUT";
    case ESP_RST_SDIO: return "ESP_RST_SDIO";
    case ESP_RST_UNKNOWN:
    default: return "ESP_RST_UNKNOWN";
    }
}

const char* RestartHelperClass::getEspResetReasonDescription(uint8_t reason)
{
    switch (static_cast<esp_reset_reason_t>(reason)) {
    case ESP_RST_POWERON: return "Power-on reset";
    case ESP_RST_EXT: return "External reset";
    case ESP_RST_SW: return "Software reset";
    case ESP_RST_PANIC: return "Panic or exception";
    case ESP_RST_INT_WDT: return "Interrupt watchdog";
    case ESP_RST_TASK_WDT: return "Task watchdog";
    case ESP_RST_WDT: return "Other watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep-sleep wake-up";
    case ESP_RST_BROWNOUT: return "Brownout";
    case ESP_RST_SDIO: return "SDIO reset";
    case ESP_RST_UNKNOWN:
    default: return "Unknown reset cause";
    }
}

const char* RestartHelperClass::getRestartReasonName(RestartReason reason)
{
    switch (reason) {
    case RestartReason::UserRequest: return "USER_REQUEST";
    case RestartReason::ConfigChange: return "CONFIG_CHANGE";
    case RestartReason::ConfigRestore: return "CONFIG_RESTORE";
    case RestartReason::FactoryReset: return "FACTORY_RESET";
    case RestartReason::OtaUpdate: return "OTA_UPDATE";
    case RestartReason::InverterUnresponsive: return "INVERTER_UNRESPONSIVE";
    case RestartReason::SystemRecovery: return "SYSTEM_RECOVERY";
    case RestartReason::Unknown:
    default: return "UNKNOWN";
    }
}
