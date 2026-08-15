<template>
    <BasePage
        :title="$t('home.LiveData')"
        :isLoading="dataLoading"
        :isWideScreen="true"
        :showWebSocket="true"
        :isWebsocketConnected="isWebsocketConnected"
        @reload="reloadData"
    >
        <HintView :hints="liveData.hints" />
        <InverterTotalInfo
            :totalData="liveData.total"
            :hasInverters="hasInverters"
            :solarChargerData="liveData.solarcharger"
            :totalBattData="liveData.battery"
            :powerMeterData="liveData.power_meter"
            :gridChargerData="liveData.gridcharger"
        />
        <PowerHistoryChart />
        <div class="row g-3 mt-1 inverter-workspace">
            <div class="col-lg-2" :style="[inverterData.length == 1 ? { display: 'none' } : {}]">
                <div
                    class="nav nav-pills inverter-nav gap-2"
                    id="v-pills-tab"
                    role="tablist"
                    aria-orientation="vertical"
                >
                    <button
                        v-for="inverter in inverterData"
                        :key="inverter.serial"
                        class="nav-link inverter-nav-item text-break"
                        :id="'v-pills-' + inverter.serial + '-tab'"
                        data-bs-toggle="pill"
                        :data-bs-target="'#v-pills-' + inverter.serial"
                        type="button"
                        role="tab"
                        aria-controls="'v-pills-' + inverter.serial"
                        aria-selected="true"
                    >
                        <div class="d-flex align-items-center">
                            <div class="me-3">
                                <span
                                    v-if="inverter.AC"
                                    class="badge inverter-power-badge"
                                    :class="{
                                        'text-bg-secondary': !inverter.poll_enabled,
                                        'text-bg-danger': inverter.poll_enabled && !inverter.reachable,
                                        'text-bg-warning':
                                            inverter.poll_enabled && inverter.reachable && !inverter.producing,
                                        'text-bg-success':
                                            inverter.poll_enabled && inverter.reachable && inverter.producing,
                                    }"
                                >
                                    {{ $n(inverter.AC[0]?.Power?.v || 0, 'decimalNoDigits') }}
                                    {{ inverter.AC[0]?.Power?.u }}
                                </span>
                                <span v-else class="badge text-bg-light">-</span>
                            </div>
                            <div class="inverter-nav-name">
                                {{ inverter.name }}
                            </div>
                        </div>
                    </button>
                </div>
            </div>

            <div
                class="tab-content"
                id="v-pills-tabContent"
                :class="{
                    'col-lg-10': inverterData.length > 1,
                    'col-12': inverterData.length == 1,
                }"
            >
                <div
                    v-for="inverter in inverterData"
                    :key="inverter.serial"
                    class="tab-pane fade show"
                    :id="'v-pills-' + inverter.serial"
                    role="tabpanel"
                    :aria-labelledby="'v-pills-' + inverter.serial + '-tab'"
                    tabindex="0"
                >
                    <div class="card inverter-panel">
                        <div
                            class="card-header inverter-header d-flex flex-wrap justify-content-between align-items-center gap-3"
                            :class="{
                                'inverter-status-disabled': !inverter.poll_enabled,
                                'inverter-status-error': inverter.poll_enabled && !inverter.reachable,
                                'inverter-status-idle': inverter.poll_enabled && inverter.reachable && !inverter.producing,
                                'inverter-status-producing': inverter.poll_enabled && inverter.reachable && inverter.producing,
                            }"
                        >
                            <div class="inverter-identity flex-grow-1">
                                <div class="inverter-title-row">
                                    <span class="inverter-status-dot" aria-hidden="true"></span>
                                    <strong class="inverter-title">{{ inverter.name }}</strong>
                                    <span v-if="inverter.AC" class="inverter-live-power">
                                        {{ $n(inverter.AC[0]?.Power?.v || 0, 'decimalNoDigits') }}
                                        {{ inverter.AC[0]?.Power?.u }}
                                    </span>
                                </div>
                                <div class="inverter-meta">
                                    <span>
                                        {{ $t('home.SerialNumber') }}{{ inverter.serial }}
                                    </span>
                                    <span>
                                        {{ $t('home.CurrentLimit') }}:
                                        <template v-if="inverter.limit_absolute > -1">
                                            {{ $n(inverter.limit_absolute, 'decimalNoDigits') }} W | </template
                                        >{{ $n(inverter.limit_relative / 100, 'percentOneDigit') }}
                                    </span>
                                    <span>
                                        <DataAgeDisplay :data-age-ms="inverter.data_age_ms" />
                                    </span>
                                </div>
                            </div>
                            <div class="btn-toolbar inverter-actions" role="toolbar">
                                <div class="btn-group" role="group">
                                    <button
                                        :disabled="!isLogged"
                                        type="button"
                                        class="btn btn-sm btn-outline-danger inverter-action"
                                        @click="onShowLimitSettings(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowSetInverterLimit')"
                                    >
                                        <BIconSpeedometer />
                                    </button>
                                </div>

                                <div class="btn-group" role="group">
                                    <button
                                        :disabled="!isLogged"
                                        type="button"
                                        class="btn btn-sm btn-outline-danger inverter-action"
                                        @click="onShowPowerSettings(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.TurnOnOff')"
                                    >
                                        <BIconPower />
                                    </button>
                                </div>

                                <div class="btn-group" role="group">
                                    <button
                                        type="button"
                                        class="btn btn-sm btn-outline-info inverter-action"
                                        @click="onShowDevInfo(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowInverterInfo')"
                                    >
                                        <BIconCpu />
                                    </button>
                                </div>

                                <div class="btn-group" role="group">
                                    <button
                                        type="button"
                                        class="btn btn-sm btn-outline-info inverter-action"
                                        @click="onShowGridProfile(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowGridProfile')"
                                    >
                                        <BIconOutlet />
                                    </button>
                                </div>

                                <div class="btn-group" role="group">
                                    <button
                                        v-if="inverter.events >= 0"
                                        type="button"
                                        class="btn btn-sm btn-outline-secondary position-relative inverter-action"
                                        @click="onShowEventlog(inverter.serial)"
                                        v-tooltip
                                        :title="$t('home.ShowEventlog')"
                                    >
                                        <BIconJournalText />
                                        <span
                                            class="position-absolute top-0 start-100 translate-middle badge rounded-pill text-bg-danger"
                                        >
                                            {{ inverter.events }}
                                            <span class="visually-hidden">{{ $t('home.UnreadMessages') }}</span>
                                        </span>
                                    </button>
                                </div>
                            </div>
                        </div>
                        <div class="card-body inverter-body">
                            <div class="row flex-row-reverse flex-wrap-reverse g-3 channel-grid">
                                <template
                                    v-for="chanType in [
                                        { obj: inverter.INV, name: 'INV' },
                                        { obj: inverter.AC, name: 'AC' },
                                        { obj: inverter.DC, name: 'DC' },
                                    ].reverse()"
                                >
                                    <template v-if="chanType.obj != null">
                                        <template
                                            v-for="channel in Object.keys(chanType.obj)
                                                .sort()
                                                .reverse()
                                                .map((x) => +x)"
                                            :key="channel"
                                        >
                                            <template
                                                v-if="
                                                    chanType.name != 'DC' ||
                                                    (chanType.name == 'DC' && getSumIrridiation(inverter) == 0) ||
                                                    (chanType.name == 'DC' &&
                                                        getSumIrridiation(inverter) > 0 &&
                                                        chanType.obj[channel]?.Irradiation?.max) ||
                                                    0 > 0
                                                "
                                            >
                                                <div class="col" v-if="chanType.obj[channel]">
                                                    <InverterChannelInfo
                                                        :channelData="chanType.obj[channel]"
                                                        :channelType="chanType.name"
                                                        :channelNumber="channel"
                                                    />
                                                </div>
                                            </template>
                                        </template>
                                    </template>
                                </template>
                            </div>

                            <BootstrapAlert class="m-3" :show="!inverter.hasOwnProperty('INV')">
                                <div class="d-flex justify-content-center align-items-center">
                                    <div class="spinner-border m-1" role="status">
                                        <span class="visually-hidden">{{ $t('home.LoadingInverter') }}</span>
                                    </div>
                                    <span>{{ $t('home.LoadingInverter') }}</span>
                                </div>
                            </BootstrapAlert>

                            <div class="accordion radio-stats mt-4" id="accordionRadioStats">
                                <div class="accordion-item accordion-table">
                                    <h2 class="accordion-header">
                                        <button
                                            class="accordion-button collapsed"
                                            type="button"
                                            data-bs-toggle="collapse"
                                            data-bs-target="#collapseStats"
                                            aria-expanded="true"
                                            aria-controls="collapseStats"
                                        >
                                            <BIconBroadcast />&nbsp;{{ $t('home.RadioStats') }}
                                        </button>
                                    </h2>
                                    <div
                                        id="collapseStats"
                                        class="accordion-collapse collapse"
                                        data-bs-parent="#accordionRadioStats"
                                    >
                                        <div class="accordion-body">
                                            <table class="table table-striped table-hover">
                                                <tbody>
                                                    <tr>
                                                        <td>{{ $t('home.TxRequest') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.tx_request) }}</td>
                                                        <td></td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxSuccess') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_success) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_success,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxFailNothing') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_fail_nothing) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_fail_nothing,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxFailPartial') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_fail_partial) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_fail_partial,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.RxFailCorrupt') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.rx_fail_corrupt) }}</td>
                                                        <td>
                                                            {{
                                                                ratio(
                                                                    inverter.radio_stats.rx_fail_corrupt,
                                                                    inverter.radio_stats.tx_request
                                                                )
                                                            }}
                                                        </td>
                                                    </tr>
                                                    <tr>
                                                        <td>{{ $t('home.TxReRequest') }}</td>
                                                        <td>{{ $n(inverter.radio_stats.tx_re_request) }}</td>
                                                        <td></td>
                                                    </tr>
                                                    <tr>
                                                        <td>
                                                            {{ $t('home.Rssi') }}
                                                            <BIconInfoCircle v-tooltip :title="$t('home.RssiHint')" />
                                                        </td>
                                                        <td>
                                                            {{ $t('home.dBm', { dbm: $n(inverter.radio_stats.rssi) }) }}
                                                        </td>
                                                        <td></td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                            <div class="d-flex">
                                                <button
                                                    :disabled="!isLogged || performRadioStatsReset"
                                                    type="button"
                                                    class="btn btn-danger ms-auto me-3 mt-3"
                                                    @click="onResetRadioStats(inverter.serial)"
                                                >
                                                    <template v-if="!performRadioStatsReset">
                                                        <BIconArrowCounterclockwise />&nbsp;{{ $t('home.StatsReset') }}
                                                    </template>
                                                    <template v-else>
                                                        <span
                                                            class="spinner-border spinner-border-sm"
                                                            aria-hidden="true"
                                                        ></span>
                                                        <span role="status">&nbsp;{{ $t('home.StatsResetting') }}</span>
                                                    </template>
                                                </button>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <SolarChargerView v-if="liveData.solarcharger.enabled" />
        <BatteryView v-if="liveData.battery.enabled" />
        <GridChargerView v-if="liveData.gridcharger.enabled" />
    </BasePage>

    <ModalDialog modalId="eventView" :title="$t('home.EventLog')" :loading="eventLogLoading">
        <EventLog :eventLogList="eventLogList" />
    </ModalDialog>

    <ModalDialog modalId="devInfoView" :title="$t('home.InverterInfo')" :loading="devInfoLoading">
        <DevInfo :devInfoList="devInfoList" />
    </ModalDialog>

    <ModalDialog modalId="gridProfileView" :title="$t('home.GridProfile')" :loading="gridProfileLoading">
        <GridProfile :gridProfileList="gridProfileList" :gridProfileRawList="gridProfileRawList" />
    </ModalDialog>

    <ModalDialog modalId="limitSettingView" :title="$t('home.LimitSettings')" :loading="limitSettingLoading">
        <BootstrapAlert v-model="showAlertLimit" :variant="alertTypeLimit">
            {{ alertMessageLimit }}
        </BootstrapAlert>

        <div class="row mb-3">
            <label for="inputCurrentLimit" class="col-sm-3 col-form-label">{{ $t('home.CurrentLimit') }} </label>
            <div class="col-sm-4">
                <div class="input-group">
                    <input
                        type="text"
                        class="form-control"
                        id="inputCurrentLimit"
                        aria-describedby="currentLimitType"
                        v-model="currentLimitRelative"
                        disabled
                    />
                    <span class="input-group-text" id="currentLimitType">%</span>
                </div>
            </div>

            <div class="col-sm-4" v-if="currentLimitList.max_power > 0">
                <div class="input-group">
                    <input
                        type="text"
                        class="form-control"
                        id="inputCurrentLimitAbsolute"
                        aria-describedby="currentLimitTypeAbsolute"
                        v-model="currentLimitAbsolute"
                        disabled
                    />
                    <span class="input-group-text" id="currentLimitTypeAbsolute">W</span>
                </div>
            </div>
        </div>

        <div class="row mb-3 align-items-center">
            <label for="inputLastLimitSet" class="col-sm-3 col-form-label">
                {{ $t('home.LastLimitSetStatus') }}
            </label>
            <div class="col-sm-9">
                <span
                    class="badge"
                    :class="{
                        'text-bg-danger': currentLimitList.limit_set_status == 'Failure',
                        'text-bg-warning': currentLimitList.limit_set_status == 'Pending',
                        'text-bg-success': currentLimitList.limit_set_status == 'Ok',
                        'text-bg-secondary': currentLimitList.limit_set_status == 'Unknown',
                    }"
                >
                    {{ $t('home.' + currentLimitList.limit_set_status) }}
                </span>
            </div>
        </div>

        <div class="row mb-3">
            <label for="inputTargetLimit" class="col-sm-3 col-form-label">{{ $t('home.SetLimit') }}</label>
            <div class="col-sm-9">
                <div class="input-group">
                    <input
                        type="number"
                        name="inputTargetLimit"
                        class="form-control"
                        id="inputTargetLimit"
                        :min="targetLimitMin"
                        :max="targetLimitMax"
                        v-model="targetLimitList.limit_value"
                    />
                    <button
                        class="btn btn-primary dropdown-toggle"
                        type="button"
                        data-bs-toggle="dropdown"
                        aria-expanded="false"
                    >
                        {{ targetLimitTypeText }}
                    </button>
                    <ul class="dropdown-menu dropdown-menu-end">
                        <li>
                            <a class="dropdown-item" @click="onSelectType(true)" href="#">{{ $t('home.Relative') }}</a>
                        </li>
                        <li>
                            <a class="dropdown-item" @click="onSelectType(false)" href="#">{{ $t('home.Absolute') }}</a>
                        </li>
                    </ul>
                </div>
                <div
                    v-if="!targetLimitRelative"
                    class="alert alert-secondary mt-3"
                    role="alert"
                    v-html="$t('home.LimitHint')"
                ></div>
            </div>
        </div>

        <template #footer>
            <button type="button" class="btn btn-danger" @click="onSetLimitSettings(true)">
                {{ $t('home.SetPersistent') }}
            </button>

            <button type="button" class="btn btn-danger" @click="onSetLimitSettings(false)">
                {{ $t('home.SetNonPersistent') }}
            </button>
        </template>
    </ModalDialog>

    <ModalDialog modalId="powerSettingView" :title="$t('home.PowerSettings')" :loading="powerSettingLoading">
        <BootstrapAlert v-model="showAlertPower" :variant="alertTypePower">
            {{ alertMessagePower }}
        </BootstrapAlert>

        <div class="row mb-3 align-items-center">
            <label for="inputLastPowerSet" class="col col-form-label">{{ $t('home.LastPowerSetStatus') }}</label>
            <div class="col">
                <span
                    class="badge"
                    :class="{
                        'text-bg-danger': successCommandPower == 'Failure',
                        'text-bg-warning': successCommandPower == 'Pending',
                        'text-bg-success': successCommandPower == 'Ok',
                        'text-bg-secondary': successCommandPower == 'Unknown',
                    }"
                >
                    {{ $t('home.' + successCommandPower) }}
                </span>
            </div>
        </div>

        <div class="d-grid gap-2 col-6 mx-auto">
            <button type="button" class="btn btn-success" @click="onSetPowerSettings(true)">
                <BIconToggleOn class="fs-4" />&nbsp;{{ $t('home.TurnOn') }}
            </button>
            <button type="button" class="btn btn-danger" @click="onSetPowerSettings(false)">
                <BIconToggleOff class="fs-4" />&nbsp;{{ $t('home.TurnOff') }}
            </button>
            <button type="button" class="btn btn-warning" @click="onSetPowerSettings(true, true)">
                <BIconArrowCounterclockwise class="fs-4" />&nbsp;{{ $t('home.Restart') }}
            </button>
        </div>
    </ModalDialog>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import DataAgeDisplay from '@/components/DataAgeDisplay.vue';
