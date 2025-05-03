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
					target: 'http://192.168.43.220',
					changeOrigin: true,
					rewrite: (path) => path.replace(/^\/api/, '/api'),
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

