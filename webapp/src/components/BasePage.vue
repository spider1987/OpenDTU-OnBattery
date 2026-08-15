<template>
    <div :class="{ 'container-xxl': !isWideScreen, 'container-fluid': isWideScreen }" role="main">
        <div class="page-header">
            <div class="row mb-3 align-items-center">
                <div :class="'align-content-center ' + (showReload ? 'col-10' : 'col-12')">
                    <h1 class="mb-0 page-title">
                        {{ title }}
                        <span
                            v-if="showWebSocket"
                            :class="{
                                onlineMarker: isWebsocketConnected,
                                offlineMarker: !isWebsocketConnected,
                            }"
                        ></span>
                    </h1>
                </div>
                <div class="col-2 align-content-center" v-if="showReload">
                    <button
                        type="button"
                        class="float-end btn btn-outline-primary fs-5"
                        @click="$emit('reload')"
                        v-tooltip
                        :title="$t('base.Reload')"
                    >
                        <BIconArrowClockwise />
                    </button>
                </div>
            </div>
        </div>

        <div class="text-center" v-if="isLoading">
            <div class="spinner-border" role="status">
                <span class="visually-hidden">{{ $t('base.Loading') }}</span>
            </div>
        </div>

        <template v-if="!isLoading">
            <slot />
        </template>
    </div>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import { BIconArrowClockwise } from 'bootstrap-icons-vue';
import PullToRefresh from 'pulltorefreshjs';

export default defineComponent({
    components: {
        BIconArrowClockwise,
    },
    props: {
        title: { type: String, required: true },
        isLoading: { type: Boolean, required: false, default: false },
        isWideScreen: { type: Boolean, required: false, default: false },
        isWebsocketConnected: { type: Boolean, required: false, default: false },
        showWebSocket: { type: Boolean, required: false, default: false },
        showReload: { type: Boolean, required: false, default: false },
    },
    mounted() {
        console.log('init');
        PullToRefresh.init({
            mainElement: 'body', // above which element?
            instructionsPullToRefresh: this.$t('base.Pull'),
            instructionsReleaseToRefresh: this.$t('base.Release'),
            instructionsRefreshing: this.$t('base.Refreshing'),
            onRefresh: () => {
                this.$emit('reload');
            },
        });
    },
    unmounted() {
        console.log('destroy');
        PullToRefresh.destroyAll();
    },
});
</script>

<style scoped>
.ptr--text {
    color: var(--bs-primary-text-emphasis) !important;
}
.ptr--icon {
    color: var(--bs-primary-text-emphasis) !important;
}

.offlineMarker:before {
    content: '';
    position: absolute;
    width: 9px;
    height: 9px;
    background: var(--bs-danger);
    border-radius: 50%;
}
.onlineMarker:before {
    content: '';
    position: absolute;
    width: 9px;
    height: 9px;
    background: var(--bs-success);
    border-radius: 50%;
}
.page-header {
    padding-top: 0.25rem;
}

.page-title {
    color: var(--bs-emphasis-color);
    font-size: clamp(1.75rem, 3vw, 2.4rem);
    font-weight: 650;
    letter-spacing: -0.025em;
}
</style>
