<template>
  <div class="dashboard">
    <h1>Real-Time Measurements</h1>
    <div class="measurements">
      <h2>Phase Difference: <span>{{ phaseDifference }}</span> ps</h2>
      <h2>Measurement Count: <span>{{ measurementCount }}</span></h2>
    </div>
    <div class="uncertainty-analysis">
      <h2>Uncertainty Analysis</h2>
      <div>
        <div>Total RSS Uncertainty: 1.1 ps</div>
        <div>Comparator Offset: <span>{{ comparatorOffset }} ps</span></div>
        <div>Comparator Jitter: <span>{{ comparatorJitter }} ps</span></div>
        <div>TDC Resolution: <span>{{ tdcResolution }} ps</span></div>
        <div>Delay Line Error: <span>{{ delayLineError }} ps</span></div>
        <div>Temperature Drift: <span>{{ temperatureDrift }} ps</span></div>
        <div class="percentage-bars">
          <div class="bar"><div class="fill" :style="{ width: '45%' }"></div> Comparator Offset (0.5 ps)</div>
          <div class="bar"><div class="fill" :style="{ width: '5%' }"></div> Comparator Jitter (0.05 ps)</div>
          <div class="bar"><div class="fill" :style="{ width: '5.5%' }"></div> TDC Resolution (0.055 ps)</div>
          <div class="bar"><div class="fill" :style="{ width: '45%' }"></div> Delay Line Error (0.5 ps)</div>
          <div class="bar"><div class="fill" :style="{ width: '10%' }"></div> Temperature Drift (0.1 ps)</div>
        </div>
      </div>
    </div>
    <div class="configuration">
      <h2>Configuration</h2>
      <label for="delayLineTap">Delay Line Tap:</label>
      <input type="range" id="delayLineTap" min="0" max="14" v-model="delayLineTap" @input="updateDelayLineTap"> ns
      <label for="measurementMode">Measurement Mode:</label>
      <select id="measurementMode" v-model="measurementMode">
        <option value="phase">Phase</option>
        <option value="frequency">Frequency</option>
        <option value="time-interval">Time Interval</option>
      </select>
    </div>
    <div v-if="connectionStatus">
      <p>Connected to API</p>
    </div>
    <div v-else>
      <p>Disconnected from API</p>
    </div>
  </div>
</template>

<script>
export default {
  data() {
    return {
      phaseDifference: 0,
      measurementCount: 0,
      comparatorOffset: 0.5,
      comparatorJitter: 0.05,
      tdcResolution: 0.055,
      delayLineError: 0.5,
      temperatureDrift: 0.1,
      delayLineTap: 0,
      measurementMode: 'phase',
      connectionStatus: false,
    };
  },
  methods: {
    fetchMeasurements() {
      setInterval(async () => {
        try {
          const response = await fetch('/api/measurements');
          const data = await response.json();
          this.phaseDifference = data.phaseDifference;
          this.measurementCount = data.measurementCount;
          this.connectionStatus = true;
        } catch (error) {
          console.error(error);
          this.connectionStatus = false;
        }
      }, 100);
    },
    updateDelayLineTap() {
      // Handle delay line tap change
    },
  },
  mounted() {
    this.fetchMeasurements();
  },
};
</script>

<style scoped>
.dashboard {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 20px;
  background: linear-gradient(135deg, #6e3b8c, #e564de);
  padding: 20px;
  color: white;
}
.measurements, .uncertainty-analysis, .configuration {
  background: rgba(255, 255, 255, 0.1);
  border-radius: 10px;
  padding: 15px;
}
.percentage-bars {
  margin-top: 10px;
}
.bar {
  background: #fff;
  border-radius: 5px;
  overflow: hidden;
  position: relative;
  margin-top: 5px;
}
.bar .fill {
  background: #4caf50;
  height: 100%;
}
</style>
