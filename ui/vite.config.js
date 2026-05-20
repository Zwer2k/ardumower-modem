import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig, loadEnv } from 'vite';
import { visualizer } from 'rollup-plugin-visualizer';

const production = false;

export default defineConfig(({ mode }) => {
	// Load env file from parent directory (one level up)
	const env = loadEnv(mode, '../', '');
	const espDevIp = env.ESP_DEV_IP || '192.168.43.221';

	return {
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
						ws: true, // Wichtig für WebSocket-Proxying
						changeOrigin: true, // Empfohlen für Cross-Origin-Szenarien
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
					manualChunks: (id) => {
						return 'my-app';
					}
				}
			}
		}
	};
});

