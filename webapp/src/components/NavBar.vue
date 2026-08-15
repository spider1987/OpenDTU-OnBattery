<template>
    <nav class="navbar navbar-expand-md fixed-top app-navbar bg-body-tertiary">
        <div class="container-fluid">
            <router-link @click="onClick" class="navbar-brand app-brand" to="/">
                <BIconTree v-if="isXmas" width="30" height="30" class="d-inline-block align-text-top text-success" />

                <BIconEgg v-else-if="isEaster" width="30" height="30" class="d-inline-block align-text-top text-info" />

                <BIconSun v-else width="30" height="30" class="d-inline-block align-text-top text-warning" />

                <span class="app-brand-name">OpenDTU-OnBattery</span>
                <span class="text-info mx-2"
                    ><BIconBatteryCharging width="20" height="20" class="d-inline-block align-text-center"
                /></span>
            </router-link>
            <button
                class="navbar-toggler"
                type="button"
                data-bs-toggle="collapse"
                data-bs-target="#navbarNavAltMarkup"
                aria-controls="navbarNavAltMarkup"
                aria-expanded="false"
                aria-label="Toggle navigation"
            >
                <span class="navbar-toggler-icon"></span>
            </button>
            <div class="collapse navbar-collapse" ref="navbarCollapse" id="navbarNavAltMarkup">
                <ul class="navbar-nav navbar-nav-scroll d-flex me-auto flex-sm-fill">
                    <li class="nav-item">
                        <router-link @click="onClick" class="nav-link" to="/">{{ $t('menu.LiveView') }}</router-link>
                    </li>
                    <li class="nav-item dropdown">
                        <a
                            class="nav-link dropdown-toggle"
                            :class="{ active: isSettingsRoute }"
                            href="#"
                            id="settingsDropdown"
                            ref="settingsDropdownToggle"
                            role="button"
                            data-bs-toggle="dropdown"
                            aria-expanded="false"
                        >
                            {{ $t('menu.Settings') }}
                        </a>
                        <ul class="dropdown-menu app-dropdown settings-menu" aria-labelledby="settingsDropdown">
                            <li class="menu-section-title">{{ $t('menu.ConnectivityGroup') }}</li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/network">{{
                                    $t('menu.NetworkSettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/ntp">{{
                                    $t('menu.NTPSettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/mqtt">{{
                                    $t('menu.MQTTSettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/security"
                                    >{{ $t('menu.SecuritySettings') }}
                                </router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/logging"
                                    >{{ $t('menu.LoggingSettings') }}
                                </router-link>
                            </li>
                            <li class="menu-section-title">{{ $t('menu.EnergyGroup') }}</li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/inverter"
                                    >{{ $t('menu.InverterSettings') }}
                                </router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/dtu">{{
                                    $t('menu.DTUSettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/solarcharger">{{
                                    $t('menu.SolarChargerSettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/powermeter">{{
                                    $t('menu.PowerMeterSettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/powerhistory">{{
                                    $t('menu.PowerHistorySettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/powerlimiter"
                                    >{{ $t('menu.PowerLimiterSettings') }}</router-link
                                >
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/battery">{{
                                    $t('menu.BatterySettings')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/chargerac">{{
                                    $t('menu.AcChargerSettings')
                                }}</router-link>
                            </li>
                            <li class="menu-section-title">{{ $t('menu.SystemGroup') }}</li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/device">{{
                                    $t('menu.DeviceManager')
                                }}</router-link>
                            </li>
                            <li class="menu-section-title">{{ $t('menu.MaintenanceGroup') }}</li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/settings/config">{{
                                    $t('menu.ConfigManagement')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/firmware/upgrade">{{
                                    $t('menu.FirmwareUpgrade')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/maintenance/reboot">{{
                                    $t('menu.DeviceReboot')
                                }}</router-link>
                            </li>
                        </ul>
                    </li>
                    <li class="nav-item dropdown">
                        <a
                            class="nav-link dropdown-toggle"
                            :class="{ active: isInfoRoute }"
                            href="#"
                            id="infoDropdown"
                            ref="infoDropdownToggle"
                            role="button"
                            data-bs-toggle="dropdown"
                            aria-expanded="false"
                        >
                            {{ $t('menu.Info') }}
                        </a>
                        <ul class="dropdown-menu app-dropdown info-menu" aria-labelledby="infoDropdown">
                            <li class="menu-section-title">{{ $t('menu.StatusGroup') }}</li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/info/system">{{
                                    $t('menu.System')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/info/network">{{
                                    $t('menu.Network')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/info/ntp">{{
                                    $t('menu.NTP')
                                }}</router-link>
                            </li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/info/mqtt">{{
                                    $t('menu.MQTT')
                                }}</router-link>
                            </li>
                            <li class="menu-section-title">{{ $t('menu.DiagnosticsGroup') }}</li>
                            <li>
                                <router-link @click="onClick" class="dropdown-item" to="/info/console">{{
                                    $t('menu.Console')
                                }}</router-link>
                            </li>
                        </ul>
                    </li>
                    <li class="nav-item">
                        <router-link @click="onClick" class="nav-link" to="/about">{{ $t('menu.About') }}</router-link>
                    </li>
                    <li class="flex-sm-fill"></li>
                    <ThemeSwitcher class="me-2" />
                    <form class="d-flex" role="search">
                        <LocaleSwitcher class="me-2" />
                        <button v-if="isLogged" class="btn btn-outline-danger" @click="signout">
                            {{ $t('menu.Logout') }}
                        </button>
                        <button v-if="!isLogged" class="btn btn-outline-success" @click="signin">
                            {{ $t('menu.Login') }}
                        </button>
                    </form>
                </ul>
            </div>
        </div>
    </nav>