import DevInfo from '@/components/DevInfo.vue';
import EventLog from '@/components/EventLog.vue';
import GridProfile from '@/components/GridProfile.vue';
import HintView from '@/components/HintView.vue';
import InverterChannelInfo from '@/components/InverterChannelInfo.vue';
import InverterTotalInfo from '@/components/InverterTotalInfo.vue';
import PowerHistoryChart from '@/components/PowerHistoryChart.vue';
import { LimitType } from '@/types/LimitConfig';
import ModalDialog from '@/components/ModalDialog.vue';
import SolarChargerView from '@/components/SolarChargerView.vue';
import GridChargerView from '@/components/GridChargerView.vue';
import BatteryView from '@/components/BatteryView.vue';
import type { DevInfoStatus } from '@/types/DevInfoStatus';
import type { EventlogItems } from '@/types/EventlogStatus';
import type { GridProfileRawdata } from '@/types/GridProfileRawdata';
import type { GridProfileStatus } from '@/types/GridProfileStatus';
import type { LimitConfig } from '@/types/LimitConfig';
import type { LimitStatus } from '@/types/LimitStatus';
import type { Inverter, LiveData } from '@/types/LiveDataStatus';
import { authHeader, authUrl, handleResponse, isLoggedIn } from '@/utils/authentication';
import * as bootstrap from 'bootstrap';
import {
    BIconArrowCounterclockwise,
    BIconBroadcast,
    BIconCpu,
    BIconInfoCircle,
    BIconJournalText,
    BIconOutlet,
    BIconPower,
    BIconSpeedometer,
    BIconToggleOff,
    BIconToggleOn,
} from 'bootstrap-icons-vue';
import { defineComponent } from 'vue';
import WebSocketService from '@/utils/websocketService';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        DataAgeDisplay,
        DevInfo,
        EventLog,
        GridProfile,
        HintView,
        InverterChannelInfo,
        InverterTotalInfo,
        PowerHistoryChart,
        ModalDialog,
        BIconArrowCounterclockwise,
        BIconBroadcast,
        BIconCpu,
        BIconInfoCircle,
        BIconJournalText,
        BIconOutlet,
        BIconPower,
        BIconSpeedometer,
        BIconToggleOff,
        BIconToggleOn,
        SolarChargerView,
        GridChargerView,
        BatteryView,
    },
    data() {
        return {
            isLogged: isLoggedIn(),

            socket: {} as WebSocketService,
            heartInterval: 0,
            dataAgeTimers: {} as Record<string, number>,
            dataLoading: true,
            liveData: {} as LiveData,
            isFirstFetchAfterConnect: true,
            eventLogView: {} as bootstrap.Modal,
            eventLogList: {} as EventlogItems,
            eventLogLoading: true,
            devInfoView: {} as bootstrap.Modal,
            devInfoList: {} as DevInfoStatus,
            devInfoLoading: true,
            gridProfileView: {} as bootstrap.Modal,
            gridProfileList: {} as GridProfileStatus,
            gridProfileRawList: {} as GridProfileRawdata,
            gridProfileLoading: true,

            limitSettingView: {} as bootstrap.Modal,
            limitSettingLoading: true,

            currentLimitList: {} as LimitStatus,
            targetLimitList: {} as LimitConfig,

            targetLimitMin: 0,
            targetLimitMax: 100,
            targetLimitTypeText: this.$t('home.Relative'),
            targetLimitRelative: true,

            alertMessageLimit: '',
            alertTypeLimit: 'info',
            showAlertLimit: false,
            performRadioStatsReset: false,

            powerSettingView: {} as bootstrap.Modal,
            powerSettingSerial: '',
            powerSettingLoading: true,
            alertMessagePower: '',
            alertTypePower: 'info',
            showAlertPower: false,
            successCommandPower: '',

            isWebsocketConnected: false,
        };
    },
    created() {
        this.getInitialData();
        this.initSocket();
        this.$emitter.on('logged-in', () => {
            this.isLogged = this.isLoggedIn();
        });
        this.$emitter.on('logged-out', () => {
            this.isLogged = this.isLoggedIn();
        });
    },
    mounted() {
        this.eventLogView = new bootstrap.Modal('#eventView');
        this.devInfoView = new bootstrap.Modal('#devInfoView');
        this.gridProfileView = new bootstrap.Modal('#gridProfileView');
        this.limitSettingView = new bootstrap.Modal('#limitSettingView');
        this.powerSettingView = new bootstrap.Modal('#powerSettingView');
    },
    unmounted() {
        this.socket?.close();
    },
    updated() {
        console.log('Updated');
        // Select first tab
        if (this.isFirstFetchAfterConnect) {
            console.log('isFirstFetchAfterConnect');

            this.$nextTick(() => {
                console.log('nextTick');
                const firstTabEl = document.querySelector('#v-pills-tab:first-child button');
                if (firstTabEl != null) {
                    this.isFirstFetchAfterConnect = false;
                    console.log('Show');
                    const firstTab = new bootstrap.Tab(firstTabEl);
                    firstTab.show();
                }
            });
        }
    },
    computed: {
        currentLimitAbsolute(): string {
            if (this.currentLimitList.max_power > 0) {
                return this.$n(
                    (this.currentLimitList.limit_relative * this.currentLimitList.max_power) / 100,
                    'decimalNoDigits'
                );
            }
            return '0';
        },
        currentLimitRelative(): string {
            return this.$n(this.currentLimitList.limit_relative, 'decimalOneDigit');
        },
        inverterData(): Inverter[] {
            return this.liveData.inverters.slice().sort((a: Inverter, b: Inverter) => {
                return a.order - b.order;
            });
        },
        hasInverters(): boolean {
            return this.liveData?.inverters?.length > 0 || false;
        },
    },
    methods: {
        isLoggedIn,
        getInitialData(triggerLoading: boolean = true) {
            if (triggerLoading) {
                this.dataLoading = true;
            }
            fetch('/api/livedata/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.liveData = data;
                    if (triggerLoading) {
                        this.dataLoading = false;
                    }
                });
        },
        reloadData() {
            this.socket?.close();

            this.getInitialData(false);
            this.initSocket();
        },
        handleMessage(event: MessageEvent) {
            if (!event.data || event.data === '{}') {
                this.socket?.close(); // force reconnect
                this.initSocket();
                return;
            }

            const newData = JSON.parse(event.data);

            if (typeof newData.solarcharger !== 'undefined') {
                Object.assign(this.liveData.solarcharger, newData.solarcharger);
            }
            if (typeof newData.gridcharger !== 'undefined') {
                Object.assign(this.liveData.gridcharger, newData.gridcharger);
            }
            if (typeof newData.battery !== 'undefined') {
                Object.assign(this.liveData.battery, newData.battery);
            }
            if (typeof newData.power_meter !== 'undefined') {
                Object.assign(this.liveData.power_meter, newData.power_meter);
            }

            if (typeof newData.total === 'undefined') {
                return;
            }

            Object.assign(this.liveData.total, newData.total);
            Object.assign(this.liveData.hints, newData.hints);

            const idx = this.liveData.inverters.findIndex((i) => i.serial === newData.inverters[0].serial);

            if (idx == -1) {
                Object.assign(this.liveData.inverters, newData.inverters);
                this.liveData.inverters.forEach((inv) => this.resetDataAging(inv));
            } else if (this.liveData.inverters[idx]) {
                Object.assign(this.liveData.inverters[idx], newData.inverters[0]);
                this.resetDataAging(this.liveData.inverters[idx]);
            }
        },
        initSocket() {
            console.log('Starting connection to WebSocket Server');

            const { protocol, host } = location;
            const authString = authUrl();
            const webSocketUrl = `${protocol === 'https:' ? 'wss' : 'ws'}://${authString}${host}/livedata`;

            this.socket = new WebSocketService(webSocketUrl, {
                onMessage: this.handleMessage,
                onOpen: () => {
                    console.log('WebSocket connected');
                    this.isWebsocketConnected = true;
                },
                onClose: () => {
                    console.log('WebSocket closed');
                    this.isWebsocketConnected = false;
                },
            });

            // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
            window.onbeforeunload = () => {
                this.socket?.close();
            };

            this.socket?.connect();
        },
        resetDataAging(inv: Inverter) {
            if (this.dataAgeTimers[inv.serial] !== undefined) {
                clearTimeout(this.dataAgeTimers[inv.serial]);
            }

            const nextMs = 1000 - (inv.data_age_ms % 1000);
            this.dataAgeTimers[inv.serial] = setTimeout(() => {
                this.doDataAging(inv.serial);
            }, nextMs);
        },
        doDataAging(serial: string) {
            const inv = this.liveData?.inverters?.find((inv) => inv.serial === serial);
            if (inv === undefined) {
                return;
            }

            inv.data_age_ms += 1000;

            this.dataAgeTimers[serial] = setTimeout(() => {
                this.doDataAging(serial);
            }, 1000);
        },
        onShowEventlog(serial: string) {
            this.eventLogLoading = true;
            fetch('/api/eventlog/status?inv=' + serial + '&locale=' + this.$i18n.locale, {
                headers: authHeader(),
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.eventLogList = data;
                    this.eventLogLoading = false;
                });

            this.eventLogView.show();
        },
        onShowDevInfo(serial: string) {
            this.devInfoLoading = true;
            fetch('/api/devinfo/status?inv=' + serial, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.devInfoList = data;
                    this.devInfoList.serial = serial;
                    this.devInfoLoading = false;
                });

            this.devInfoView.show();
        },
        onShowGridProfile(serial: string) {
            this.gridProfileLoading = true;
            fetch('/api/gridprofile/status?inv=' + serial, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.gridProfileList = data;

                    fetch('/api/gridprofile/rawdata?inv=' + serial, { headers: authHeader() })
                        .then((response) => handleResponse(response, this.$emitter, this.$router))
                        .then((data) => {
                            this.gridProfileRawList = data;
                            this.gridProfileLoading = false;
                        });
                });

            this.gridProfileView.show();
        },
        onShowLimitSettings(serial: string) {
            this.showAlertLimit = false;
            this.targetLimitList.serial = '';
            this.targetLimitList.limit_value = 0;
            this.onSelectType(true);

            this.limitSettingLoading = true;
            fetch('/api/limit/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.currentLimitList = data[serial];
                    this.targetLimitList.serial = serial;
                    this.limitSettingLoading = false;
                });

            this.limitSettingView.show();
        },
        onResetRadioStats(serial: string) {
            this.performRadioStatsReset = true;
            fetch('/api/inverter/stats_reset?inv=' + serial, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(() => {
                    this.performRadioStatsReset = false;
                });
        },
        onSetLimitSettings(setPersistent: boolean) {
            if (setPersistent) {
                if (this.targetLimitRelative) {
                    this.targetLimitList.limit_type = LimitType.RelativPersistent;
                } else {
                    this.targetLimitList.limit_type = LimitType.AbsolutPersistent;
                }
            } else {
                if (this.targetLimitRelative) {
                    this.targetLimitList.limit_type = LimitType.RelativNonPersistent;
                } else {
                    this.targetLimitList.limit_type = LimitType.AbsolutNonPersistent;
                }
            }
            const formData = new FormData();
            formData.append('data', JSON.stringify(this.targetLimitList));

            console.log(this.targetLimitList);

            fetch('/api/limit/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    if (response.type == 'success') {
                        this.limitSettingView.hide();
                    } else {
                        this.alertMessageLimit = this.$t('apiresponse.' + response.code, response.param);
                        this.alertTypeLimit = response.type;
                        this.showAlertLimit = true;
                    }
                });
        },
        onSelectType(isRelative: boolean) {
            if (isRelative) {
                this.targetLimitTypeText = this.$t('home.Relative');
                this.targetLimitMin = 0;
                this.targetLimitMax = 100;
            } else {
                this.targetLimitTypeText = this.$t('home.Absolute');
                this.targetLimitMin = 0;
                this.targetLimitMax = this.currentLimitList.max_power > 0 ? this.currentLimitList.max_power : 2250;
            }
            this.targetLimitRelative = isRelative;
        },

        onShowPowerSettings(serial: string) {
            this.showAlertPower = false;
            this.powerSettingSerial = '';
            this.powerSettingLoading = true;
            fetch('/api/power/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.successCommandPower = data[serial].power_set_status;
                    this.powerSettingSerial = serial;
                    this.powerSettingLoading = false;
                });
            this.powerSettingView.show();
        },

        onSetPowerSettings(turnOn: boolean, restart = false) {
            const data = restart
                ? {
                      serial: this.powerSettingSerial,
                      restart: true,
                  }
                : {
                      serial: this.powerSettingSerial,
                      power: turnOn,
                  };

            const formData = new FormData();
            formData.append('data', JSON.stringify(data));

            console.log(data);

            fetch('/api/power/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    if (response.type == 'success') {
                        this.powerSettingView.hide();
                    } else {
                        this.alertMessagePower = this.$t('apiresponse.' + response.code, response.param);
                        this.alertTypePower = response.type;
                        this.showAlertPower = true;
                    }
                });
        },
        getSumIrridiation(inv: Inverter): number {
            let total = 0;
            Object.keys(inv.DC).forEach((key) => {
                total += inv.DC[key as unknown as number]?.Irradiation?.max || 0;
            });
            return total;
        },
        ratio(val_small: number, val_large: number): string {
            if (val_large == 0) {
                return '-';
            }
            return this.$n(val_small / val_large, 'percent');
        },
    },
});
</script>

