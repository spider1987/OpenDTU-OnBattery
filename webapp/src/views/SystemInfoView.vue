<template>
    <BasePage :title="$t('systeminfo.SystemInfo')" :isLoading="dataLoading" :show-reload="true" @reload="getSystemInfo">
        <div class="system-info-page">
            <section class="system-summary-grid" :aria-label="$t('systeminfo.Overview')">
                <article class="system-summary-card">
                    <span class="system-summary-label">{{ $t('firmwareinfo.Hostname') }}</span>
                    <strong>{{ systemDataList.hostname }}</strong>
                    <small>{{ systemDataList.chipmodel }}</small>
                </article>
                <article class="system-summary-card">
                    <span class="system-summary-label">{{ $t('firmwareinfo.Uptime') }}</span>
                    <strong>{{ $t('firmwareinfo.UptimeValue', timeInHours(systemDataList.uptime)) }}</strong>
                    <small>{{ systemDataList.resetreason_0 }}</small>
                </article>
                <article class="system-summary-card">
                    <span class="system-summary-label">{{ $t('systeminfo.FreeHeap') }}</span>
                    <strong>{{ $n(Math.round(freeHeap / 1024), 'kilobyte') }}</strong>
                    <small>{{ $t('systeminfo.HeapUsed', { value: $n(heapUsage, 'percent') }) }}</small>
                </article>
                <article class="system-summary-card">
                    <span class="system-summary-label">{{ $t('hardwareinfo.CpuTemperature') }}</span>
                    <strong>{{ $n(systemDataList.cputemp, 'celsius') }}</strong>
                    <small>{{ systemDataList.cpufreq }} {{ $t('hardwareinfo.Mhz') }}</small>
                </article>
            </section>

            <div class="system-card-grid">
                <FirmwareInfo class="system-card-wide firmware-card" :systemStatus="systemDataList" />
                <HardwareInfo :systemStatus="systemDataList" />
                <MemoryInfo :systemStatus="systemDataList" />
            </div>

            <details class="system-details">
                <summary>{{ $t('systeminfo.TechnicalDetails') }}</summary>
                <div class="system-card-grid system-detail-grid">
                    <MemoryDetails
                        :title="$t('memorydetails.HeapDetails')"
                        :total="systemDataList.heap_total"
                        :used="systemDataList.heap_used"
                        :min-free="systemDataList.heap_min_free"
                        :max-block="systemDataList.heap_max_block"
                    />
                    <template v-if="systemDataList.psram_total > 0">
                        <MemoryDetails
                            :title="$t('memorydetails.PsramDetails')"
                            :total="systemDataList.psram_total"
                            :used="systemDataList.psram_used"
                            :min-free="systemDataList.psram_min_free"
                            :max-block="systemDataList.psram_max_block"
                        />
                    </template>
                    <TaskDetails class="system-card-wide" :taskDetails="systemDataList.task_details" />
                    <RadioInfo :systemStatus="systemDataList" />
                    <UartAllocations :allocations="systemDataList.uarts" />
                </div>
            </details>
        </div>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import FirmwareInfo from '@/components/FirmwareInfo.vue';
import HardwareInfo from '@/components/HardwareInfo.vue';
import MemoryInfo from '@/components/MemoryInfo.vue';
import MemoryDetails from '@/components/MemoryDetails.vue';
import TaskDetails from '@/components/TaskDetails.vue';
import RadioInfo from '@/components/RadioInfo.vue';
import UartAllocations from '@/components/UartAllocations.vue';
import type { SystemStatus } from '@/types/SystemStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { timestampToString } from '@/utils';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        FirmwareInfo,
        HardwareInfo,
        MemoryInfo,
        MemoryDetails,
        TaskDetails,
        RadioInfo,
        UartAllocations,
    },
    data() {
        return {
            dataLoading: true,
            systemDataList: {} as SystemStatus,
        };
    },
    created() {
        this.getSystemInfo();
    },
    methods: {
        getSystemInfo() {
            this.dataLoading = true;
            fetch('/api/system/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.systemDataList = data;
                })
                .finally(() => {
                    this.dataLoading = false;
                });
        },
    },
    computed: {
        freeHeap(): number {
            return Math.max(0, this.systemDataList.heap_total - this.systemDataList.heap_used);
        },
        heapUsage(): number {
            return this.systemDataList.heap_total > 0
                ? this.systemDataList.heap_used / this.systemDataList.heap_total
                : 0;
        },
        timeInHours() {
            return (value: number) => {
                const [count, time] = timestampToString(this.$i18n.locale, value, true);
                return { count, time };
            };
        },
    },
});
</script>

