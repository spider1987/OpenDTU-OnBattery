// SPDX-License-Identifier: GPL-2.0-or-later

/* Runtime Data Management
 *
 * Read and write runtime data persistent on LittleFS
 * - The data is stored in JSON format
 * - The data is written during WebApp 'OTA firmware upgrade' and during Webapp 'Reboot'
 * - For security reasons such as 'unexpected power cycles' or 'physical resets', data is also written once a day at 00:05
 * - The data will not be written if the last write operation was less than one hour ago. ('OTA firmware upgrade' and 'Reboot')
 * - Threadsave access to the data is provided by a mutex.
 *
 * How to use:
 *  - Runtime data must be added in the read() and write() methods.
 *  - To avoid reenter deadlocks, do not call write() or read() from a locally locked mutex to save locally data on demand!
 *  - Use requestWriteOnNextTaskLoop() and requestReadOnNextTaskLoop() to avoid deadlocks if you want to handle locally data on demand.
 *
 * 2025.09.11 - 1.0 - first version
 * 2025.12.01 - 1.1 - added read mode ON_DEMAND and START_UP
 */

#include <Utils.h>
#include <LittleFS.h>
#include <esp_log.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include "Configuration.h"
#include "Datastore.h"
#include "RuntimeData.h"


#undef TAG
static const char* TAG = "runtime";


constexpr const char* RUNTIME_FILENAME = "/runtime.json";   // filename of the runtime data file
constexpr uint16_t RUNTIME_VERSION = 2;                     // version prepared for future migration support


RuntimeClass RuntimeData; // singleton instance


/*
 * Init the runtime data loop task
 */
void RuntimeClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&RuntimeClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(60 * 1000); // every minute
    _loopTask.enable();
}


/*
 * The runtime data loop is called every minute
 */
void RuntimeClass::loop(void)
{
    const bool dailyYieldChanged = updateDailyYield();

    // check if we need to write the runtime data, either it is 00:05 or on request
    if (dailyYieldChanged || _writeNow.exchange(false) || getWriteTrigger()) {
        write(0); // no freeze time.
    }

    // check if we need to read the runtime data on request
    // for example, if some data is not available during startup
    if (_readNow.exchange(false)) {
        read(ReadMode::ON_DEMAND); // read data that can be read on demand
    }
}


/*
 * Writes the runtime data to LittleFS file
 * freezeMinutes: Minimum necessary time [minutes] between now and last write operation
 */
bool RuntimeClass::write(uint16_t const freezeMinutes)
{
    auto cleanExit = [this](const bool writeOk, const char* text) -> bool {
        if (writeOk) {
            ESP_LOGI(TAG,"%s", text);
        } else {
            ESP_LOGE(TAG,"%s", text);
        }
        _writeOK.store(writeOk);
        return writeOk;
    };

    // we need a valid epoch time before we can write the runtime data
    time_t nextEpoch;
    if (!Utils::getEpoch(&nextEpoch, 1)) { return cleanExit(false, "Local time not available, skipping write"); }
    uint16_t nextCount;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        // check minimum interval between writes (enforced only when freezeMinutes > 0)
        if ((freezeMinutes > 0) && (_writeEpoch != 0) && (difftime(nextEpoch, _writeEpoch) < 60 * freezeMinutes)) {
            return cleanExit(false, "Time interval too short, skipping write");
        }

        // prepare the next write count
        nextCount = _writeCount + 1;

    } // mutex is automatically released when lock goes out of this scope

    // prepare the JSON document and store the runtime data in it is done outside the
    // mutex protection to minimize the time the mutex is locked.
    JsonDocument doc;
    JsonObject info = doc["info"].to<JsonObject>();
    info["version"] = RUNTIME_VERSION;
    info["save_count"] = nextCount;
    info["save_epoch"] = nextEpoch;

    uint32_t dailyYieldCurrentDay;
    uint32_t dailyYieldCurrentWh;
    uint8_t dailyYieldCount;
    DailyYieldRecord dailyYieldHistory[DAILY_YIELD_MAX_DAYS];
    {
        std::lock_guard<std::mutex> lock(_mutex);
        dailyYieldCurrentDay = _dailyYieldCurrentDay;
        dailyYieldCurrentWh = _dailyYieldCurrentWh;
        dailyYieldCount = _dailyYieldCount;
        std::copy_n(_dailyYieldHistory, dailyYieldCount, dailyYieldHistory);
    }

    JsonObject dailyYield = doc["daily_yield"].to<JsonObject>();
    dailyYield["current_day"] = dailyYieldCurrentDay;
    dailyYield["current_wh"] = dailyYieldCurrentWh;
    JsonArray dailyYieldRecords = dailyYield["records"].to<JsonArray>();
    for (uint8_t i = 0; i < dailyYieldCount; ++i) {
        JsonArray record = dailyYieldRecords.add<JsonArray>();
        record.add(dailyYieldHistory[i].Day);
        record.add(dailyYieldHistory[i].YieldWh);
    }

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return cleanExit(false, "JSON alloc fault, skipping write");
    }

    File fRuntime = LittleFS.open(RUNTIME_FILENAME, "w");
    if (!fRuntime) { return cleanExit(false, "Failed to open file for writing"); }

    if (serializeJson(doc, fRuntime) == 0) {
        fRuntime.close();
        return cleanExit(false, "Failed to serialize to file");
    }

    fRuntime.close();

    {
        std::lock_guard<std::mutex> lock(_mutex);

        // commit the new state only after a successful write
        _fileVersion = RUNTIME_VERSION;
        _writeEpoch = nextEpoch;
        _writeCount = nextCount;

    } // mutex is automatically released when lock goes out of this scope

    return cleanExit(true, "Written to file");
}


