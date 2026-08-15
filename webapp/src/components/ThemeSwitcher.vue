<template>
    <li class="nav-item dropdown">
        <button
            class="btn btn-link nav-link py-2 px-0 px-lg-2 dropdown-toggle d-flex align-items-center"
            id="bd-theme"
            type="button"
            aria-expanded="false"
            data-bs-toggle="dropdown"
            data-bs-display="static"
            :aria-label="$t('localeswitcher.ToggleTheme')"
        >
            <BIconCircleHalf class="bi my-1 theme-icon-active" />
        </button>
        <ul class="dropdown-menu dropdown-menu-end theme-menu">
            <li>
                <button
                    type="button"
                    class="dropdown-item d-flex align-items-center"
                    data-bs-theme-value="light"
                    aria-pressed="false"
                >
                    <BIconSunFill class="bi me-2 opacity-50 theme-icon" />
                    {{ $t('localeswitcher.Light') }}
                </button>
            </li>
            <li>
                <button
                    type="button"
                    class="dropdown-item d-flex align-items-center"
                    data-bs-theme-value="dark"
                    aria-pressed="false"
                >
                    <BIconMoonStarsFill class="bi me-2 opacity-50 theme-icon" />
                    {{ $t('localeswitcher.Dark') }}
                </button>
            </li>
            <li>
                <button
                    type="button"
                    class="dropdown-item d-flex align-items-center"
                    data-bs-theme-value="oled-night"
                    aria-pressed="false"
                >
                    <BIconMoonFill class="bi me-2 opacity-75 theme-icon text-info" />
                    <span>
                        <span class="d-block">{{ $t('localeswitcher.OledNight') }}</span>
                        <small>{{ $t('localeswitcher.OledNightDescription') }}</small>
                    </span>
                </button>
            </li>
            <li>
                <button
                    type="button"
                    class="dropdown-item d-flex align-items-center"
                    data-bs-theme-value="cyberpunk"
                    aria-pressed="false"
                >
                    <BIconLightningChargeFill class="bi me-2 opacity-75 theme-icon text-warning" />
                    <span>
                        <span class="d-block">{{ $t('localeswitcher.Cyberpunk') }}</span>
                        <small>{{ $t('localeswitcher.CyberpunkDescription') }}</small>
                    </span>
                </button>
            </li>
            <li v-for="theme in additionalThemes" :key="theme.value">
                <button
                    type="button"
                    class="dropdown-item d-flex align-items-center"
                    :data-bs-theme-value="theme.value"
                    aria-pressed="false"
                >
                    <BIconCircleFill
                        class="bi me-2 opacity-75 theme-icon theme-color-dot"
                        :style="{ color: theme.color }"
                    />
                    <span>
                        <span class="d-block">{{ $t(`localeswitcher.${theme.label}`) }}</span>
                        <small>{{ $t(`localeswitcher.${theme.description}`) }}</small>
                    </span>
                </button>
            </li>
            <li><hr class="dropdown-divider" /></li>
            <li>
                <button
                    type="button"
                    class="dropdown-item d-flex align-items-center active"
                    data-bs-theme-value="auto"
                    aria-pressed="true"
                >
                    <BIconCircleHalf class="bi me-2 opacity-50 theme-icon" />
                    {{ $t('localeswitcher.Auto') }}
                </button>
            </li>
        </ul>
    </li>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import {
    BIconCircleFill,
    BIconCircleHalf,
    BIconLightningChargeFill,
    BIconMoonFill,
    BIconMoonStarsFill,
    BIconSunFill,
} from 'bootstrap-icons-vue';

export default defineComponent({
    name: 'ThemeSwitcher',
    components: {
        BIconCircleFill,
        BIconCircleHalf,
        BIconLightningChargeFill,
        BIconMoonFill,
        BIconSunFill,
        BIconMoonStarsFill,
    },
    data() {
        return {
            storedTheme: 'auto',
            additionalThemes: [
                { value: 'solar-command', label: 'SolarCommand', description: 'SolarCommandDescription', color: '#d4a72c' },
                { value: 'industrial', label: 'Industrial', description: 'IndustrialDescription', color: '#f7c600' },
                { value: 'minimal-light', label: 'MinimalLight', description: 'MinimalLightDescription', color: '#3282b8' },
                { value: 'nature-energy', label: 'NatureEnergy', description: 'NatureEnergyDescription', color: '#79c267' },
                { value: 'holographic', label: 'Holographic', description: 'HolographicDescription', color: '#9b7cff' },
                { value: 'retro-terminal', label: 'RetroTerminal', description: 'RetroTerminalDescription', color: '#65ff8f' },
            ],
        };
    },
    methods: {
        getPreferredTheme() {
            if (this.storedTheme) {
                return this.storedTheme;
            }
            return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
        },
        setTheme(theme: string) {
            const resolvedTheme =
                theme === 'auto'
                    ? window.matchMedia('(prefers-color-scheme: dark)').matches
                        ? 'dark'
                        : 'light'
                    : theme;
            document.documentElement.setAttribute('data-bs-theme', resolvedTheme);
        },
        showActiveTheme(theme: string) {
            const activeThemeIcon = document.querySelector('.theme-icon-active');
            const btnToActive = document.querySelector(`[data-bs-theme-value="${theme}"]`);
            const svgOfActiveBtn = btnToActive?.querySelector('.theme-icon');

            document.querySelectorAll('[data-bs-theme-value]').forEach((element) => {
                element.classList.remove('active');
            });

            btnToActive?.classList.add('active');

            if (svgOfActiveBtn) {
                activeThemeIcon?.replaceChildren('*', svgOfActiveBtn?.cloneNode(true));
            }
        },
    },
    mounted() {
        this.storedTheme = localStorage.getItem('theme') || 'auto';
        this.setTheme(this.getPreferredTheme());
        this.showActiveTheme(this.getPreferredTheme());

        window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => {
            if (this.storedTheme === 'auto') {
                this.setTheme(this.getPreferredTheme());
            }
        });

        document.querySelectorAll('[data-bs-theme-value]').forEach((toggle) => {
            toggle.addEventListener('click', () => {
                const theme = toggle.getAttribute('data-bs-theme-value') || 'auto';
                localStorage.setItem('theme', theme);
                this.storedTheme = theme;
                this.setTheme(theme);
                this.showActiveTheme(theme);
            });
        });
    },
});
</script>

<style scoped>
.theme-menu {
    width: min(23rem, calc(100vw - 1rem));
    max-height: calc(100vh - 5rem);
    overflow-y: auto;
}

.theme-menu .dropdown-item {
    min-height: 2.5rem;
    white-space: normal;
}

.theme-menu small {
    display: block;
    color: var(--bs-secondary-color);
    font-size: 0.7rem;
    line-height: 1.2;
}

.theme-color-dot {
    flex: 0 0 auto;
}
</style>
