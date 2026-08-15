export interface PowerHistoryInverterConfig {
    serial: string;
    name: string;
    enabled: boolean;
}

export interface PowerHistoryConfig {
    enabled: boolean;
    power_meter_enabled: boolean;
    power_meter_available: boolean;
    inverter_total_enabled: boolean;
    interval_minutes: number;
    inverters: PowerHistoryInverterConfig[];
}