<style scoped>
.inverter-workspace {
    align-items: flex-start;
}

.inverter-nav {
    display: flex;
    flex-direction: column;
    position: sticky;
    top: 5.25rem;
}

.inverter-nav-item {
    width: 100%;
    min-height: 3.25rem;
    padding: 0.65rem 0.75rem;
    color: var(--bs-body-color);
    background: var(--bs-tertiary-bg);
    border: 1px solid var(--bs-border-color);
    border-radius: 0.75rem;
    text-align: left;
}

.inverter-nav-item:hover {
    border-color: rgba(var(--bs-primary-rgb), 0.55);
    background: var(--bs-secondary-bg);
}

.inverter-nav-item.active {
    color: var(--bs-body-color);
    background: rgba(var(--bs-primary-rgb), 0.12);
    border-color: var(--bs-primary);
}

.inverter-power-badge {
    min-width: 3.75rem;
    padding: 0.45rem 0.5rem;
    font-variant-numeric: tabular-nums;
}

.inverter-nav-name {
    min-width: 0;
    font-weight: 600;
    line-height: 1.2;
}

.inverter-panel {
    overflow: hidden;
    border-color: var(--bs-border-color);
    border-radius: 0.9rem;
}

.inverter-header {
    position: relative;
    padding: 1rem 1.1rem;
    color: var(--bs-body-color);
    background: var(--bs-tertiary-bg);
    border-bottom: 1px solid var(--bs-border-color);
    border-left: 0.35rem solid var(--inverter-status-color, var(--bs-secondary));
}

