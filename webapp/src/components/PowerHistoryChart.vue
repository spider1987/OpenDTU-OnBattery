<template>
    <section v-if="history.enabled" class="card power-history-card mt-3" aria-labelledby="power-history-title">
        <div class="card-header power-history-header">
            <div>
                <h2 id="power-history-title" class="h5 mb-1">{{ $t('home.PowerHistory') }}</h2>
                <p class="power-history-subtitle mb-0">{{ $t('home.PowerHistoryHint') }}</p>
            </div>
            <div class="btn-group btn-group-sm" role="group" :aria-label="$t('home.HistoryPeriod')">
                <button
                    type="button"
                    class="btn"
                    :class="hours === 12 ? 'btn-primary' : 'btn-outline-primary'"
                    @click="setHours(12)"
                >
                    {{ $t('home.Last12Hours') }}
                </button>
                <button
                    type="button"
                    class="btn"
                    :class="hours === 24 ? 'btn-primary' : 'btn-outline-primary'"
                    @click="setHours(24)"
                >
                    {{ $t('home.Last24Hours') }}
                </button>
            </div>
        </div>

        <div class="card-body power-history-body" :class="{ 'has-chart-data': hasChartData }">
            <div v-if="loading" class="power-history-state">
                <span class="spinner-border spinner-border-sm" aria-hidden="true"></span>
                {{ $t('base.Loading') }}
            </div>
            <div v-else-if="!hasChartData" class="power-history-state text-body-secondary">
                {{ $t('home.NoHistoryData') }}
            </div>
            <template v-else>
                <div class="power-history-plot">
                    <svg
                        viewBox="0 0 1000 300"
                        role="img"
                        :aria-label="$t('home.PowerHistory')"
                        preserveAspectRatio="none"
                    >
                        <g class="history-grid">
                            <template v-for="tick in yTicks" :key="'y-' + tick.value">
                                <line x1="62" x2="985" :y1="tick.y" :y2="tick.y" />
                                <text x="54" :y="tick.y + 4" text-anchor="end">{{ tick.label }}</text>
                            </template>
                            <line x1="62" x2="985" :y1="zeroY" :y2="zeroY" class="zero-line" />
                            <template v-for="tick in xTicks" :key="'x-' + tick.timestamp">
                                <line :x1="tick.x" :x2="tick.x" y1="15" y2="270" />
                                <text :x="tick.x" y="292" text-anchor="middle">{{ tick.label }}</text>
                            </template>
                        </g>
                        <path
                            v-for="series in chartSeries"
                            :key="series.key"
                            class="history-line"
                            :style="{ stroke: series.color }"
                            :d="series.path"
                        />
                    </svg>
                </div>
                <div class="power-history-legend" :aria-label="$t('home.Legend')">
                    <div v-for="series in chartSeries" :key="'legend-' + series.key" class="history-legend-item">
                        <span class="history-legend-swatch" :style="{ backgroundColor: series.color }"></span>
                        <span>{{ series.label }}</span>
                        <strong>{{ formatPower(series.lastValue) }}</strong>
                    </div>
                </div>
            </template>
        </div>
    </section>
</template>

<script lang="ts">
import { authHeader } from '@/utils/authentication';
import { defineComponent } from 'vue';

interface HistoryInverter {
    serial: string;
    name: string;
}

interface PowerHistory {
    enabled: boolean;
    power_meter: boolean;
    inverter_total: boolean;
    sample_interval: number;
    hours: number;
    inverters: HistoryInverter[];
    points: Array<Array<number | null>>;
}

interface ChartSeries {
    key: string;
    label: string;
    color: string;
    path: string;
    lastValue: number | null;
}

const EMPTY_HISTORY: PowerHistory = {
    enabled: false,
    power_meter: false,
    inverter_total: false,
    sample_interval: 60,
    hours: 24,
    inverters: [],
    points: [],
};

