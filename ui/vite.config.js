import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig, loadEnv } from 'vite';
import { visualizer } from 'rollup-plugin-visualizer';

const production = false;

export default defineConfig(({ mode }) => {
	// Load env file from parent directory (one level up)
	const env = loadEnv(mode, '../', '');
	const espDevIp = env.ESP_DEV_IP || '192.168.43.220';

	const mapEnabled = env.VITE_ENABLE_MAP === 'true';
	const liveMapEnabled = env.VITE_ENABLE_LIVE_MAP === 'true';
	const gpsDashboardEnabled = env.VITE_ENABLE_GPS_DASHBOARD === 'true';

	return {
		define: {
			'import.meta.env.VITE_ENABLE_MAP': mapEnabled ? '"true"' : '"false"',
			'import.meta.env.VITE_ENABLE_LIVE_MAP': liveMapEnabled ? '"true"' : '"false"',
			'import.meta.env.VITE_ENABLE_GPS_DASHBOARD': gpsDashboardEnabled ? '"true"' : '"false"'
		},
		...(!production && { 
			server: {
				host: 'localhost',
				port: 5000,
				proxy: {
					'/api': {
						target: `http://${espDevIp}`,
						changeOrigin: true,
					},
					'/ws': {
						target: `ws://${espDevIp}`,
						ws: true,
						changeOrigin: true,
						// Prevent Vite from injecting its own HMR client WebSocket
						// upgrade handling. The app opens its own WS to /ws, so we
						// only need a plain TCP-level proxy.
						rewriteWsOrigin: false,
						// Keep the proxy alive for long-running connections and
						// avoid premature socket resets when the ESP pauses briefly.
						configure: (proxy, options) => {
							proxy.on('error', (err, req, res) => {
								// Suppress noisy ECONNRESET/EPIPE logs that would otherwise
								// flood the dev-server console. These are expected when the
								// ESP reboots or closes idle connections.
								if (err && err.code && ['ECONNRESET', 'EPIPE', 'ETIMEDOUT'].includes(err.code)) {
									return;
								}
								console.warn('[vite ws proxy] error:', err.message);
							});
						},
					},
				},
			}
		}),
		plugins: [
			sveltekit(),
			// visualizer({
			// 	emitFile: true,
			// 	filename: 'stats.html'
			// })
		],
		build: {
			rollupOptions: {
				output: {
					manualChunks: mapEnabled ? (id) => {
						return 'my-app';
					} : undefined
				}
			}
		}
	};
});

