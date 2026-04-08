<!--
  ConfigPanel.vue – Device configuration controls

  Allows the user to:
  - Adjust delay line tap (0–14, ~7 ns/tap)
  - Select measurement mode
  - Set sampling rate
  - Save or reset configuration
  - Trigger calibration reset
-->
<template>
  <div class="config-panel">
    <h2 class="section-title">Configuration</h2>

    <form @submit.prevent="saveConfig" class="config-form">

      <!-- Delay line tap -->
      <div class="field-group">
        <label class="field-label" for="delay-tap">
          Delay Line Tap
          <span class="field-hint">{{ delayTap }} tap → ~{{ (delayTap * 7).toFixed(0) }} ns offset</span>
        </label>
        <div class="slider-row">
          <span class="slider-min">0</span>
          <input
            id="delay-tap"
            v-model.number="delayTap"
            type="range"
            min="0"
            max="14"
            step="1"
            class="slider"
          />
          <span class="slider-max">14</span>
        </div>
        <div class="slider-ticks">
          <span v-for="n in 15" :key="n" :class="{ active: delayTap === n - 1 }">
            {{ n - 1 }}
          </span>
        </div>
      </div>

      <!-- Measurement mode -->
      <div class="field-group">
        <label class="field-label" for="meas-mode">Measurement Mode</label>
        <select id="meas-mode" v-model="measurementMode" class="select">
          <option value="phase">Phase Difference (ps)</option>
          <option value="frequency">Frequency Offset (ppb)</option>
          <option value="time-interval">Time Interval</option>
        </select>
      </div>

      <!-- Sampling rate -->
      <div class="field-group">
        <label class="field-label" for="sample-rate">
          Sampling Rate
          <span class="field-hint">{{ samplingRate }} Hz</span>
        </label>
        <select id="sample-rate" v-model.number="samplingRate" class="select">
          <option :value="1">1 Hz</option>
          <option :value="10">10 Hz</option>
          <option :value="100">100 Hz</option>
        </select>
      </div>

      <!-- Action buttons -->
      <div class="button-row">
        <button type="submit" class="btn btn--primary" :disabled="saving">
          {{ saving ? 'Saving…' : 'Save Settings' }}
        </button>
        <button type="button" class="btn btn--secondary" @click="resetDefaults">
          Reset Defaults
        </button>
      </div>

    </form>

    <!-- Divider -->
    <hr class="divider" />

    <!-- Calibration section -->
    <div class="calibration-section">
      <h3 class="subsection-title">Calibration</h3>
      <p class="calibration-hint">
        Resets the measurement history and statistics buffer on the device.
        Perform this after adjusting the delay tap.
      </p>
      <button class="btn btn--danger" :disabled="calibrating" @click="triggerCalibration">
        {{ calibrating ? 'Resetting…' : 'Reset Calibration' }}
      </button>
      <div v-if="calibrationStatus" class="status-message" :class="calibrationStatus.type">
        {{ calibrationStatus.message }}
      </div>
    </div>

    <!-- Save status -->
    <div v-if="saveStatus" class="status-message" :class="saveStatus.type">
      {{ saveStatus.message }}
    </div>
  </div>
</template>

<script>
import { postConfig, postCalibrate } from '../api/measurements.js'

