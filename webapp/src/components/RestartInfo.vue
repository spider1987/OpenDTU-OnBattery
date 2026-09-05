<template>
    <div class="restart-info">
        <CardElement :text="$t('restartinfo.LastRestart')" textVariant="text-bg-primary">
            <template v-if="lastRestart">
                <div class="restart-summary-grid">
                    <div class="restart-value">
                        <span>{{ $t('restartinfo.Cause') }}</span>
                        <strong :class="resetSeverityClass(lastRestart.reset_reason)">
                            {{ resetReasonText(lastRestart.reset_reason) }}
                        </strong>
                        <small>{{ lastRestart.reset_reason }}</small>
                    </div>
                    <div class="restart-value">
                        <span>{{ $t('restartinfo.RequestedReason') }}</span>
                        <strong>{{ requestedReasonText(lastRestart.requested_reason) }}</strong>
                        <small>{{ lastRestart.requested_reason }}</small>
                    </div>
                    <div class="restart-value">
                        <span>{{ $t('restartinfo.PreviousUptime') }}</span>
                        <strong>{{ formatUptime(lastRestart) }}</strong>
                        <small>{{ formatTimestamp(lastRestart.timestamp) }}</small>
                    </div>
                    <div class="restart-value">
                        <span>{{ $t('restartinfo.FreeHeap') }}</span>
                        <strong>{{ formatHeap(lastRestart.free_heap, lastRestart.metrics_valid) }}</strong>
                        <small>{{ $t('restartinfo.BeforeRestart') }}</small>
                    </div>
                    <div class="restart-value">
                        <span>{{ $t('restartinfo.MinimumFreeHeap') }}</span>
                        <strong>{{ formatHeap(lastRestart.minimum_free_heap, lastRestart.metrics_valid) }}</strong>
                        <small>{{ $t('restartinfo.PreviousBoot') }}</small>
                    </div>
                    <div class="restart-value">
                        <span>{{ $t('restartinfo.LargestFreeBlock') }}</span>
                        <strong>{{
                            formatHeap(lastRestart.largest_free_heap_block, lastRestart.metrics_valid)
                        }}</strong>
                        <small>{{ $t('restartinfo.BeforeRestart') }}</small>
                    </div>
                </div>

                <details v-if="systemStatus.restart_history?.length" class="restart-history">
                    <summary>{{ $t('restartinfo.History', { count: systemStatus.restart_history.length }) }}</summary>
                    <div class="table-responsive">
                        <table class="table table-hover align-middle mb-0">
                            <thead>
                                <tr>
                                    <th>{{ $t('restartinfo.BootNumber') }}</th>
                                    <th>{{ $t('restartinfo.Time') }}</th>
                                    <th>{{ $t('restartinfo.Cause') }}</th>
                                    <th>{{ $t('restartinfo.RequestedReason') }}</th>
                                    <th>{{ $t('restartinfo.PreviousUptime') }}</th>
                                    <th>{{ $t('restartinfo.MinimumFreeHeap') }}</th>
                                </tr>
                            </thead>
                            <tbody>
                                <tr v-for="record in systemStatus.restart_history" :key="record.boot_number">
                                    <td :data-label="$t('restartinfo.BootNumber')">#{{ record.boot_number }}</td>
                                    <td :data-label="$t('restartinfo.Time')">
                                        {{ formatTimestamp(record.timestamp) }}
                                    </td>
                                    <td :data-label="$t('restartinfo.Cause')">
                                        <span :class="resetSeverityClass(record.reset_reason)">
                                            {{ resetReasonText(record.reset_reason) }}
                                        </span>
                                        <small class="d-block text-body-secondary">{{ record.reset_reason }}</small>
                                    </td>
                                    <td :data-label="$t('restartinfo.RequestedReason')">
                                        {{ requestedReasonText(record.requested_reason) }}
                                    </td>
                                    <td :data-label="$t('restartinfo.PreviousUptime')">{{ formatUptime(record) }}</td>
                                    <td :data-label="$t('restartinfo.MinimumFreeHeap')">
                                        {{ formatHeap(record.minimum_free_heap, record.metrics_valid) }}
                                    </td>
                                </tr>
                            </tbody>
                        </table>
                    </div>
                    <div class="restart-history-actions">
                        <button type="button" class="btn btn-outline-danger btn-sm" @click="clearHistoryModal.show()">
                            {{ $t('restartinfo.ClearHistory') }}
                        </button>
                    </div>
                </details>
            </template>
            <div v-else class="text-body-secondary">{{ $t('restartinfo.NoHistory') }}</div>
        </CardElement>

        <ModalDialog
            modalId="restartHistoryClear"
            small
            :title="$t('restartinfo.ClearHistory')"
            :closeText="$t('restartinfo.Cancel')"
        >
            {{ $t('restartinfo.ClearHistoryQuestion') }}
            <template #footer>
                <button type="button" class="btn btn-danger" :disabled="clearing" @click="clearHistory">
                    <span v-if="clearing" class="spinner-border spinner-border-sm me-2" aria-hidden="true"></span>
                    {{ $t('restartinfo.ClearHistory') }}
                </button>
            </template>
        </ModalDialog>
    </div>
