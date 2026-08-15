<template>
    <div
        class="card card-table channel-card"
        :class="{
            'border-info': channelType == 'AC',
            'border-secondary': channelType == 'INV',
        }"
    >
        <div v-if="channelType == 'INV'" class="card-header channel-header channel-header-general">
            {{ $t('inverterchannelinfo.General') }}
        </div>

        <div v-if="channelType == 'DC'" class="card-header channel-header">
            <template v-if="channelData.name.u != ''">{{ channelData.name.u }}</template>
            <template v-else>{{ $t('inverterchannelinfo.String', { num: channelNumber + 1 }) }}</template>
        </div>

        <div v-if="channelType == 'AC'" class="card-header channel-header channel-header-ac">
            {{ $t('inverterchannelinfo.Phase', { num: channelNumber + 1 }) }}
        </div>

        <div class="table-responsive">
            <table class="table channel-table">
                <tbody>
                    <tr v-for="(property, key) in channelData" :key="`prop-${key}`">
                        <template v-if="key != 'name' && property">
                            <th scope="row">{{ $t('inverterchannelproperty.' + key) }}</th>
                            <td class="value">
                                {{
                                    $n(property.v, 'decimal', {
                                        minimumFractionDigits: property.d,
                                        maximumFractionDigits: property.d,
                                    })
                                }}
                            </td>
                            <td>{{ property.u }}</td>
                        </template>
                    </tr>
                </tbody>
            </table>
        </div>
    </div>
</template>

<script lang="ts">
import type { InverterStatistics } from '@/types/LiveDataStatus';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    props: {
        channelData: { type: Object as PropType<InverterStatistics>, required: true },
        channelType: { type: String, required: true },
        channelNumber: { type: Number, required: true },
    },
});
</script>

<style scoped>
.channel-card {
    height: 100%;
    overflow: hidden;
    border-color: var(--bs-border-color) !important;
    border-radius: 0.75rem;
    background: var(--bs-tertiary-bg);
}

.channel-header {
    padding: 0.7rem 0.9rem;
    color: var(--bs-emphasis-color);
    background: var(--bs-secondary-bg);
    border-bottom-color: var(--bs-border-color);
    border-left: 0.22rem solid var(--bs-primary);
    font-size: 0.88rem;
    font-weight: 700;
}

.channel-header-general {
    border-left-color: var(--bs-secondary);
}

.channel-header-ac {
    border-left-color: var(--bs-info);
}

.channel-table {
    --bs-table-bg: transparent;
    --bs-table-striped-bg: transparent;
    --bs-table-hover-bg: transparent;
    font-size: 0.84rem;
}

.channel-table tr:not(:last-child) > * {
    border-bottom-color: rgba(var(--bs-secondary-rgb), 0.16);
}

.channel-table th,
.channel-table td {
    padding-top: 0.62rem;
    padding-bottom: 0.62rem;
    vertical-align: middle;
}

.channel-table th {
    color: var(--bs-secondary-color);
    font-weight: 550;
}

.channel-table td.value {
    color: var(--bs-emphasis-color);
    font-weight: 700;
    font-variant-numeric: tabular-nums;
}

.channel-table td:last-child {
    color: var(--bs-secondary-color);
    font-size: 0.92em;
}
</style>
