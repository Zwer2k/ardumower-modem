import { writable } from "svelte/store";

export interface ConfirmOptions {
  title: string;
  message: string;
  confirmText?: string;
  cancelText?: string;
  kind?: "primary" | "danger";
}

export interface ConfirmState extends ConfirmOptions {
  resolve: (value: boolean) => void;
}

export const confirmDialogStore = writable<ConfirmState | null>(null);

export function openConfirm(options: ConfirmOptions): Promise<boolean> {
  return new Promise((resolve) => {
    confirmDialogStore.set({ ...options, resolve });
  });
}

export function closeConfirm(value: boolean) {
  confirmDialogStore.update((state) => {
    if (state) state.resolve(value);
    return null;
  });
}
