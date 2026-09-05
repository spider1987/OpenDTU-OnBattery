<template>
    <BasePage :title="$t('powerhistoryadmin.Settings')" :isLoading="dataLoading">
        <BootstrapAlert
            v-model="alert.show"
            dismissible
            :variant="alert.type"
            :auto-dismiss="alert.type !== 'success' ? 0 : 5000"
        >
            {{ alert.message }}
        </BootstrapAlert>

        <form @submit.prevent="saveConfig">
            <CardElement :text="$t('powerhistoryadmin.Recording')" textVariant="text-bg-primary">
                <div class="form-check form-switch mb-3">
                    <input class="form-check-input" type="checkbox" id="historyEnabled" v-model="config.enabled" />
                    <label class="form-check-label" for="historyEnabled">{{ $t('powerhistoryadmin.Enabled') }}</label>
                </div>

                <div class="row mb-3">
                    <label for="historyInterval" class="col-sm-4 col-form-label">
                        {{ $t('powerhistoryadmin.Interval') }}
                    </label>
                    <div class="col-sm-8">
                        <select
                            id="historyInterval"
                            class="form-select"
                            v-model.number="config.interval_minutes"
                            :disabled="!config.enabled"
                        >
                            <option v-for="minutes in intervals" :key="minutes" :value="minutes">
                                {{ $t('powerhistoryadmin.EveryMinutes', { minutes }) }}
                            </option>
                        </select>
                        <div class="form-text">{{ $t('powerhistoryadmin.IntervalHint') }}</div>
                    </div>
                </div>
            </CardElement>

            <CardElement :text="$t('powerhistoryadmin.DailyYield')" textVariant="text-bg-primary">
                <div class="form-check form-switch mb-3">
                    <input
                        class="form-check-input"
                        type="checkbox"
                        id="dailyYieldEnabled"
                        v-model="config.daily_yield_enabled"
                    />
                    <label class="form-check-label" for="dailyYieldEnabled">
                        {{ $t('powerhistoryadmin.DailyYieldEnabled') }}
                    </label>
                </div>

                <div class="row mb-2">
                    <label for="dailyYieldDays" class="col-sm-4 col-form-label">
                        {{ $t('powerhistoryadmin.Retention') }}
                    </label>
                    <div class="col-sm-8">
                        <select
                            id="dailyYieldDays"
                            class="form-select"
                            v-model.number="config.daily_yield_days"
                            :disabled="!config.daily_yield_enabled"
                        >
                            <option v-for="days in retentionOptions" :key="days" :value="days">
                                {{ $t('powerhistoryadmin.Days', { days }) }}
                            </option>
                        </select>
                        <div class="form-text">{{ $t('powerhistoryadmin.DailyYieldHint') }}</div>
                    </div>
                </div>

                <div class="daily-yield-actions mt-3">
                    <button type="button" class="btn btn-primary" :disabled="savingConfig" @click="saveConfig">
                        <span v-if="savingConfig" class="spinner-border spinner-border-sm me-2" aria-hidden="true"></span>
                        {{ $t('powerhistoryadmin.ApplyDailyYield') }}
                    </button>
                    <button type="button" class="btn btn-outline-danger" @click="showClearDailyYieldModal">
                        {{ $t('powerhistoryadmin.ClearDailyYield') }}
                    </button>
                </div>
            </CardElement>

            <CardElement :text="$t('powerhistoryadmin.Sources')" textVariant="text-bg-primary">
                <div class="form-check form-switch mb-3">
                    <input
                        class="form-check-input"
                        type="checkbox"
                        id="historyPowerMeter"
                        v-model="config.power_meter_enabled"
                        :disabled="!config.enabled || !config.power_meter_available"
                    />
                    <label class="form-check-label" for="historyPowerMeter">{{
                        $t('powerhistoryadmin.PowerMeter')
                    }}</label>
                    <div v-if="!config.power_meter_available" class="form-text">
                        {{ $t('powerhistoryadmin.PowerMeterUnavailable') }}
                    </div>
                </div>

                <div class="form-check form-switch mb-3">
                    <input
                        class="form-check-input"
                        type="checkbox"
                        id="historyInverterTotal"
                        v-model="config.inverter_total_enabled"
                        :disabled="!config.enabled"
                    />
                    <label class="form-check-label" for="historyInverterTotal">
                        {{ $t('powerhistoryadmin.InverterTotal') }}
                    </label>
                </div>

                <div class="form-check form-switch mb-3">
                    <input
                        class="form-check-input"
                        type="checkbox"
                        id="historyVoltage"
                        v-model="config.voltage_enabled"
                        :disabled="!config.enabled || !config.voltage_source_available"
                    />
                    <label class="form-check-label" for="historyVoltage">
                        {{ $t('powerhistoryadmin.Voltage') }}
                    </label>
                    <div class="form-text">
                        {{
                            config.voltage_source_available
                                ? $t('powerhistoryadmin.VoltageSource', { name: config.voltage_source_name })
                                : $t('powerhistoryadmin.VoltageUnavailable')
                        }}
                    </div>
                </div>

                <h3 class="h6 mt-4 mb-2">{{ $t('powerhistoryadmin.Inverters') }}</h3>
                <div class="list-group">
                    <label
                        v-for="inverter in config.inverters"
                        :key="inverter.serial"
                        class="list-group-item d-flex align-items-center gap-3"
                    >
                        <input
                            class="form-check-input flex-shrink-0"
                            type="checkbox"
                            v-model="inverter.enabled"
                            :disabled="!config.enabled"
                        />
                        <span>
                            <strong>{{ inverter.name }}</strong>
                            <small class="d-block text-body-secondary">{{ inverter.serial }}</small>
                        </span>
                    </label>
                </div>
                <div v-if="config.inverters.length === 0" class="form-text">
                    {{ $t('powerhistoryadmin.NoInverters') }}
                </div>
                <div v-if="config.enabled && !hasSelectedSource" class="alert alert-warning mt-3 mb-0">
                    {{ $t('powerhistoryadmin.SelectSource') }}
                </div>
            </CardElement>

            <div class="alert alert-info mt-3">{{ memoryHint }}</div>
            <FormFooter @reload="getConfig" />
        </form>

        <ModalDialog
            modalId="clearDailyYield"
            small
            :title="$t('powerhistoryadmin.ClearDailyYield')"
            :closeText="$t('powerhistoryadmin.Cancel')"
        >
            {{ $t('powerhistoryadmin.ClearDailyYieldQuestion') }}
            <template #footer>
                <button type="button" class="btn btn-danger" :disabled="clearingDailyYield" @click="clearDailyYield">
                    <span
                        v-if="clearingDailyYield"
                        class="spinner-border spinner-border-sm me-2"
                        aria-hidden="true"
                    ></span>
                    {{ $t('powerhistoryadmin.ClearDailyYield') }}
                </button>
            </template>
        </ModalDialog>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import ModalDialog from '@/components/ModalDialog.vue';
