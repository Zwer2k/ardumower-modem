import { writable } from 'svelte/store';

export interface ToastData {
  msg: string;
  type: 'success' | 'error';
}

export const toastStore = writable<ToastData | null>(null);
