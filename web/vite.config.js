import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [vue()],

  server: {
    port: 3000,
    proxy: {
      // Proxy API calls to the ESP32-C3 during local development.
      // Set VITE_DEV_TARGET to the device IP, e.g. http://192.168.1.42
      '/api': {
        target: process.env.VITE_DEV_TARGET || 'http://localhost:8080',
        changeOrigin: true,
      },
    },
  },

  build: {
    outDir: 'dist',
    // Keep the bundle small for embedding in ESP32-C3 SPIFFS (optional)
    rollupOptions: {
      output: {
        manualChunks: undefined,
      },
    },
  },
})
