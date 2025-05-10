import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';
import { visualizer } from 'rollup-plugin-visualizer';

const production = false;

export default defineConfig({
	...(!production && { 
		server: {
			host: 'localhost',
			port: 5000,
			proxy: {
				'/api': {
					target: 'http://192.168.43.180',
					changeOrigin: true,
				},
				'/ws': {
					target: 'ws://192.168.43.180',
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
});

