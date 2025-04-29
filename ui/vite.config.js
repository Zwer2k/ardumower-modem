import { sveltekit } from '@sveltejs/kit/vite';
import { svelte, vitePreprocess } from '@sveltejs/vite-plugin-svelte';
import { defineConfig } from 'vite';
import { optimizeImports } from "carbon-preprocess-svelte";

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
	],
});

