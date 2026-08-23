<template>
    <section class="card daily-yield-card mt-3">
        <details @toggle="onToggle">
            <summary class="daily-yield-summary">
                <span>
                    <strong>{{ $t('home.DailyYieldHistory') }}</strong>
                    <small class="d-block text-body-secondary">
                        {{ $t('home.DailyYieldHistoryHint', { days: retentionDays }) }}
                    </small>
                </span>
                <span class="summary-chevron" aria-hidden="true"></span>
            </summary>

            <div class="daily-yield-body">
                <div v-if="loading" class="daily-yield-state">
                    <span class="spinner-border spinner-border-sm" aria-hidden="true"></span>
                    {{ $t('base.Loading') }}
                </div>
                <div v-else-if="records.length === 0" class="daily-yield-state text-body-secondary">
                    {{ $t('home.NoDailyYieldData') }}
                </div>
                <div v-else class="daily-yield-list">
                    <div v-for="record in records" :key="record.day" class="daily-yield-row">
                        <span class="daily-yield-date" :class="{ 'is-today': record.today }">
                            {{ record.today ? $t('home.Today') : formatDate(record.day) }}
                        </span>
                        <span class="daily-yield-track" aria-hidden="true">
                            <span
                                class="daily-yield-bar"
                                :class="{ 'is-today': record.today }"
                                :style="{ width: barWidth(record.yield_wh) }"
                            ></span>
                        </span>
                        <strong class="daily-yield-value">{{ formatEnergy(record.yield_wh) }}</strong>
                    </div>
                </div>
            </div>
        </details>
    </section>
</template>

<script lang="ts">
import { authHeader } from '@/utils/authentication';
import { defineComponent } from 'vue';

interface DailyYieldRecord {
    day: number;
    yield_wh: number;
    today: boolean;
}

interface DailyYieldResponse {
    enabled: boolean;
    retention_days: number;
    records: DailyYieldRecord[];
}

export default defineComponent({
    data() {
        return {
            loading: false,
            retentionDays: 30,
            records: [] as DailyYieldRecord[],
        };
    },
    computed: {
        maxYield(): number {
            return Math.max(1, ...this.records.map((record) => record.yield_wh));
        },
    },
    methods: {
        onToggle(event: Event) {
            const details = event.currentTarget as HTMLDetailsElement;
            if (details.open) {
                this.fetchHistory();
            }
        },
        async fetchHistory() {
            this.loading = true;
            try {
                const response = await fetch('/api/livedata/daily-yield', {
                    headers: authHeader(),
                    cache: 'no-store',
                });
                if (!response.ok) {
                    throw new Error('HTTP ' + response.status);
                }
                const data = (await response.json()) as DailyYieldResponse;
                this.retentionDays = data.retention_days;
                this.records = data.records;
            } catch (error) {
                console.warn('Could not load daily yield history', error);
                this.records = [];
            } finally {
                this.loading = false;
            }
        },
        barWidth(value: number): string {
            return Math.max(0, Math.min(100, (value / this.maxYield) * 100)).toFixed(1) + '%';
        },
        formatDate(day: number): string {
            const text = String(day).padStart(8, '0');
            const date = new Date(Number(text.slice(0, 4)), Number(text.slice(4, 6)) - 1, Number(text.slice(6, 8)));
            return new Intl.DateTimeFormat(String(this.$i18n.locale), {
                weekday: 'short',
                day: '2-digit',
                month: '2-digit',
            }).format(date);
        },
        formatEnergy(valueWh: number): string {
            if (valueWh >= 1000) {
                return this.$n(valueWh / 1000, 'decimalTwoDigits') + ' kWh';
            }
            return this.$n(valueWh, 'decimalNoDigits') + ' Wh';
        },
    },
});
</script>

<style scoped>
.daily-yield-card {
    overflow: hidden;
    border-color: var(--bs-border-color);
    border-radius: 0.9rem;
    background: var(--bs-tertiary-bg);
}

.daily-yield-summary {
    display: flex;
    min-height: 4.25rem;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
    padding: 0.8rem 1rem;
    cursor: pointer;
    list-style: none;
}

.daily-yield-summary::-webkit-details-marker {
    display: none;
}

.daily-yield-summary strong {
    color: var(--bs-success);
}

.summary-chevron {
    width: 0.65rem;
    height: 0.65rem;
    flex: 0 0 auto;
    border-right: 2px solid var(--bs-success);
    border-bottom: 2px solid var(--bs-success);
    transform: rotate(45deg);
    transition: transform 0.15s ease;
}

details[open] .summary-chevron {
    transform: rotate(225deg);
}

.daily-yield-body {
    padding: 0.25rem 1rem 1rem;
    border-top: 1px solid var(--bs-border-color);
}

.daily-yield-state {
    display: flex;
    min-height: 5rem;
    align-items: center;
    justify-content: center;
    gap: 0.55rem;
}

.daily-yield-list {
    display: grid;
    gap: 0.35rem;
    padding-top: 0.75rem;
}

.daily-yield-row {
    display: grid;
    grid-template-columns: minmax(6.5rem, 0.65fr) minmax(8rem, 2fr) minmax(5.5rem, auto);
    align-items: center;
    gap: 0.75rem;
    min-height: 2rem;
}

.daily-yield-date {
    color: var(--bs-secondary-color);
    font-size: 0.82rem;
}

.daily-yield-date.is-today {
    color: var(--bs-body-color);
    font-weight: 700;
}

.daily-yield-track {
    display: block;
    height: 0.55rem;
    overflow: hidden;
    border-radius: 999px;
    background: var(--bs-secondary-bg);
}

.daily-yield-bar {
    display: block;
    height: 100%;
    border-radius: inherit;
    background: var(--bs-warning);
}

.daily-yield-bar.is-today {
    background: var(--bs-success);
}

.daily-yield-value {
    text-align: right;
    font-size: 0.86rem;
    font-variant-numeric: tabular-nums;
}

@media (max-width: 575.98px) {
    .daily-yield-row {
        grid-template-columns: minmax(5rem, 0.75fr) minmax(4rem, 1fr) minmax(5rem, auto);
        gap: 0.5rem;
    }
}
</style>