export default defineComponent({
    data() {
        return {
            hours: 24,
            loading: true,
            history: { ...EMPTY_HISTORY } as PowerHistory,
            refreshTimer: 0,
        };
    },
    computed: {
        hasChartData(): boolean {
            return this.history.points.length > 1;
        },
        timeRange(): { min: number; max: number } {
            if (!this.hasChartData) {
                return { min: 0, max: 1 };
            }
            return {
                min: Number(this.history.points[0]?.[0] ?? 0),
                max: Number(this.history.points[this.history.points.length - 1]?.[0] ?? 1),
            };
        },
        valueRange(): { min: number; max: number } {
            const values = this.history.points.flatMap((point) =>
                point.slice(1).filter((value): value is number => typeof value === 'number')
            );
            if (values.length === 0) {
                return { min: -1, max: 1 };
            }
            let min = Math.min(0, ...values);
            let max = Math.max(0, ...values);
            if (min === max) {
                max += 1;
                min -= 1;
            }
            const padding = (max - min) * 0.08;
            return { min: min - padding, max: max + padding };
        },
        chartSeries(): ChartSeries[] {
            const definitions = [
                ...(this.history.power_meter
                    ? [
                          {
                              key: 'grid',
                              label: this.$t('home.NetworkPower'),
                              color: 'var(--history-grid-power)',
                          },
                      ]
                    : []),
                ...this.history.inverters.map((inverter, index) => ({
                    key: inverter.serial,
                    label: inverter.name,
                    color: 'var(--history-inverter-' + (index % 10) + ')',
                })),
                ...(this.history.inverter_total
                    ? [
                          {
                              key: 'inverter-total',
                              label: this.$t('home.InverterTotal'),
                              color: 'var(--history-inverter-' + (this.history.inverters.length % 10) + ')',
                          },
                      ]
                    : []),
            ];

            return definitions.map((definition, seriesIndex) => {
                const values = this.history.points.map((point) => point[seriesIndex + 1] ?? null);
                return {
                    ...definition,
                    path: this.createPath(values),
                    lastValue:
                        [...values].reverse().find((value): value is number => typeof value === 'number') ?? null,
                };
            });
        },
        yTicks(): Array<{ value: number; y: number; label: string }> {
            return Array.from({ length: 5 }, (_, index) => {
                const ratio = index / 4;
                const value = this.valueRange.max - ratio * (this.valueRange.max - this.valueRange.min);
                return {
                    value,
                    y: 15 + ratio * 255,
                    label: this.formatAxisPower(value),
                };
            });
        },
        xTicks(): Array<{ timestamp: number; x: number; label: string }> {
            return Array.from({ length: 5 }, (_, index) => {
                const ratio = index / 4;
                const timestamp = this.timeRange.min + ratio * (this.timeRange.max - this.timeRange.min);
                return {
                    timestamp,
                    x: 62 + ratio * 923,
                    label: this.formatTime(timestamp),
                };
            });
        },
        zeroY(): number {
            return this.scaleY(0);
        },
    },
    mounted() {
        this.fetchHistory();
    },
    beforeUnmount() {
        window.clearInterval(this.refreshTimer);
    },
    methods: {
        setHours(hours: number) {
            if (this.hours === hours) {
                return;
            }
            this.hours = hours;
            this.fetchHistory();
        },
        async fetchHistory(showLoading: boolean = true) {
            if (showLoading) {
                this.loading = true;
            }
            try {
                const response = await fetch('/api/livedata/power-history?hours=' + this.hours, {
                    headers: authHeader(),
                    cache: 'no-store',
                });
                if (!response.ok) {
                    throw new Error('HTTP ' + response.status);
                }
                this.history = (await response.json()) as PowerHistory;
                this.scheduleRefresh();
            } catch (error) {
                console.warn('Could not load power history', error);
            } finally {
                this.loading = false;
            }
        },
        scheduleRefresh() {
            window.clearInterval(this.refreshTimer);
            if (!this.history.enabled) {
                return;
            }
            const refreshMs = Math.max(60_000, this.history.sample_interval * 1000);
            this.refreshTimer = window.setInterval(() => this.fetchHistory(false), refreshMs);
        },
        scaleX(timestamp: number): number {
            const range = this.timeRange.max - this.timeRange.min;
            return 62 + ((timestamp - this.timeRange.min) / (range || 1)) * 923;
        },
        scaleY(value: number): number {
            const range = this.valueRange.max - this.valueRange.min;
            return 15 + ((this.valueRange.max - value) / (range || 1)) * 255;
        },
        createPath(values: Array<number | null>): string {
            let path = '';
            let segmentOpen = false;
            values.forEach((value, index) => {
                if (typeof value !== 'number') {
                    segmentOpen = false;
                    return;
                }
                const timestamp = Number(this.history.points[index]?.[0] ?? 0);
                const command = segmentOpen ? 'L' : 'M';
                path += command + this.scaleX(timestamp).toFixed(1) + ',' + this.scaleY(value).toFixed(1) + ' ';
                segmentOpen = true;
            });
            return path.trim();
        },
        formatAxisPower(value: number): string {
            const absolute = Math.abs(value);
            if (absolute >= 1000) {
                return this.$n(value / 1000, 'decimalOneDigit') + ' kW';
            }
            return this.$n(value, 'decimalNoDigits') + ' W';
        },
        formatPower(value: number | null): string {
            return value === null ? '–' : this.$n(value, 'decimalNoDigits') + ' W';
        },
        formatTime(timestamp: number): string {
            return new Intl.DateTimeFormat(String(this.$i18n.locale), {
                hour: '2-digit',
                minute: '2-digit',
            }).format(new Date(timestamp * 1000));
        },
    },
});
</script>