import type { AlertResponse } from '@/types/AlertResponse';
import type { PowerHistoryConfig } from '@/types/PowerHistoryConfig';
import { authHeader, handleResponse } from '@/utils/authentication';
import * as bootstrap from 'bootstrap';
import { defineComponent } from 'vue';

export default defineComponent({
    components: { BasePage, BootstrapAlert, CardElement, FormFooter, ModalDialog },
    data() {
        return {
            dataLoading: true,
            alert: {} as AlertResponse,
            intervals: [1, 3, 5, 10, 15, 30, 60],
            retentionOptions: [7, 14, 30],
            savingConfig: false,
            clearingDailyYield: false,
            config: {
                enabled: false,
                power_meter_enabled: false,
                power_meter_available: false,
                inverter_total_enabled: false,
                voltage_enabled: false,
                voltage_source_available: false,
                voltage_source_name: '',
                interval_minutes: 5,
                daily_yield_enabled: false,
                daily_yield_days: 30,
                inverters: [],
            } as PowerHistoryConfig,
        };
    },
    computed: {
        hasSelectedSource(): boolean {
            return (
                (this.config.power_meter_available && this.config.power_meter_enabled) ||
                this.config.inverter_total_enabled ||
                (this.config.voltage_source_available && this.config.voltage_enabled) ||
                this.config.inverters.some((inverter) => inverter.enabled)
            );
        },
        memoryHint(): string {
            const points = Math.floor((24 * 60) / this.config.interval_minutes);
            const inverterCount =
                this.config.inverters.filter((inverter) => inverter.enabled).length +
                (this.config.inverter_total_enabled ? 1 : 0);
            const bytes =
                points *
                (4 +
                    inverterCount * 2 +
                    (this.config.power_meter_available && this.config.power_meter_enabled ? 4 : 0) +
                    (this.config.voltage_source_available && this.config.voltage_enabled ? 2 : 0));
            return this.$t('powerhistoryadmin.MemoryHint', {
                points,
                memory: Math.max(1, Math.ceil(bytes / 1024)),
            });
        },
    },
    created() {
        this.getConfig();
    },
    methods: {
        async getConfig() {
            this.dataLoading = true;
            try {
                const response = await fetch('/api/livedata/power-history/config', {
                    headers: authHeader(),
                    cache: 'no-store',
                });
                const data = (await handleResponse(response, this.$emitter, this.$router)) as PowerHistoryConfig;
                this.config = data;
                if (!this.config.power_meter_available) {
                    this.config.power_meter_enabled = false;
                }
                if (!this.config.voltage_source_available) {
                    this.config.voltage_enabled = false;
                }
            } finally {
                this.dataLoading = false;
            }
        },
        async saveConfig() {
            if (this.config.enabled && !this.hasSelectedSource) {
                this.alert = {
                    show: true,
                    type: 'warning',
                    message: this.$t('powerhistoryadmin.SelectSource'),
                } as AlertResponse;
                return;
            }
            this.savingConfig = true;
            const payload = {
                enabled: this.config.enabled,
                power_meter_enabled: this.config.power_meter_available && this.config.power_meter_enabled,
                inverter_total_enabled: this.config.inverter_total_enabled,
                voltage_enabled: this.config.voltage_source_available && this.config.voltage_enabled,
                interval_minutes: this.config.interval_minutes,
                daily_yield_enabled: this.config.daily_yield_enabled,
                daily_yield_days: this.config.daily_yield_days,
                inverters: this.config.inverters.map(({ serial, enabled }) => ({ serial, enabled })),
            };
            const formData = new FormData();
            formData.append('data', JSON.stringify(payload));
            try {
                const response = await fetch('/api/livedata/power-history/config', {
                    method: 'POST',
                    headers: authHeader(),
                    body: formData,
                });
                const data = await handleResponse(response, this.$emitter, this.$router);
                this.alert = data;
                this.alert.message = this.$t('apiresponse.' + data.code, data.param);
                this.alert.show = true;
                await this.getConfig();
            } finally {
                this.savingConfig = false;
            }
        },
        showClearDailyYieldModal() {
            const element = document.getElementById('clearDailyYield');
            if (element) {
                bootstrap.Modal.getOrCreateInstance(element).show();
            }
        },
        async clearDailyYield() {
            this.clearingDailyYield = true;
            try {
                const response = await fetch('/api/livedata/daily-yield/clear', {
                    method: 'POST',
                    headers: authHeader(),
                });
                const data = await handleResponse(response, this.$emitter, this.$router);
                this.alert = {
                    show: true,
                    type: data.type,
                    message: this.$t(
                        data.success
                            ? 'powerhistoryadmin.ClearDailyYieldSuccess'
                            : 'powerhistoryadmin.ClearDailyYieldFailed',
                    ),
                } as AlertResponse;
                const element = document.getElementById('clearDailyYield');
                if (element) {
                    bootstrap.Modal.getOrCreateInstance(element).hide();
                }
            } finally {
                this.clearingDailyYield = false;
            }
        },
    },
});
</script>

<style scoped>
.daily-yield-actions {
    display: grid;
    gap: 0.65rem;
}

@media (min-width: 576px) {
    .daily-yield-actions {
        display: flex;
        justify-content: flex-end;
    }
}
</style>
