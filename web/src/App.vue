<template>
  <div class="app-shell">
    <!-- Header -->
    <header class="app-header">
      <div class="header-left">
        <h1 class="app-title">TDC7201 GPSDO Module</h1>
        <span class="app-subtitle">Precision Timing Measurement Dashboard</span>
      </div>
      <div class="header-right">
        <span class="status-badge" :class="connected ? 'status-connected' : 'status-disconnected'">
          {{ connected ? '● Connected' : '○ Disconnected' }}
        </span>
        <span class="ip-badge" v-if="deviceIp">{{ deviceIp }}</span>
      </div>
    </header>

    <!-- Main grid -->
    <main class="main-grid">
      <MeasurementDisplay
        :phase-diff-ps="phaseDiffPs"
        :freq-offset-ppb="freqOffsetPpb"
        :measurement-count="measurementCount"
        :connected="connected"
        class="grid-span-2"
      />

      <UncertaintyAnalysis
        :uncertainty="uncertaintyBreakdown"
        :total-rms-ps="totalUncertaintyPs"
      />

      <ConfigPanel @config-updated="onConfigUpdated" />
    </main>

    <!-- Footer -->
    <footer class="app-footer">
      TDC7201 GPSDO Module &nbsp;·&nbsp; jeffkarney &nbsp;·&nbsp;
      <a href="https://github.com/jeffkarney/TDC7201-GPSDO-Module" target="_blank">GitHub</a>
    </footer>
  </div>
</template>

<script>
import MeasurementDisplay   from './components/MeasurementDisplay.vue'
import UncertaintyAnalysis  from './components/UncertaintyAnalysis.vue'
import ConfigPanel          from './components/ConfigPanel.vue'
import { startPolling }     from './api/measurements.js'

export default {
  name: 'App',

  components: { MeasurementDisplay, UncertaintyAnalysis, ConfigPanel },

  data() {
    return {
      phaseDiffPs: 0,
      freqOffsetPpb: 0,
      measurementCount: 0,
      totalUncertaintyPs: 0,
      uncertaintyBreakdown: {},
      connected: false,
      deviceIp: '',
      _poller: null,
    }
  },

  mounted() {
    this._poller = startPolling(
      (data) => {
        this.phaseDiffPs       = data.phase_difference_ps  ?? 0
        this.freqOffsetPpb     = data.freq_offset_ppb      ?? 0
        this.measurementCount  = data.measurement_count    ?? 0
        this.totalUncertaintyPs = data.uncertainty_ps      ?? 0
        this.uncertaintyBreakdown = data.uncertainty_breakdown ?? {}
        this.connected = true
      },
      () => {
        this.connected = false
      },
      100   /* ms */
    )
  },

  beforeUnmount() {
    if (this._poller) this._poller.stop()
  },

  methods: {
    onConfigUpdated(cfg) {
      console.log('Config updated:', cfg)
    },
  },
}
</script>

<style>
/* Global reset */
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

body {
  font-family: 'Segoe UI', system-ui, sans-serif;
  background: #f0f2ff;
  color: #222;
  min-height: 100vh;
}
</style>

<style scoped>
.app-shell {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
}

/* ---- Header ---- */
.app-header {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: #fff;
  padding: 16px 24px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
}

.app-title {
  font-size: 1.3rem;
  font-weight: 700;
  letter-spacing: -0.02em;
}

.app-subtitle {
  font-size: 0.78rem;
  opacity: 0.75;
  display: block;
  margin-top: 2px;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 10px;
}

.status-badge {
  font-size: 0.78rem;
  font-weight: 600;
  padding: 4px 10px;
  border-radius: 20px;
}

.status-connected    { background: rgba(76, 175, 80, 0.25); color: #b9f6ca; }
.status-disconnected { background: rgba(244, 67, 54, 0.25); color: #ffcdd2; }

.ip-badge {
  font-size: 0.75rem;
  opacity: 0.7;
  font-family: 'Courier New', monospace;
}

/* ---- Main grid ---- */
.main-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 20px;
  padding: 24px;
  flex: 1;
}

.grid-span-2 {
  grid-column: span 2;
}

/* ---- Footer ---- */
.app-footer {
  text-align: center;
  padding: 14px;
  font-size: 0.78rem;
  color: #888;
  border-top: 1px solid #e0e0e0;
}

.app-footer a {
  color: #667eea;
  text-decoration: none;
}

/* ---- Responsive ---- */
@media (max-width: 700px) {
  .main-grid {
    grid-template-columns: 1fr;
    padding: 14px;
  }
  .grid-span-2 { grid-column: span 1; }
}
</style>