<style scoped>
.power-history-card {
    overflow: hidden;
    border-color: var(--bs-border-color);
    border-radius: 0.9rem;
    background: var(--bs-tertiary-bg);
}

.power-history-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
    padding: 0.9rem 1rem;
    background: transparent;
    border-bottom-color: var(--bs-border-color);
}

.power-history-subtitle {
    color: var(--bs-secondary-color);
    font-size: 0.78rem;
}

.power-history-body {
    min-height: 8rem;
    padding: 0.8rem 1rem 1rem;
}

.power-history-body.has-chart-data {
    min-height: 15rem;
}

.power-history-state {
    display: flex;
    min-height: 6rem;
    align-items: center;
    justify-content: center;
    gap: 0.6rem;
}

.power-history-plot {
    width: 100%;
    min-height: 15rem;
}

.power-history-plot svg {
    display: block;
    width: 100%;
    height: clamp(15rem, 28vw, 23rem);
    overflow: visible;
}

.history-grid line {
    stroke: var(--bs-border-color);
    stroke-width: 1;
    vector-effect: non-scaling-stroke;
}

.history-grid .zero-line {
    stroke: var(--bs-secondary-color);
    stroke-width: 1.25;
}

.history-grid text {
    fill: var(--bs-secondary-color);
    font-size: 12px;
}

.history-line {
    fill: none;
    stroke-width: 2;
    stroke-linecap: round;
    stroke-linejoin: round;
    vector-effect: non-scaling-stroke;
}

.power-history-legend {
    display: flex;
    flex-wrap: wrap;
    gap: 0.55rem 1.2rem;
    padding: 0.7rem 0.4rem 0;
    border-top: 1px solid var(--bs-border-color);
}

.history-legend-item {
    display: grid;
    grid-template-columns: auto auto auto;
    align-items: center;
    gap: 0.4rem;
    color: var(--bs-secondary-color);
    font-size: 0.82rem;
}

.history-legend-item strong {
    color: var(--bs-body-color);
    font-variant-numeric: tabular-nums;
}

.history-legend-swatch {
    width: 0.8rem;
    height: 0.2rem;
    border-radius: 1rem;
}

@media (max-width: 575.98px) {
    .power-history-header {
        align-items: stretch;
        flex-direction: column;
    }

    .power-history-header .btn-group {
        align-self: flex-start;
    }

    .power-history-body {
        padding-inline: 0.4rem;
    }

    .power-history-plot {
        overflow-x: auto;
    }

    .power-history-plot svg {
        min-width: 44rem;
    }
}
</style>