</template>

<script lang="ts">
import { isLoggedIn, logout } from '@/utils/authentication';
import { BIconEgg, BIconSun, BIconTree, BIconBatteryCharging } from 'bootstrap-icons-vue';
import { Dropdown } from 'bootstrap';
import { defineComponent } from 'vue';
import LocaleSwitcher from './LocaleSwitcher.vue';
import ThemeSwitcher from './ThemeSwitcher.vue';

export default defineComponent({
    components: {
        BIconEgg,
        BIconSun,
        BIconTree,
        BIconBatteryCharging,
        LocaleSwitcher,
        ThemeSwitcher,
    },
    data() {
        return {
            isLogged: isLoggedIn(),
            now: {} as Date,
            clockInterval: 0,
        };
    },
    created() {
        this.$emitter.on('logged-in', this.updateLoginState);
        this.$emitter.on('logged-out', this.updateLoginState);

        this.now = new Date();
        this.clockInterval = setInterval(() => {
            this.now = new Date();
        }, 10000);
    },
    unmounted() {
        this.$emitter.off('logged-in', this.updateLoginState);
        this.$emitter.off('logged-out', this.updateLoginState);
        clearInterval(this.clockInterval);
    },
    computed: {
        isSettingsRoute() {
            return (
                this.$route.path.startsWith('/settings/') ||
                this.$route.path.startsWith('/firmware/') ||
                this.$route.path.startsWith('/maintenance/')
            );
        },
        isInfoRoute() {
            return this.$route.path.startsWith('/info/');
        },
        isXmas() {
            return this.now.getMonth() + 1 == 12 && this.now.getDate() >= 24 && this.now.getDate() <= 26;
        },
        isEaster() {
            const easter = this.getEasterSunday(this.now.getFullYear());
            const easterStart = new Date(easter);
            const easterEnd = new Date(easter);
            easterStart.setDate(easterStart.getDate() - 2);
            easterEnd.setDate(easterEnd.getDate() + 1);
            return this.now >= easterStart && this.now < easterEnd;
        },
    },
    methods: {
        isLoggedIn,
        logout,
        updateLoginState() {
            this.isLogged = this.isLoggedIn();
        },
        signin(e: Event) {
            e.preventDefault();
            this.$router.push('/login');
        },
        signout(e: Event) {
            e.preventDefault();
            this.logout();
            this.$emitter.emit('logged-out');
            this.$router.push('/');
        },
        onClick() {
            const dropdownToggles = [this.$refs.settingsDropdownToggle, this.$refs.infoDropdownToggle];
            dropdownToggles.forEach((toggle) => {
                if (toggle instanceof HTMLElement) {
                    Dropdown.getInstance(toggle)?.hide();
                }
            });

            if (this.$refs.navbarCollapse) {
                (this.$refs.navbarCollapse as HTMLElement).classList.remove('show');
            }
        },
        getEasterSunday(year: number): Date {
            const f = Math.floor;
            const G = year % 19;
            const C = f(year / 100);
            const H = (C - f(C / 4) - f((8 * C + 13) / 25) + 19 * G + 15) % 30;
            const I = H - f(H / 28) * (1 - f(29 / (H + 1)) * f((21 - G) / 11));
            const J = (year + f(year / 4) + I + 2 - C + f(C / 4)) % 7;
            const L = I - J;
            const month = 3 + f((L + 40) / 44);
            const day = L + 28 - 31 * f(month / 4);

            return new Date(year, month - 1, day);
        },
    },
});
</script>