.inverter-status-disabled {
    --inverter-status-color: var(--bs-secondary);
}

.inverter-status-error {
    --inverter-status-color: var(--bs-danger);
}

.inverter-status-idle {
    --inverter-status-color: var(--bs-warning);
}

.inverter-status-producing {
    --inverter-status-color: var(--bs-success);
}

.inverter-identity {
    min-width: 16rem;
}

.inverter-title-row {
    display: flex;
    align-items: center;
    gap: 0.65rem;
}

.inverter-status-dot {
    width: 0.65rem;
    height: 0.65rem;
    flex: 0 0 auto;
    background: var(--inverter-status-color, var(--bs-secondary));
    border-radius: 50%;
}

.inverter-title {
    font-size: 1.05rem;
}

.inverter-live-power {
    padding: 0.25rem 0.55rem;
    color: var(--bs-emphasis-color);
    background: var(--bs-body-bg);
    border: 1px solid var(--bs-border-color);
    border-radius: 999px;
    font-size: 0.82rem;
    font-weight: 700;
    font-variant-numeric: tabular-nums;
}

.inverter-meta {
    display: flex;
    flex-wrap: wrap;
    gap: 0.25rem 1.15rem;
    margin-top: 0.4rem;
    color: var(--bs-secondary-color);
    font-size: 0.82rem;
}

