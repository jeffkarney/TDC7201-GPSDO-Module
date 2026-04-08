<!--
  MeasurementDisplay.vue – Real-time measurement results component

  Displays:
  - Phase difference in picoseconds
  - Frequency offset in ppb
  - Update counter
  - Simple sparkline trend
-->
<template>
  <div class="measurement-display">
    <h2 class="section-title">Live Measurements</h2>

    <div class="metric-grid">
      <!-- Phase difference -->
      <div class="metric-card" :class="{ 'metric-card--stale': isStale }">
        <div class="metric-label">Phase Difference</div>
        <div class="metric-value">
          <span class="metric-number">{{ phaseDiffFormatted }}</span>
          <span class="metric-unit">ps</span>
        </div>
      </div>

      <!-- Frequency offset -->
      <div class="metric-card">
        <div class="metric-label">Frequency Offset</div>
        <div class="metric-value">
          <span class="metric-number">{{ freqOffsetFormatted }}</span>
          <span class="metric-unit">ppb</span>
        </div>
      </div>

      <!-- Measurement count -->
      <div class="metric-card">
        <div class="metric-label">Measurement #</div>
        <div class="metric-value">
          <span class="metric-number">{{ measurementCount.toLocaleString() }}</span>
        </div>
      </div>

      <!-- Update rate -->
      <div class="metric-card">
        <div class="metric-label">Update Rate</div>
        <div class="metric-value">
          <span class="metric-number">{{ updateRateFormatted }}</span>
          <span class="metric-unit">Hz</span>
        </div>
      </div>
    </div>

    <!-- Trend sparkline -->
    <div class="trend-section">
      <h3 class="trend-title">Phase Trend (last {{ history.length }} samples)</h3>
      <svg class="sparkline" :viewBox="`0 0 ${svgWidth} ${svgHeight}`"
           preserveAspectRatio="none">
        <polyline
          :points="sparklinePoints"
          fill="none"
          stroke="#667eea"
          stroke-width="2"
          stroke-linejoin="round"
        />
        <line :x1="0" :y1="svgHeight / 2" :x2="svgWidth" :y2="svgHeight / 2"
              stroke="#ccc" stroke-width="1" stroke-dasharray="4 4" />
      </svg>
      <div class="trend-labels">
        <span>{{ trendMin }} ps</span>
        <span>{{ trendMax }} ps</span>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'MeasurementDisplay',

  props: {
    /** Latest phase_difference_ps from API */
    phaseDiffPs: { type: Number, default: 0 },
    /** Latest freq_offset_ppb from API */
    freqOffsetPpb: { type: Number, default: 0 },
    /** Total measurement count */
    measurementCount: { type: Number, default: 0 },
    /** True when connection is active */
    connected: { type: Boolean, default: false },
  },

  data() {
    return {
      history: [],
      maxHistory: 60,
      svgWidth: 400,
      svgHeight: 80,
      lastUpdateMs: 0,
      updateRateHz: 0,
      _rateTimer: null,
      _prevCount: 0,
    }
  },

  computed: {
    phaseDiffFormatted() {
      return this.phaseDiffPs.toFixed(2)
    },
    freqOffsetFormatted() {
      return this.freqOffsetPpb.toFixed(3)
    },
    updateRateFormatted() {
      return this.updateRateHz.toFixed(1)
    },
    isStale() {
      return !this.connected || (Date.now() - this.lastUpdateMs > 2000)
    },
    trendMin() {
      if (this.history.length === 0) return '0.00'
      return Math.min(...this.history).toFixed(2)
    },
    trendMax() {
      if (this.history.length === 0) return '0.00'
      return Math.max(...this.history).toFixed(2)
    },
    sparklinePoints() {
      if (this.history.length < 2) return ''
      const vals = this.history
      const min  = Math.min(...vals)
      const max  = Math.max(...vals)
      const range = max - min || 1
      const W = this.svgWidth
      const H = this.svgHeight
      const pad = 4

      return vals.map((v, i) => {
        const x = (i / (vals.length - 1)) * (W - 2 * pad) + pad
        const y = H - pad - ((v - min) / range) * (H - 2 * pad)
        return `${x.toFixed(1)},${y.toFixed(1)}`
      }).join(' ')
    },
  },

  watch: {
    phaseDiffPs(val) {
      this.lastUpdateMs = Date.now()
      this.history.push(val)
      if (this.history.length > this.maxHistory) {
        this.history.shift()
      }
    },
    measurementCount(newVal) {
      this.updateRateHz = newVal - this._prevCount  // samples per second (roughly)
      this._prevCount   = newVal
    },
  },

  mounted() {
    this._prevCount = this.measurementCount
  },

  beforeUnmount() {
    if (this._rateTimer) clearInterval(this._rateTimer)
  },
}
</script>

<style scoped>
.measurement-display {
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
  border-bottom: 2px solid #667eea;
  padding-bottom: 8px;
}

.metric-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
  margin-bottom: 20px;
}

.metric-card {
  background: #f8f9ff;
  border-radius: 8px;
  padding: 14px;
  border: 1px solid #e8eaf6;
  transition: border-color 0.3s;
}

.metric-card--stale {
  border-color: #f44336;
  background: #fff8f8;
}

.metric-label {
  font-size: 0.75rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: #888;
  margin-bottom: 6px;
}

.metric-value {
  display: flex;
  align-items: baseline;
  gap: 4px;
}

.metric-number {
  font-size: 1.5rem;
  font-weight: 700;
  color: #667eea;
  font-family: 'Courier New', monospace;
}

.metric-unit {
  font-size: 0.8rem;
  color: #999;
}

.trend-section {
  margin-top: 8px;
}

.trend-title {
  font-size: 0.85rem;
  color: #666;
  margin: 0 0 8px;
}

.sparkline {
  width: 100%;
  height: 80px;
  background: #f8f9ff;
  border-radius: 6px;
  border: 1px solid #e8eaf6;
  display: block;
}

.trend-labels {
  display: flex;
  justify-content: space-between;
  font-size: 0.72rem;
  color: #aaa;
  margin-top: 4px;
}
</style>
