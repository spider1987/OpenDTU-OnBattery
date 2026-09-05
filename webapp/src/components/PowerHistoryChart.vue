<template>
    <section v-if="history.enabled" class="card power-history-card mt-3" aria-labelledby="power-history-title">
        <div class="card-header power-history-header">
            <div>
                <h2 id="power-history-title" class="h5 mb-1">{{ $t('home.PowerHistory') }}</h2>
                <p class="power-history-subtitle mb-0">{{ $t('home.PowerHistoryHint') }}</p>
                <p v-if="history.stored_points" class="power-history-samples mb-0">
                    {{
                        $t('home.HistorySamples', {
                            count: history.stored_points,
                            minutes: Math.max(1, Math.round(history.display_interval / 60)),
                        })
                    }}
                </p>
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
                <div ref="chartPlot" class="power-history-plot">
                    <div class="power-history-canvas">
                        <svg
                            ref="chartSvg"
                            viewBox="0 0 1000 300"
                            role="img"
                            :aria-label="$t('home.PowerHistory')"
                            preserveAspectRatio="none"
                            @pointerdown="handlePointerDown"
                            @pointermove="handlePointerMove"
                            @pointerleave="handlePointerLeave"
                        >
                            <g class="history-grid">
                                <template v-for="tick in yTicks" :key="'y-' + tick.value">
                                    <line x1="62" x2="930" :y1="tick.y" :y2="tick.y" />
                                    <text x="54" :y="tick.y + 4" text-anchor="end">{{ tick.label }}</text>
                                </template>
                                <line x1="62" x2="930" :y1="zeroY" :y2="zeroY" class="zero-line" />
                                <template v-if="history.voltage">
                                    <template v-for="tick in voltageTicks" :key="'voltage-' + tick.value">
                                        <text x="992" :y="tick.y + 4" text-anchor="end">{{ tick.label }}</text>
                                    </template>
                                </template>
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
                            <g v-if="showSampleMarkers" aria-hidden="true">
                                <template v-for="series in chartSeries" :key="'points-' + series.key">
                                    <circle
                                        v-for="(point, index) in series.points"
                                        :key="series.key + '-point-' + index"
                                        class="history-sample-point"
                                        :style="{ fill: series.color }"
                                        :cx="point.x"
                                        :cy="point.y"
                                        r="1.35"
                                    />
                                </template>
                            </g>
                            <rect class="history-hit-area" x="62" y="15" width="868" height="255" />
                            <g v-if="hoveredPoint" class="history-hover" aria-hidden="true">
                                <line
                                    class="history-hover-line"
                                    :x1="hoveredPoint.x"
                                    :x2="hoveredPoint.x"
                                    y1="15"
                                    y2="270"
                                />
                                <circle
                                    v-for="series in hoveredPoint.values.filter((entry) => entry.y !== null)"
                                    :key="'hover-' + series.key"
                                    class="history-hover-point"
                                    :style="{ stroke: series.color }"
                                    :cx="hoveredPoint.x"
                                    :cy="series.y ?? 0"
                                    r="4"
                                />
                            </g>
                        </svg>
                    </div>
                    <div
                        v-if="hoveredPoint"
                        class="power-history-tooltip"
                        :class="{ 'align-end': hoverAlignEnd }"
                        :style="{ left: hoverTooltipX + 'px' }"
                    >
                        <strong class="power-history-tooltip-time">{{
                            formatHoverTime(hoveredPoint.timestamp)
                        }}</strong>
                        <div
                            v-for="series in hoveredPoint.values"
                            :key="'tooltip-' + series.key"
                            class="tooltip-series"
                        >
                            <span class="history-legend-swatch" :style="{ backgroundColor: series.color }"></span>
                            <span>{{ series.label }}</span>
                            <strong>{{ formatSeriesValue(series.value, series.unit) }}</strong>
                        </div>
                    </div>
                </div>
                <div class="power-history-legend" :aria-label="$t('home.Legend')">
                    <div v-for="series in chartSeries" :key="'legend-' + series.key" class="history-legend-item">
                        <span class="history-legend-swatch" :style="{ backgroundColor: series.color }"></span>
                        <span>{{ series.label }}</span>
                        <strong>{{ formatSeriesValue(series.lastValue, series.unit) }}</strong>
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

interface VoltageSource extends HistoryInverter {
    channel: number;
}

interface PowerHistory {
    enabled: boolean;
    power_meter: boolean;
    inverter_total: boolean;
    voltage: boolean;
    voltage_source: VoltageSource | null;
    sample_interval: number;
    display_interval: number;
    stored_points: number;
    hours: number;
    inverters: HistoryInverter[];
    points: Array<Array<number | null>>;
}