<style scoped>
.app-navbar {
    min-height: 3.75rem;
    border-bottom: 1px solid var(--bs-border-color-translucent);
}

.app-brand {
    display: flex;
    height: 2rem;
    align-items: center;
    padding: 0;
}

.app-brand-name {
    margin-left: 0.55rem;
    font-weight: 650;
    letter-spacing: -0.015em;
}

.nav-link {
    border-radius: 0.45rem;
}

.nav-link.router-link-active:not(.dropdown-toggle) {
    color: var(--bs-navbar-active-color);
    background: rgba(255, 255, 255, 0.07);
}

.nav-link.active {
    color: var(--bs-navbar-active-color);
    background: rgba(var(--bs-primary-rgb), 0.1);
}

.app-dropdown {
    margin-top: 0.55rem !important;
    padding: 0.65rem;
    border: 1px solid var(--bs-border-color);
    border-radius: 0.85rem;
    background: var(--bs-body-bg);
    box-shadow: 0 1rem 2.5rem rgba(0, 0, 0, 0.22);
}

.settings-menu {
    width: min(44rem, calc(100vw - 2rem));
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 0.2rem 0.65rem;
}

.settings-menu.show {
    display: grid;
}

.info-menu {
    width: min(22rem, calc(100vw - 2rem));
}

.menu-section-title {
    grid-column: 1 / -1;
    margin: 0.35rem 0.35rem 0.15rem;
    padding: 0.35rem 0.5rem;
    color: var(--bs-secondary-color);
    border-bottom: 1px solid var(--bs-border-color-translucent);
    font-size: 0.72rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
}

.menu-section-title:first-child {
    margin-top: 0;
}

.app-dropdown .dropdown-item {
    position: relative;
    min-height: 2.45rem;
    display: flex;
    align-items: center;
    padding: 0.55rem 0.75rem 0.55rem 1.65rem;
    border-radius: 0.55rem;
    color: var(--bs-body-color);
    font-weight: 550;
}

.app-dropdown .dropdown-item::before {
    position: absolute;
    left: 0.75rem;
    width: 0.38rem;
    height: 0.38rem;
    content: '';
    border-radius: 50%;
    background: var(--bs-primary);
    opacity: 0.75;
}

.app-dropdown .dropdown-item:hover,
.app-dropdown .dropdown-item:focus,
.app-dropdown .dropdown-item.router-link-active {
    color: var(--bs-emphasis-color);
    background: rgba(var(--bs-primary-rgb), 0.12);
}

.app-dropdown .dropdown-item.router-link-active::before {
    opacity: 1;
    box-shadow: 0 0 0 0.24rem rgba(var(--bs-primary-rgb), 0.14);
}

@media (max-width: 767.98px) {
    .app-dropdown,
    .settings-menu,
    .info-menu {
        width: 100%;
        max-height: 65vh;
        overflow-y: auto;
        box-shadow: none;
    }

    .settings-menu.show {
        display: grid;
        grid-template-columns: 1fr;
    }
}
</style>