.inverter-actions {
    display: flex;
    flex-wrap: nowrap;
    gap: 0.45rem;
}

.inverter-action {
    display: inline-flex;
    width: 2.45rem;
    height: 2.45rem;
    align-items: center;
    justify-content: center;
    border-radius: 0.65rem !important;
    font-size: 1.15rem;
}

.inverter-body {
    padding: 1rem;
}

.channel-grid > .col {
    min-width: 13rem;
}

.radio-stats .accordion-item {
    overflow: hidden;
    border-radius: 0.75rem;
}

.radio-stats .accordion-button {
    padding: 0.85rem 1rem;
    font-size: 0.9rem;
    font-weight: 600;
}

@media (max-width: 991.98px) {
    .inverter-nav {
        flex-direction: row;
        position: static;
        flex-wrap: nowrap;
        overflow-x: auto;
        padding-bottom: 0.25rem;
        scrollbar-width: thin;
    }

    .inverter-nav-item {
        width: auto;
        min-width: 10rem;
    }
}

@media (max-width: 575.98px) {
    .inverter-header {
        align-items: stretch !important;
    }

    .inverter-identity {
        min-width: 100%;
    }

    .inverter-actions {
        width: 100%;
        justify-content: space-between;
    }

    .inverter-action {
        width: 2.65rem;
        height: 2.65rem;
    }
}
</style>