</template>

<script lang="ts">
import CardElement from '@/components/CardElement.vue';
import ModalDialog from '@/components/ModalDialog.vue';
import type { RestartRecord, SystemStatus } from '@/types/SystemStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { timestampToString } from '@/utils';
import * as bootstrap from 'bootstrap';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    components: {
        CardElement,
        ModalDialog,
    },
    emits: ['cleared'],
    data() {
        return {
            clearHistoryModal: {} as bootstrap.Modal,
            clearing: false,
        };
    },
    mounted() {
        this.clearHistoryModal = new bootstrap.Modal('#restartHistoryClear');
    },
    props: {
        systemStatus: { type: Object as PropType<SystemStatus>, required: true },
    },
    computed: {
        lastRestart(): RestartRecord | undefined {
            return this.systemStatus.last_restart;
        },
    },
    methods: {
        resetReasonText(reason: string): string {
            return this.$t(`restartinfo.ResetReasons.${reason}`).toString();
        },
        requestedReasonText(reason: string): string {
            return this.$t(`restartinfo.RequestReasons.${reason}`).toString();
        },
        formatTimestamp(timestamp: number): string {
            if (!timestamp) return this.$t('restartinfo.Unknown').toString();
            return new Date(timestamp * 1000).toLocaleString(this.$i18n.locale, {
                dateStyle: 'short',
                timeStyle: 'medium',
            });
        },
        formatUptime(record: RestartRecord): string {
            if (!record.metrics_valid) return this.$t('restartinfo.Unknown').toString();
            const [count, time] = timestampToString(this.$i18n.locale, record.previous_uptime, true);
            return this.$t('firmwareinfo.UptimeValue', { count, time }).toString();
        },
        formatHeap(value: number, valid: boolean): string {
            if (!valid) return this.$t('restartinfo.Unknown').toString();
            return this.$n(Math.round(value / 1024), 'kilobyte');
        },
        resetSeverityClass(reason: string): string {
            if (['ESP_RST_PANIC', 'ESP_RST_INT_WDT', 'ESP_RST_TASK_WDT', 'ESP_RST_WDT'].includes(reason)) {
                return 'text-danger';
            }
            if (reason === 'ESP_RST_BROWNOUT' || reason === 'ESP_RST_UNKNOWN') {
                return 'text-warning';
            }
            return 'text-body-emphasis';
        },
        clearHistory() {
            this.clearing = true;
            fetch('/api/system/restart_history/clear', {
                method: 'POST',
                headers: authHeader(),
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(() => {
                    this.clearHistoryModal.hide();
                    this.$emit('cleared');
                })
                .finally(() => {
                    this.clearing = false;
                });
        },
    },
});
</script>

<style scoped>
.restart-info {
    min-width: 0;
}

.restart-summary-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 0.75rem;
}

.restart-value {
    min-width: 0;
    display: grid;
    gap: 0.15rem;
    padding: 0.85rem;
    border: 1px solid var(--bs-border-color-translucent);
    border-radius: 0.65rem;
    background: rgba(var(--bs-body-color-rgb), 0.025);
}

.restart-value > span {
    color: var(--bs-secondary-color);
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.045em;
    text-transform: uppercase;
}

.restart-value strong {
    overflow: hidden;
    text-overflow: ellipsis;
}

.restart-value small {
    overflow: hidden;
    color: var(--bs-secondary-color);
    text-overflow: ellipsis;
}

.restart-history {
    margin-top: 1rem;
    overflow: hidden;
    border: 1px solid var(--bs-border-color-translucent);
    border-radius: 0.65rem;
}

.restart-history > summary {
    padding: 0.85rem 1rem;
    cursor: pointer;
    font-weight: 700;
    user-select: none;
}

.restart-history[open] > summary {
    border-bottom: 1px solid var(--bs-border-color-translucent);
}

.restart-history-actions {
    display: flex;
    justify-content: flex-end;
    padding: 0.75rem 1rem;
    border-top: 1px solid var(--bs-border-color-translucent);
}

@media (max-width: 991.98px) {
    .restart-summary-grid {
        grid-template-columns: repeat(2, minmax(0, 1fr));
    }
}

@media (max-width: 767.98px) {
    .restart-summary-grid {
        grid-template-columns: 1fr;
    }

    .restart-history .table-responsive {
        overflow: visible;
    }

    .restart-history table,
    .restart-history tbody,
    .restart-history tr,
    .restart-history td {
        display: block;
        width: 100%;
    }

    .restart-history thead {
        display: none;
    }

    .restart-history tr {
        padding: 0.5rem 0.8rem;
        border-bottom: 1px solid var(--bs-border-color-translucent);
    }

    .restart-history tr:last-child {
        border-bottom: 0;
    }

    .restart-history td {
        display: grid;
        grid-template-columns: minmax(7.5rem, 42%) minmax(0, 1fr);
        gap: 0.65rem;
        padding: 0.4rem 0;
        border: 0;
        text-align: right;
        overflow-wrap: anywhere;
    }

    .restart-history td::before {
        content: attr(data-label);
        color: var(--bs-secondary-color);
        font-weight: 600;
        text-align: left;
    }
}
</style>
