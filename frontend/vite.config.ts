import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    // Optimize for small bundle size
    minify: 'esbuild',
    rollupOptions: {
      output: {
        manualChunks: undefined,
        // Keep bundle size small for embedded system
        compact: true,
      },
    },
    // Target modern browsers (ESP32 web server)
    target: 'es2015',
  },
  server: {
    port: 5173,
    host: true,
  },
})

