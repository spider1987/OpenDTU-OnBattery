export interface TaskDetail {
    name: string;
    stack_watermark: number;
    priority: number;
}

export interface UartAllocation {
    port: number;
    owner: string;
}

export interface RestartRecord {
    boot_number: number;
    timestamp: number;
    previous_uptime: number;
    free_heap: number;
    minimum_free_heap: number;
    largest_free_heap_block: number;
    metrics_valid: boolean;
    reset_reason: string;
    reset_description: string;
    requested_reason: string;
}

export interface SystemStatus {
    // HardwareInfo
    chipmodel: string;
    chiprevision: number;
    chipcores: number;
    cpufreq: number;
    cputemp: number;
    flashsize: number;
    // TaskDetails
    task_details: TaskDetail[];
    // FirmwareInfo
    hostname: string;
    sdkversion: string;
    config_version: string;
    config_version_onbattery: string;
    firmware_version: string;
    git_hash: string;
    git_branch: string;
    pioenv: string;
    resetreason_0: string;
    resetreason_1: string;
    last_restart?: RestartRecord;
    restart_history: RestartRecord[];
    cfgsavecount: number;
    runtime_savecount: string;
    uptime: number;
    // MemoryInfo
    heap_total: number;
    heap_used: number;
    heap_max_block: number;
    heap_min_free: number;
    littlefs_total: number;
    littlefs_used: number;
    psram_total: number;
    psram_used: number;
    psram_max_block: number;
    psram_min_free: number;
    sketch_total: number;
    sketch_used: number;
    // RadioInfo
    nrf_configured: boolean;
    nrf_connected: boolean;
    nrf_pvariant: boolean;
    cmt_configured: boolean;
    cmt_connected: boolean;
    // UARTs
    uarts: UartAllocation[];
}
