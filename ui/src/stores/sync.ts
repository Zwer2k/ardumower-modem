import { writable } from 'svelte/store';

const STORAGE_KEY = 'ardumower_sync_state';

function loadInitial(): { needsUpload: boolean; mapCrcBeforeUpload: number; syncedMapCrc: number; uploadTimestamp: number } {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      return {
        needsUpload: parsed.needsUpload ?? false,
        mapCrcBeforeUpload: parsed.mapCrcBeforeUpload ?? -1,
        syncedMapCrc: parsed.syncedMapCrc ?? -1,
        uploadTimestamp: 0,
      };
    }
  } catch (_) { /* ignore */ }
  return { needsUpload: false, mapCrcBeforeUpload: -1, syncedMapCrc: -1, uploadTimestamp: 0 };
}

function persist(state: { needsUpload: boolean; mapCrcBeforeUpload: number; syncedMapCrc: number; uploadTimestamp: number }) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({
      needsUpload: state.needsUpload,
      mapCrcBeforeUpload: state.mapCrcBeforeUpload,
      syncedMapCrc: state.syncedMapCrc,
    }));
  } catch (_) { /* ignore */ }
}

export const SyncStore = writable(loadInitial());

SyncStore.subscribe((state) => persist(state));
