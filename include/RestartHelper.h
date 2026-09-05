// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <array>
#include <cstdint>
#include <mutex>

enum class RestartReason : uint8_t {
    Unknown = 0,
    UserRequest,
    ConfigChange,
    ConfigRestore,
    FactoryReset,
    OtaUpdate,
    InverterUnresponsive,
    SystemRecovery,
};

struct RestartRecord {
    uint32_t BootNumber = 0;
    int64_t Timestamp = 0;
    uint32_t PreviousUptime = 0;
    uint32_t FreeHeap = 0;
    uint32_t MinimumFreeHeap = 0;
    uint32_t LargestFreeHeapBlock = 0;
    uint8_t EspResetReason = 0;
    RestartReason RequestedReason = RestartReason::Unknown;
    bool MetricsValid = false;
};

class RestartHelperClass {
public:
    static constexpr uint8_t RESTART_HISTORY_SIZE = 10;

    RestartHelperClass();
    void init(Scheduler& scheduler);
    void triggerRestart(RestartReason reason);

    RestartRecord getLastRestart() const;
    uint8_t getRestartHistory(RestartRecord* records, uint8_t maxRecords) const;
    bool clearHistory();

    static const char* getEspResetReasonName(uint8_t reason);
    static const char* getEspResetReasonDescription(uint8_t reason);
    static const char* getRestartReasonName(RestartReason reason);

private:
    void loop();
    void snapshotLoop();
    void captureBootRecord();
    void persistRestartRequest(RestartReason reason);

    Task _rebootTask;
    Task _bootCaptureTask;
    Task _snapshotTask;
    mutable std::mutex _mutex;
    std::array<RestartRecord, RESTART_HISTORY_SIZE> _history = {};
    uint8_t _historyCount = 0;
    uint32_t _bootNumber = 0;
    bool _restartPending = false;
};

extern RestartHelperClass RestartHelper;