export default {
  name: 'ConfigPanel',

  emits: ['config-updated'],

  data() {
    return {
      delayTap: 2,
      measurementMode: 'phase',
      samplingRate: 10,
      saving: false,
      calibrating: false,
      saveStatus: null,
      calibrationStatus: null,
    }
  },

  methods: {
    async saveConfig() {
      this.saving = true
      this.saveStatus = null
      try {
        await postConfig({
          delay_line_tap: this.delayTap,
          measurement_mode: this.measurementMode,
          sampling_rate_hz: this.samplingRate,
        })
        this.saveStatus = { type: 'success', message: '✓ Configuration saved successfully.' }
        this.$emit('config-updated', {
          delay_line_tap: this.delayTap,
          measurement_mode: this.measurementMode,
        })
      } catch (err) {
        this.saveStatus = { type: 'error', message: `✗ Save failed: ${err.message}` }
      } finally {
        this.saving = false
        setTimeout(() => { this.saveStatus = null }, 4000)
      }
    },

    resetDefaults() {
      this.delayTap        = 2
      this.measurementMode = 'phase'
      this.samplingRate    = 10
    },

    async triggerCalibration() {
      this.calibrating = true
      this.calibrationStatus = null
      try {
        await postCalibrate()
        this.calibrationStatus = { type: 'success', message: '✓ Calibration reset on device.' }
      } catch (err) {
        this.calibrationStatus = { type: 'error', message: `✗ Calibration failed: ${err.message}` }
      } finally {
        this.calibrating = false
        setTimeout(() => { this.calibrationStatus = null }, 4000)
      }
    },
  },
}
</script>

<style scoped>
.config-panel {
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
  border-bottom: 2px solid #4facfe;
  padding-bottom: 8px;
}

.subsection-title {
  font-size: 0.95rem;
  font-weight: 600;
  color: #444;
  margin: 0 0 8px;
}

.config-form {
  display: flex;
  flex-direction: column;
  gap: 18px;
}

.field-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.field-label {
  font-size: 0.85rem;
  font-weight: 600;
  color: #555;
  display: flex;
  justify-content: space-between;
  align-items: baseline;
}

.field-hint {
  font-size: 0.75rem;
  font-weight: 400;
  color: #999;
}

.slider-row {
  display: flex;
  align-items: center;
  gap: 8px;
}

.slider-min,
.slider-max {
  font-size: 0.75rem;
  color: #aaa;
}

.slider {
  flex: 1;
  appearance: none;
  height: 6px;
  border-radius: 3px;
  background: linear-gradient(to right, #667eea, #764ba2);
  outline: none;
  cursor: pointer;
}

.slider::-webkit-slider-thumb {
  appearance: none;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: #fff;
  border: 2px solid #667eea;
  cursor: pointer;
  box-shadow: 0 1px 4px rgba(0, 0, 0, 0.2);
}

.slider-ticks {
  display: flex;
  justify-content: space-between;
  font-size: 0.6rem;
  color: #ccc;
  padding: 0 2px;
}

.slider-ticks span.active {
  color: #667eea;
  font-weight: 700;
}

.select {
  padding: 8px 10px;
  border: 1px solid #ddd;
  border-radius: 6px;
  font-size: 0.875rem;
  color: #333;
  background: #fafafa;
  cursor: pointer;
  transition: border-color 0.2s;
}

.select:focus {
  border-color: #667eea;
  outline: none;
}

.button-row {
  display: flex;
  gap: 10px;
  flex-wrap: wrap;
}

.btn {
  flex: 1;
  padding: 10px 16px;
  border: none;
  border-radius: 6px;
  font-size: 0.875rem;
  font-weight: 600;
  cursor: pointer;
  transition: opacity 0.2s, transform 0.1s;
}

.btn:hover:not(:disabled) {
  opacity: 0.9;
  transform: translateY(-1px);
}

.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn--primary {
  background: linear-gradient(135deg, #667eea, #764ba2);
  color: #fff;
}

.btn--secondary {
  background: #f0f0f0;
  color: #555;
}

.btn--danger {
  background: #f44336;
  color: #fff;
  width: 100%;
  margin-top: 8px;
}

.divider {
  border: none;
  border-top: 1px solid #eee;
  margin: 18px 0;
}

.calibration-section {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.calibration-hint {
  font-size: 0.78rem;
  color: #888;
  margin: 0;
}

.status-message {
  margin-top: 8px;
  padding: 8px 12px;
  border-radius: 6px;
  font-size: 0.82rem;
}

.status-message.success {
  background: #e8f5e9;
  color: #2e7d32;
}

.status-message.error {
  background: #ffebee;
  color: #c62828;
}
</style>
