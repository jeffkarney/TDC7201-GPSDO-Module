<!--
  UncertaintyAnalysis.vue – Uncertainty breakdown visualization component

  Shows:
  - Total RMS uncertainty
  - Component-wise contributions as stacked bar chart
  - Confidence interval display
-->
<template>
  <div class="uncertainty-analysis">
    <h2 class="section-title">Measurement Uncertainty</h2>

    <!-- Total uncertainty badge -->
    <div class="total-badge">
      <span class="total-label">Total RMS Uncertainty</span>
      <span class="total-value">±{{ totalRmsFormatted }} ps</span>
    </div>

    <!-- Confidence interval note -->
    <p class="confidence-note">
      95% confidence interval: <strong>±{{ ci95Formatted }} ps</strong>
      (k = 2 coverage factor)
    </p>

    <!-- Stacked bar chart -->
    <div class="bar-chart">
      <div
        v-for="(item, index) in components"
        :key="item.key"
        class="bar-row"
      >
        <div class="bar-label">{{ item.label }}</div>
        <div class="bar-track">
          <div
            class="bar-fill"
            :style="{
              width: barPercent(item.value) + '%',
              background: palette[index % palette.length],
            }"
          />
        </div>
        <div class="bar-value">{{ item.value.toFixed(0) }} ps</div>
      </div>
    </div>

    <!-- Legend table -->
    <table class="legend-table">
      <thead>
        <tr>
          <th>Source</th>
          <th>Value (ps)</th>
          <th>Contribution (%)</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="(item, index) in components" :key="item.key + '_row'">
          <td>
            <span
              class="legend-dot"
              :style="{ background: palette[index % palette.length] }"
            />
            {{ item.label }}
          </td>
          <td class="num-cell">{{ item.value.toFixed(1) }}</td>
          <td class="num-cell">{{ contribution(item.value) }}%</td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<script>
export default {
  name: 'UncertaintyAnalysis',

  props: {
    /** Uncertainty breakdown object from /api/measurements */
    uncertainty: {
      type: Object,
      default: () => ({
        comparator_offset_ps: 500,
        comparator_jitter_ps: 50,
        tdc_resolution_ps: 55,
        delay_line_error_ps: 500,
        temperature_drift_ps: 100,
      }),
    },
    /** Total RMS uncertainty in ps (from API) */
    totalRmsPs: { type: Number, default: 0 },
  },

  data() {
    return {
      palette: ['#667eea', '#764ba2', '#f093fb', '#f5576c', '#4facfe'],
    }
  },

  computed: {
    components() {
      const u = this.uncertainty
      return [
        { key: 'comparator_offset',  label: 'TLV3501 Offset',      value: u.comparator_offset_ps  || 0 },
        { key: 'comparator_jitter',  label: 'TLV3501 Jitter',      value: u.comparator_jitter_ps  || 0 },
        { key: 'tdc_resolution',     label: 'TDC Resolution',      value: u.tdc_resolution_ps     || 0 },
        { key: 'delay_line_error',   label: 'Delay Line Error',    value: u.delay_line_error_ps   || 0 },
        { key: 'temperature_drift',  label: 'Temperature Drift',   value: u.temperature_drift_ps  || 0 },
      ]
    },
    totalRms() {
      // Use the API value if nonzero, else recompute from components
      if (this.totalRmsPs > 0) return this.totalRmsPs
      return Math.sqrt(
        this.components.reduce((sum, c) => sum + c.value * c.value, 0)
      )
    },
    totalRmsFormatted() {
      return this.totalRms.toFixed(2)
    },
    ci95Formatted() {
      return (this.totalRms * 2).toFixed(2)
    },
    maxComponent() {
      return Math.max(...this.components.map(c => c.value), 1)
    },
  },

  methods: {
    barPercent(value) {
      return Math.min((value / this.maxComponent) * 100, 100)
    },
    contribution(value) {
      const sq = value * value
      const total = this.components.reduce((sum, c) => sum + c.value * c.value, 0)
      if (total === 0) return '0.0'
      return ((sq / total) * 100).toFixed(1)
    },
  },
}
</script>

<style scoped>
.uncertainty-analysis {
  background: #fff;
  border-radius: 10px;
  padding: 20px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.08);
}

.section-title {
  margin: 0 0 16px;
  font-size: 1.1rem;
  font-weight: 700;
  color: #333;
  border-bottom: 2px solid #764ba2;
  padding-bottom: 8px;
}

.total-badge {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: linear-gradient(135deg, #667eea, #764ba2);
  border-radius: 8px;
  padding: 12px 16px;
  color: #fff;
  margin-bottom: 10px;
}

.total-label {
  font-size: 0.85rem;
  opacity: 0.9;
}

.total-value {
  font-size: 1.4rem;
  font-weight: 700;
  font-family: 'Courier New', monospace;
}

.confidence-note {
  font-size: 0.8rem;
  color: #777;
  margin: 0 0 16px;
}

.bar-chart {
  display: flex;
  flex-direction: column;
  gap: 10px;
  margin-bottom: 16px;
}

.bar-row {
  display: grid;
  grid-template-columns: 130px 1fr 60px;
  align-items: center;
  gap: 8px;
}

.bar-label {
  font-size: 0.8rem;
  color: #555;
  text-align: right;
}

.bar-track {
  height: 14px;
  background: #f0f0f0;
  border-radius: 7px;
  overflow: hidden;
}

.bar-fill {
  height: 100%;
  border-radius: 7px;
  transition: width 0.4s ease;
}

.bar-value {
  font-size: 0.75rem;
  color: #888;
  font-family: 'Courier New', monospace;
  text-align: right;
}

.legend-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.8rem;
  margin-top: 8px;
}

.legend-table th {
  text-align: left;
  color: #888;
  font-weight: 600;
  border-bottom: 1px solid #eee;
  padding: 4px 6px;
}

.legend-table td {
  padding: 4px 6px;
  border-bottom: 1px solid #f5f5f5;
}

.legend-dot {
  display: inline-block;
  width: 10px;
  height: 10px;
  border-radius: 50%;
  margin-right: 6px;
  vertical-align: middle;
}

.num-cell {
  text-align: right;
  font-family: 'Courier New', monospace;
}
</style>