/*
 * Read the runtime data from LittleFS file
 * mode = START_UP: read data that can be initialized during startup
 * mode = ON_DEMAND: read data that can not be read during startup
 */
bool RuntimeClass::read(ReadMode const mode)
{
    bool readOk = false;
    JsonDocument doc;

    // Note: We do not exit on read or allocation errors. In that case we need the default values
    File fRuntime = LittleFS.open(RUNTIME_FILENAME, "r", false);
    if (fRuntime) {
        Utils::skipBom(fRuntime);
        DeserializationError error = deserializeJson(doc, fRuntime);
        if (!error && Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
            readOk = true; // success of reading the runtime data
        }
    }

    JsonObject info = doc["info"];
    { // mutex is automatically released when lock goes out of this scope
        std::lock_guard<std::mutex> lock(_mutex);
        _fileVersion = info["version"] | 0U; // 0 means no file available and runtime data is not valid
        _writeCount = info["save_count"] | 0U;
        _writeEpoch = info["save_epoch"] | 0U;
    } // mutex is automatically released when lock goes out of this scope

    if (mode == ReadMode::START_UP) {
        JsonObject dailyYield = doc["daily_yield"];
        std::lock_guard<std::mutex> lock(_mutex);
        _dailyYieldCurrentDay = dailyYield["current_day"] | 0U;
        _dailyYieldCurrentWh = dailyYield["current_wh"] | 0U;
        _dailyYieldCount = 0;
        for (JsonArray record : dailyYield["records"].as<JsonArray>()) {
            if (_dailyYieldCount >= DAILY_YIELD_MAX_DAYS || record.size() < 2) {
                break;
            }
            const uint32_t day = record[0] | 0U;
            if (day == 0) {
                continue;
            }
            _dailyYieldHistory[_dailyYieldCount].Day = day;
            _dailyYieldHistory[_dailyYieldCount].YieldWh = record[1] | 0U;
            _dailyYieldHistory[_dailyYieldCount].IsToday = false;
            ++_dailyYieldCount;
        }
        pruneDailyYieldLocked(Configuration.get().PowerHistory.DailyYieldDays);
    }


    if (fRuntime) { fRuntime.close(); }
    if (readOk) {
        ESP_LOGI(TAG, "Read successfully");
    } else {
        ESP_LOGE(TAG, "Read fault, using default values");
    }
    _readOK.store(readOk);
    return readOk;
}


/*
 * Get the write counter
 */
uint16_t RuntimeClass::getWriteCount(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _writeCount;
}


/*
 * Get the write epoch time
 */
time_t RuntimeClass::getWriteEpochTime(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _writeEpoch;
}


/*
 * Get the write count and time as string
 * Format: "<count> / <dd>-<mon> <hh>:<mm>"
 * If epoch time and local time is not available the time is replaced by "no time"
 */
String RuntimeClass::getWriteCountAndTimeString(void) const
{
    time_t epoch;
    uint16_t count;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        epoch = _writeEpoch;
        count = _writeCount;
    } // mutex is automatically released when lock goes out of this scope

    char buf[32] = "";
    struct tm time;

    // Before we can convert the epoch to local time, we need to ensure we've received the correct time
    // from the time server. This may take some time after the system startup.
    if ((epoch != 0) && (getLocalTime(&time, 1))) {
        localtime_r(&epoch, &time);
        strftime(buf, sizeof(buf), " / %d-%h %R", &time);
    } else {
        snprintf(buf, sizeof(buf), " / no time");
    }
    String ctString = String(count) + String(buf);
    return ctString;
}

