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
                <button type="button" class="dropdown-item d-flex align-items-center" data-bs-theme-value="dark">
                    <BIconMoonStarsFill class="bi me-2 opacity-50 theme-icon" />
                    {{ $t('localeswitcher.Dark') }}
                </button>
            </li>
            <li>
                <button type="button" class="dropdown-item d-flex align-items-center" data-bs-theme-value="oled-night">
                    <BIconMoonFill class="bi me-2 opacity-75 theme-icon text-info" />
                    <span>
                        <span class="d-block">{{ $t('localeswitcher.OledNight') }}</span>
                        <small>{{ $t('localeswitcher.OledNightDescription') }}</small>
                    </span>
                </button>
            </li>
        </ul>
    </li>
</template>

<script lang="ts">
import { BIconCircleHalf, BIconMoonFill, BIconMoonStarsFill } from 'bootstrap-icons-vue';
import { defineComponent } from 'vue';

export default defineComponent({
    name: 'ThemeSwitcher',
    components: { BIconCircleHalf, BIconMoonFill, BIconMoonStarsFill },
    data() {
        return { storedTheme: 'dark' };
    },
    methods: {
        setTheme(theme: string) {
            document.documentElement.setAttribute('data-bs-theme', theme === 'oled-night' ? 'oled-night' : 'dark');
        },
        showActiveTheme(theme: string) {
            const activeThemeIcon = document.querySelector('.theme-icon-active');
            const btnToActive = document.querySelector('[data-bs-theme-value="' + theme + '"]');
            const svgOfActiveBtn = btnToActive?.querySelector('.theme-icon');
            document.querySelectorAll('[data-bs-theme-value]').forEach((element) => element.classList.remove('active'));
            btnToActive?.classList.add('active');
            if (svgOfActiveBtn) {
                activeThemeIcon?.replaceChildren('*', svgOfActiveBtn.cloneNode(true));
            }
        },
    },
    mounted() {
        this.storedTheme = localStorage.getItem('theme') === 'oled-night' ? 'oled-night' : 'dark';
        localStorage.setItem('theme', this.storedTheme);
        this.setTheme(this.storedTheme);
        this.showActiveTheme(this.storedTheme);
        document.querySelectorAll('[data-bs-theme-value]').forEach((toggle) => {
            toggle.addEventListener('click', () => {
                const theme = toggle.getAttribute('data-bs-theme-value') === 'oled-night' ? 'oled-night' : 'dark';
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
    width: min(20rem, calc(100vw - 1rem));
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
</style>