interface ChartSeries {
    key: string;
    label: string;
    color: string;
    path: string;
    points: Array<{ x: number; y: number }>;
    lastValue: number | null;
    unit: 'power' | 'voltage';
}

interface HoverSeries {
    key: string;
    label: string;
    color: string;
    value: number | null;
    y: number | null;
    unit: 'power' | 'voltage';
}

interface HoveredPoint {
    timestamp: number;
    x: number;
    values: HoverSeries[];
}

const EMPTY_HISTORY: PowerHistory = {
    enabled: false,
    power_meter: false,
    inverter_total: false,
    voltage: false,
    voltage_source: null,
    sample_interval: 60,
    display_interval: 60,
    stored_points: 0,
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
            hoveredPointIndex: null as number | null,
            hoverTooltipX: 0,
            hoverAlignEnd: false,
            hoverPinned: false,
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
            const seriesCount =
                (this.history.power_meter ? 1 : 0) +
                this.history.inverters.length +
                (this.history.inverter_total ? 1 : 0);
            const values = this.history.points.flatMap((point) =>
                point.slice(1, seriesCount + 1).filter((value): value is number => typeof value === 'number')
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
        voltageRange(): { min: number; max: number } {
            const voltageIndex =
                1 +
                (this.history.power_meter ? 1 : 0) +
                this.history.inverters.length +
                (this.history.inverter_total ? 1 : 0);
            const values = this.history.voltage
                ? this.history.points
                      .map((point) => point[voltageIndex])
                      .filter((value): value is number => typeof value === 'number')
                : [];
            if (values.length === 0) {
                return { min: 0, max: 1 };
            }
            let min = Math.min(...values);
            let max = Math.max(...values);
            if (min === max) {
                min -= 0.5;
                max += 0.5;
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
                              unit: 'power' as const,
                          },
                      ]
                    : []),
                ...this.history.inverters.map((inverter, index) => ({
                    key: inverter.serial,
                    label: inverter.name,
                    color: 'var(--history-inverter-' + (index % 10) + ')',
                    unit: 'power' as const,
                })),
                ...(this.history.inverter_total
                    ? [
                          {
                              key: 'inverter-total',
                              label: this.$t('home.InverterTotal'),
                              color: 'var(--history-inverter-' + (this.history.inverters.length % 10) + ')',
                              unit: 'power' as const,
                          },
                      ]
                    : []),
                ...(this.history.voltage && this.history.voltage_source
                    ? [
                          {
                              key: 'dc-voltage',
                              label: this.$t('home.DcVoltage', {
                                  name: this.history.voltage_source.name,
                                  channel: this.history.voltage_source.channel,
                              }),
                              color: 'var(--history-voltage)',
                              unit: 'voltage' as const,
                          },
                      ]
                    : []),
            ];

            return definitions.map((definition, seriesIndex) => {
                const values = this.history.points.map((point) => point[seriesIndex + 1] ?? null);
                return {
                    ...definition,
                    path: this.createPath(values, definition.unit),
                    points: values.flatMap((value, index) => {
                        if (typeof value !== 'number') {
                            return [];
                        }
                        const timestamp = Number(this.history.points[index]?.[0] ?? 0);
                        return [{ x: this.scaleX(timestamp), y: this.scaleSeriesY(value, definition.unit) }];
                    }),
                    lastValue:
                        [...values].reverse().find((value): value is number => typeof value === 'number') ?? null,
                };
            });
        },
        showSampleMarkers(): boolean {
            return this.history.points.length * Math.max(1, this.chartSeries.length) <= 500;
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
        voltageTicks(): Array<{ value: number; y: number; label: string }> {
            return Array.from({ length: 5 }, (_, index) => {
                const ratio = index / 4;
                const value = this.voltageRange.max - ratio * (this.voltageRange.max - this.voltageRange.min);
                return {
                    value,
                    y: 15 + ratio * 255,
                    label: this.formatAxisVoltage(value),
                };
            });
        },
        xTicks(): Array<{ timestamp: number; x: number; label: string }> {
            return Array.from({ length: 5 }, (_, index) => {
                const ratio = index / 4;
                const timestamp = this.timeRange.min + ratio * (this.timeRange.max - this.timeRange.min);
                return {
                    timestamp,
                    x: 62 + ratio * 868,
                    label: this.formatTime(timestamp),
                };
            });
        },
        zeroY(): number {
            return this.scaleY(0);
        },
        hoveredPoint(): HoveredPoint | null {
            if (this.hoveredPointIndex === null) {
                return null;
            }
            const point = this.history.points[this.hoveredPointIndex];
            if (!point) {
                return null;
            }
            const timestamp = Number(point[0]);
            return {
                timestamp,
                x: this.scaleX(timestamp),
                values: this.chartSeries.map((series, seriesIndex) => {
                    const rawValue = point[seriesIndex + 1];
                    const value = typeof rawValue === 'number' ? rawValue : null;
                    return {
                        key: series.key,
                        label: series.label,
                        color: series.color,
                        value,
                        y: value === null ? null : this.scaleSeriesY(value, series.unit),
                        unit: series.unit,
                    };
                }),
            };
        },
    },
    mounted() {
        this.fetchHistory();
        document.addEventListener('pointerdown', this.handleDocumentPointerDown);
    },
    beforeUnmount() {
        window.clearInterval(this.refreshTimer);
        document.removeEventListener('pointerdown', this.handleDocumentPointerDown);
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
                this.clearHover();
            }
            try {
                const response = await fetch('/api/livedata/power-history?hours=' + this.hours, {
                    headers: authHeader(),
                    cache: 'no-store',
                });
                if (!response.ok) {
                    throw new Error('HTTP ' + response.status);
                }
                const selectedTimestamp = this.hoveredPoint?.timestamp ?? null;
                const hoverPinned = this.hoverPinned;
                this.history = (await response.json()) as PowerHistory;
                if (!showLoading && selectedTimestamp !== null) {
                    this.hoveredPointIndex = this.findClosestPointIndex(selectedTimestamp);
                    this.hoverPinned = hoverPinned && this.hoveredPointIndex !== null;
                }
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
            return 62 + ((timestamp - this.timeRange.min) / (range || 1)) * 868;
        },
        scaleY(value: number): number {
            const range = this.valueRange.max - this.valueRange.min;
            return 15 + ((this.valueRange.max - value) / (range || 1)) * 255;
        },
        scaleVoltageY(value: number): number {
            const range = this.voltageRange.max - this.voltageRange.min;
            return 15 + ((this.voltageRange.max - value) / (range || 1)) * 255;
        },
        scaleSeriesY(value: number, unit: 'power' | 'voltage'): number {
            return unit === 'voltage' ? this.scaleVoltageY(value) : this.scaleY(value);
        },
        createPath(values: Array<number | null>, unit: 'power' | 'voltage'): string {
            let path = '';
            let segmentOpen = false;
            values.forEach((value, index) => {
                if (typeof value !== 'number') {
                    segmentOpen = false;
                    return;
                }
                const timestamp = Number(this.history.points[index]?.[0] ?? 0);
                const command = segmentOpen ? 'L' : 'M';
                path +=
                    command +
                    this.scaleX(timestamp).toFixed(1) +
                    ',' +
                    this.scaleSeriesY(value, unit).toFixed(1) +
                    ' ';
                segmentOpen = true;
            });
            return path.trim();
        },
        handlePointerDown(event: PointerEvent) {
            this.updateHover(event);
            this.hoverPinned = event.pointerType !== 'mouse';
        },
        handlePointerMove(event: PointerEvent) {
            if (event.pointerType !== 'mouse') {
                return;
            }
            this.hoverPinned = false;
            this.updateHover(event);
        },
        handlePointerLeave(event: PointerEvent) {
            if (event.pointerType === 'mouse' && !this.hoverPinned) {
                this.clearHover();
            }
        },
        handleDocumentPointerDown(event: PointerEvent) {
            if (!this.hoverPinned) {
                return;
            }
            const plot = this.$refs.chartPlot as HTMLDivElement | undefined;
            if (plot && event.target instanceof Node && plot.contains(event.target)) {
                return;
            }
            this.clearHover();
        },
        findClosestPointIndex(timestamp: number): number | null {
            if (!this.history.points.length) {
                return null;
            }
            let closestIndex = 0;
            let closestDistance = Number.POSITIVE_INFINITY;
            this.history.points.forEach((point, index) => {
                const distance = Math.abs(Number(point[0]) - timestamp);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestIndex = index;
                }
            });
            return closestIndex;
        },
        updateHover(event: PointerEvent) {
            if (!this.hasChartData) {
                return;
            }
            const svg = this.$refs.chartSvg as SVGSVGElement | undefined;
            const plot = this.$refs.chartPlot as HTMLDivElement | undefined;
            if (!svg || !plot) {
                return;
            }
            const svgRect = svg.getBoundingClientRect();
            const plotRect = plot.getBoundingClientRect();
            const svgX = ((event.clientX - svgRect.left) / svgRect.width) * 1000;
            const clampedX = Math.min(930, Math.max(62, svgX));
            const timestamp =
                this.timeRange.min + ((clampedX - 62) / 868) * (this.timeRange.max - this.timeRange.min);

            this.hoveredPointIndex = this.findClosestPointIndex(timestamp);
            this.hoverTooltipX = Math.max(8, Math.min(plotRect.width - 8, event.clientX - plotRect.left));
            this.hoverAlignEnd = this.hoverTooltipX > plotRect.width * 0.62;
        },
        clearHover() {
            this.hoveredPointIndex = null;
            this.hoverPinned = false;
        },
        formatAxisPower(value: number): string {
            const absolute = Math.abs(value);
            if (absolute >= 1000) {
                return this.$n(value / 1000, 'decimalOneDigit') + ' kW';
            }
            return this.$n(value, 'decimalNoDigits') + ' W';
        },
        formatAxisVoltage(value: number): string {
            return this.$n(value, 'decimalOneDigit') + ' V';
        },
        formatSeriesValue(value: number | null, unit: 'power' | 'voltage'): string {
            if (value === null) {
                return '–';
            }
            return unit === 'voltage'
                ? this.$n(value, 'decimalTwoDigits') + ' V'
                : this.$n(value, 'decimalNoDigits') + ' W';
        },
        formatTime(timestamp: number): string {
            return new Intl.DateTimeFormat(String(this.$i18n.locale), {
                hour: '2-digit',
                minute: '2-digit',
            }).format(new Date(timestamp * 1000));
        },
        formatHoverTime(timestamp: number): string {
            return new Intl.DateTimeFormat(String(this.$i18n.locale), {
                day: '2-digit',
                month: '2-digit',
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

.power-history-header h2 {
    color: var(--bs-primary);
}

.power-history-samples {
    margin-top: 0.2rem;
    color: rgba(var(--bs-warning-rgb), 0.82);
    font-size: 0.72rem;
    font-variant-numeric: tabular-nums;
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
    position: relative;
    width: 100%;
    min-height: 15rem;
}

.power-history-canvas {
    width: 100%;
}

.power-history-canvas svg {
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

.history-sample-point {
    stroke: none;
    pointer-events: none;
}

.history-hit-area {
    cursor: crosshair;
    fill: transparent;
    pointer-events: all;
}

.history-hover-line {
    stroke: var(--bs-emphasis-color);
    stroke-dasharray: 4 4;
    stroke-width: 1.25;
    vector-effect: non-scaling-stroke;
}

.history-hover-point {
    fill: var(--bs-tertiary-bg);
    stroke-width: 2.5;
    vector-effect: non-scaling-stroke;
}

.power-history-tooltip {
    position: absolute;
    z-index: 2;
    top: 0.45rem;
    min-width: 12rem;
    max-width: min(20rem, calc(100% - 1rem));
    padding: 0.55rem 0.65rem;
    border: 1px solid var(--bs-border-color);
    border-radius: 0.55rem;
    background: var(--bs-body-bg);
    box-shadow: 0 0.35rem 1rem rgba(0, 0, 0, 0.22);
    color: var(--bs-body-color);
    font-size: 0.78rem;
    pointer-events: none;
    transform: translateX(0.65rem);
}

.power-history-tooltip.align-end {
    transform: translateX(calc(-100% - 0.65rem));
}

.power-history-tooltip-time {
    display: block;
    margin-bottom: 0.35rem;
    font-variant-numeric: tabular-nums;
}

.tooltip-series {
    display: grid;
    grid-template-columns: auto minmax(0, 1fr) auto;
    align-items: center;
    gap: 0.4rem;
    color: var(--bs-secondary-color);
}

.tooltip-series + .tooltip-series {
    margin-top: 0.2rem;
}

.tooltip-series strong {
    color: var(--bs-body-color);
    font-variant-numeric: tabular-nums;
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

    .power-history-canvas {
        overflow-x: auto;
        overscroll-behavior-inline: contain;
    }

    .power-history-canvas svg {
        min-width: 44rem;
    }

    .power-history-tooltip {
        position: fixed;
        right: 0.5rem;
        top: auto;
        bottom: 0.75rem;
        left: 0.5rem !important;
        max-width: calc(100vw - 1rem);
        transform: none !important;
    }
}
</style>