uint8_t RuntimeClass::getDailyYieldHistory(DailyYieldRecord* records, uint8_t maxRecords, bool includeCurrent) const
{
    if (records == nullptr || maxRecords == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    uint8_t count = 0;
    if (includeCurrent && _dailyYieldCurrentDay != 0) {
        records[count++] = { _dailyYieldCurrentDay, _dailyYieldCurrentWh, true };
    }
    for (int16_t i = static_cast<int16_t>(_dailyYieldCount) - 1; i >= 0 && count < maxRecords; --i) {
        records[count++] = _dailyYieldHistory[i];
        records[count - 1].IsToday = false;
    }
    return count;
}

bool RuntimeClass::updateDailyYield(void)
{
    struct tm nowTime;
    if (!getLocalTime(&nowTime, 1)) {
        return false;
    }

    const uint32_t day = static_cast<uint32_t>(nowTime.tm_year + 1900) * 10000U
        + static_cast<uint32_t>(nowTime.tm_mon + 1) * 100U
        + static_cast<uint32_t>(nowTime.tm_mday);
    const uint32_t yieldWh = static_cast<uint32_t>(std::max<int64_t>(
        0, static_cast<int64_t>(std::llround(Datastore.getTotalAcYieldDayEnabled()))));
    const auto& config = Configuration.get().PowerHistory;
    const uint8_t retentionDays = config.DailyYieldDays <= 7 ? 7 : (config.DailyYieldDays <= 14 ? 14 : 30);

    std::lock_guard<std::mutex> lock(_mutex);
    pruneDailyYieldLocked(retentionDays);

    if (_dailyYieldCurrentDay == 0) {
        _dailyYieldCurrentDay = day;
        _dailyYieldCurrentWh = yieldWh;
        return false;
    }

    if (_dailyYieldCurrentDay == day) {
        _dailyYieldCurrentWh = std::max(_dailyYieldCurrentWh, yieldWh);
        return false;
    }

    if (config.DailyYieldEnabled) {
        appendDailyYieldLocked(_dailyYieldCurrentDay, _dailyYieldCurrentWh, retentionDays);
    }
    _dailyYieldCurrentDay = day;
    _dailyYieldCurrentWh = yieldWh;
    return config.DailyYieldEnabled;
}

void RuntimeClass::appendDailyYieldLocked(uint32_t day, uint32_t yieldWh, uint8_t retentionDays)
{
    if (_dailyYieldCount > 0 && _dailyYieldHistory[_dailyYieldCount - 1].Day == day) {
        _dailyYieldHistory[_dailyYieldCount - 1].YieldWh = std::max(
            _dailyYieldHistory[_dailyYieldCount - 1].YieldWh, yieldWh);
        return;
    }

    if (_dailyYieldCount >= DAILY_YIELD_MAX_DAYS) {
        std::move(_dailyYieldHistory + 1, _dailyYieldHistory + _dailyYieldCount, _dailyYieldHistory);
        --_dailyYieldCount;
    }
    _dailyYieldHistory[_dailyYieldCount++] = { day, yieldWh, false };
    pruneDailyYieldLocked(retentionDays);
}

void RuntimeClass::pruneDailyYieldLocked(uint8_t retentionDays)
{
    const uint8_t keep = retentionDays <= 7 ? 7 : (retentionDays <= 14 ? 14 : DAILY_YIELD_MAX_DAYS);
    if (_dailyYieldCount <= keep) {
        return;
    }
    const uint8_t removeCount = _dailyYieldCount - keep;
    std::move(_dailyYieldHistory + removeCount, _dailyYieldHistory + _dailyYieldCount, _dailyYieldHistory);
    _dailyYieldCount = keep;
}


/*
 * Returns true once a day between 00:05 - 00:10
 */
bool RuntimeClass::getWriteTrigger(void) {

    struct tm nowTime;
    if (!getLocalTime(&nowTime, 1)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if ((nowTime.tm_hour == 0) && (nowTime.tm_min >= 5) && (nowTime.tm_min <= 10)) {
        if (_lastTrigger == false) {
            _lastTrigger = true;
            const time_t nowEpoch = time(nullptr);
            if (_writeEpoch != 0 && difftime(nowEpoch, _writeEpoch) < 10 * 60) {
                return false;
            }
            return true;
        }
    } else {
        _lastTrigger = false;
    }
    return false;
}