<style scoped>
.system-info-page {
    display: grid;
    gap: 1.25rem;
}

.system-summary-grid {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 0.85rem;
}

.system-summary-card {
    min-width: 0;
    display: grid;
    gap: 0.22rem;
    padding: 1rem 1.1rem;
    border: 1px solid var(--bs-border-color);
    border-top: 3px solid var(--bs-primary);
    border-radius: 0.8rem;
    background: var(--bs-tertiary-bg);
}

.system-summary-card strong {
    overflow: hidden;
    color: var(--bs-emphasis-color);
    font-size: clamp(1.05rem, 1.8vw, 1.35rem);
    font-weight: 700;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.system-summary-card small,
.system-summary-label {
    overflow: hidden;
    color: var(--bs-secondary-color);
    text-overflow: ellipsis;
    white-space: nowrap;
}

.system-summary-label {
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.055em;
    text-transform: uppercase;
}

.system-card-grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 1rem;
    align-items: start;
}

.system-card-wide {
    grid-column: 1 / -1;
}

.system-details {
    overflow: hidden;
    border: 1px solid var(--bs-border-color);
    border-radius: 0.8rem;
    background: var(--bs-tertiary-bg);
}

.system-details > summary {
    padding: 1rem 1.15rem;
    color: var(--bs-emphasis-color);
    cursor: pointer;
    font-weight: 700;
    user-select: none;
}

.system-details[open] > summary {
    border-bottom: 1px solid var(--bs-border-color-translucent);
}

.system-detail-grid {
    padding: 1rem;
}

.system-info-page :deep(.card) {
    border-color: var(--bs-border-color);
    border-radius: 0.8rem;
    background: var(--bs-tertiary-bg);
}

.system-info-page :deep(.card-header.text-bg-primary) {
    color: var(--bs-primary) !important;
    border-bottom-color: var(--bs-border-color-translucent);
    background: rgba(var(--bs-primary-rgb), 0.1) !important;
    font-weight: 700;
}

.system-info-page :deep(.table > :not(caption) > * > *) {
    padding-top: 0.7rem;
    padding-bottom: 0.7rem;
}

.system-info-page :deep(.table th) {
    color: var(--bs-secondary-color);
    font-weight: 600;
}

@media (min-width: 768px) {
    .system-info-page :deep(.firmware-card tbody) {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }

    .system-info-page :deep(.firmware-card tr) {
        display: grid;
        grid-template-columns: minmax(0, 0.9fr) minmax(0, 1.1fr);
        border-bottom: 1px solid var(--bs-border-color-translucent);
    }

    .system-info-page :deep(.firmware-card tr:nth-last-child(-n + 2)) {
        border-bottom: 0;
    }

    .system-info-page :deep(.firmware-card th),
    .system-info-page :deep(.firmware-card td) {
        min-width: 0;
        border: 0;
    }
}

@media (max-width: 991.98px) {
    .system-summary-grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@media (max-width: 767.98px) {
    .system-card-grid,
    .system-summary-grid {
        grid-template-columns: 1fr;
    }

    .system-card-wide {
        grid-column: auto;
    }
}
</style>
