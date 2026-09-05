// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi_sysstatus.h"
#include "Configuration.h"
#include "NetworkSettings.h"
#include "PinMapping.h"
#include "RestartHelper.h"
#include "RuntimeData.h"
#include "SerialPortManager.h"
#include "WebApi.h"
#include "__compiled_constants.h"
#include <AsyncJson.h>
#include <CpuTemperature.h>
#include <Hoymiles.h>
#include <LittleFS.h>
#include <ResetReason.h>

namespace {

void addRestartRecord(JsonObject target, const RestartRecord& record)
{
    target["boot_number"] = record.BootNumber;
    target["timestamp"] = record.Timestamp;
    target["previous_uptime"] = record.PreviousUptime;
    target["free_heap"] = record.FreeHeap;
    target["minimum_free_heap"] = record.MinimumFreeHeap;
    target["largest_free_heap_block"] = record.LargestFreeHeapBlock;
    target["metrics_valid"] = record.MetricsValid;
    target["reset_reason"] = RestartHelperClass::getEspResetReasonName(record.EspResetReason);
    target["reset_description"] = RestartHelperClass::getEspResetReasonDescription(record.EspResetReason);
    target["requested_reason"] = RestartHelperClass::getRestartReasonName(record.RequestedReason);
}

} // namespace

void WebApiSysstatusClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/system/status", HTTP_GET, static_cast<ArRequestHandlerFunction>(std::bind(&WebApiSysstatusClass::onSystemStatus, this, _1)));
    server.on("/api/system/restart_history/clear", HTTP_POST,
        static_cast<ArRequestHandlerFunction>(std::bind(&WebApiSysstatusClass::onRestartHistoryClear, this, _1)));
}

void WebApiSysstatusClass::onRestartHistoryClear(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const bool success = RestartHelper.clearHistory();
    root["type"] = success ? "success" : "danger";
    root["message"] = success ? "Restart history cleared" : "Failed to clear restart history";
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiSysstatusClass::onSystemStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["hostname"] = NetworkSettings.getHostname();

    root["sdkversion"] = ESP.getSdkVersion();
    root["cpufreq"] = ESP.getCpuFreqMHz();
    root["cputemp"] = CpuTemperature.read();

    root["heap_total"] = ESP.getHeapSize();
    root["heap_used"] = ESP.getHeapSize() - ESP.getFreeHeap();
    root["heap_max_block"] = ESP.getMaxAllocHeap();
    root["heap_min_free"] = ESP.getMinFreeHeap();

    root["psram_total"] = ESP.getPsramSize();
    root["psram_used"] = ESP.getPsramSize() - ESP.getFreePsram();
    root["psram_max_block"] = ESP.getMaxAllocPsram();
    root["psram_min_free"] = ESP.getMinFreePsram();

    root["sketch_total"] = ESP.getFreeSketchSpace();
    root["sketch_used"] = ESP.getSketchSize();
    root["littlefs_total"] = LittleFS.totalBytes();
    root["littlefs_used"] = LittleFS.usedBytes();

    root["chiprevision"] = ESP.getChipRevision();
    root["chipmodel"] = ESP.getChipModel();
    root["chipcores"] = ESP.getChipCores();
    root["flashsize"] = ESP.getFlashChipSize();

    JsonArray taskDetails = root["task_details"].to<JsonArray>();
    static std::array<char const*, 15> constexpr task_names = {
        "IDLE0", "IDLE1", "wifi", "tiT", "loopTask", "async_tcp", "mqttclient",
        "HuaweiHwIfc", "HuaweiTwai", "HuaweiMCP2515",
        "TruckiPolling",
        "PM:SDM", "PM:HTTP+JSON", "PM:SML", "PM:HTTP+SML",
    };
    for (char const* task_name : task_names) {
        TaskHandle_t const handle = xTaskGetHandle(task_name);
        if (!handle) {
            continue;
        }
        JsonObject task = taskDetails.add<JsonObject>();
        task["name"] = task_name;
        task["stack_watermark"] = uxTaskGetStackHighWaterMark(handle);
        task["priority"] = uxTaskPriorityGet(handle);
    }

    String reason;
    reason = ResetReason::get_reset_reason_verbose(0);
    root["resetreason_0"] = reason;

    reason = ResetReason::get_reset_reason_verbose(1);
    root["resetreason_1"] = reason;

    RestartRecord restartHistory[RestartHelperClass::RESTART_HISTORY_SIZE];
    const uint8_t restartHistoryCount = RestartHelper.getRestartHistory(
        restartHistory, RestartHelperClass::RESTART_HISTORY_SIZE);
    if (restartHistoryCount > 0) {
        addRestartRecord(root["last_restart"].to<JsonObject>(), restartHistory[0]);
    }
    JsonArray restartHistoryJson = root["restart_history"].to<JsonArray>();
    for (uint8_t i = 0; i < restartHistoryCount; ++i) {
        addRestartRecord(restartHistoryJson.add<JsonObject>(), restartHistory[i]);
    }

    root["cfgsavecount"] = Configuration.get().Cfg.SaveCount;
    root["runtime_savecount"] = RuntimeData.getWriteCountAndTimeString();

    char version[16];
    snprintf(version, sizeof(version), "%d.%d.%d", CONFIG_VERSION >> 24 & 0xff, CONFIG_VERSION >> 16 & 0xff, CONFIG_VERSION >> 8 & 0xff);
    root["config_version"] = version;
    root["config_version_onbattery"] = CONFIG_VERSION_ONBATTERY;
    root["firmware_version"] = __COMPILED_FIRMWARE_VERSION__;
    root["git_hash"] = __COMPILED_GIT_HASH__;
    root["git_branch"] = __COMPILED_GIT_BRANCH__;
    root["pioenv"] = PIOENV;

    root["uptime"] = esp_timer_get_time() / 1000000;

    root["nrf_configured"] = PinMapping.isValidNrf24Config();
    root["nrf_connected"] = Hoymiles.getRadioNrf()->isConnected();
    root["nrf_pvariant"] = Hoymiles.getRadioNrf()->isPVariant();

    root["cmt_configured"] = PinMapping.isValidCmt2300Config();
    root["cmt_connected"] = Hoymiles.getRadioCmt()->isConnected();

    JsonArray uarts = root["uarts"].to<JsonArray>();
    for (auto const& allocation : SerialPortManager.getAllocations()) {
        JsonObject uart = uarts.add<JsonObject>();
        uart["port"] = allocation.first;
        uart["owner"] = allocation.second;
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
