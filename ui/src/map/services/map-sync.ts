import { writable } from "svelte/store";

export const isMapDirty = writable<boolean>(false);

export function setMapDirty(dirty: boolean) {
  isMapDirty.set(dirty);
}

