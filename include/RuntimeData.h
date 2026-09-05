// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <mutex>
#include <atomic>
#include <cstdint>


class RuntimeClass {
public:
    static constexpr uint8_t DAILY_YIELD_MAX_DAYS = 30;

    struct DailyYieldRecord {
        uint32_t Day = 0; // local date as YYYYMMDD
        uint32_t YieldWh = 0;
        bool IsToday = false;
    };

    RuntimeClass() = default;
    ~RuntimeClass() = default;
    RuntimeClass(const RuntimeClass&) = delete;
    RuntimeClass& operator=(const RuntimeClass&) = delete;
    RuntimeClass(RuntimeClass&&) = delete;
    RuntimeClass& operator=(RuntimeClass&&) = delete;

    void init(Scheduler& scheduler);
    enum class ReadMode : uint8_t { START_UP, ON_DEMAND };
    bool read(ReadMode const mode = ReadMode::START_UP);                // read runtime data
    bool write(uint16_t const freezeMinutes = 10);                      // do not write if last write operation was less than freezeMinutes ago
    void requestWriteOnNextTaskLoop(void) { _writeNow.store(true); };   // use this member function to store data on demand
    void requestReadOnNextTaskLoop(void) { _readNow.store(true); };     // use this member function to read data on demand

    uint16_t getWriteCount(void) const;
    time_t getWriteEpochTime(void) const;
    bool getReadState(void) const { return _readOK.load(); }
    bool getWriteState(void) const { return _writeOK.load(); }
    String getWriteCountAndTimeString(void) const;
    uint8_t getDailyYieldHistory(DailyYieldRecord* records, uint8_t maxRecords, bool includeCurrent = true) const;
    bool clearDailyYieldHistory();

private:
    void loop(void);
    bool getWriteTrigger(void);
    bool updateDailyYield(void);
    void appendDailyYieldLocked(uint32_t day, uint32_t yieldWh, uint8_t retentionDays);
    void pruneDailyYieldLocked(uint8_t retentionDays);

    Task _loopTask;
    std::atomic<bool> _readOK = false;      // true if the last read operation was successful
    std::atomic<bool> _writeOK = false;     // true if the last write operation was successful
    std::atomic<bool> _readNow = false;     // if true, the data is read in the next task loop()
    std::atomic<bool> _writeNow = false;    // if true, the data is stored in the next task loop()
    mutable std::mutex _mutex;              // to protect the shared data below
    bool _lastTrigger = false;              // auxiliary value to prevent multiple triggering on the same day
    uint16_t _fileVersion = 0;              // shared data: version of the runtime data file, prepared for future migration support
    uint16_t _writeCount = 0;               // shared data: number of write operations
    time_t _writeEpoch = 0;                 // shared data: epoch time when the data was written
    uint32_t _dailyYieldCurrentDay = 0;
    uint32_t _dailyYieldCurrentWh = 0;
    DailyYieldRecord _dailyYieldHistory[DAILY_YIELD_MAX_DAYS] = {};
    uint8_t _dailyYieldCount = 0;
};

extern RuntimeClass RuntimeData;
