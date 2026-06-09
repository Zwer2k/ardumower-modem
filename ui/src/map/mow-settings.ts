import { writable } from "svelte/store";
import type { MowSettings } from "../model";

const defaultSettings: MowSettings = {
  timestamp: 0,
  pattern: 0,
  width: 0.3,
  angle: 0,
  distanceToBorder: 0,
  borderLaps: 0,
  mowArea: true,
  mowExclusionBorder: false,
  mowBorderCcw: false,
};

export const mowSettingsStore = writable<MowSettings>(defaultSettings);
