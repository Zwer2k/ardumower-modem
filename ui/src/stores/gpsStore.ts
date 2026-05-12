import { writable, derived } from "svelte/store";
import { browser } from "$app/environment";
import { socketStore, socketService } from "./socket";
import {
  parseUbxFrames,
  getParserForFrame,
} from "../pages/daschboard/ubxCommands";
import type { GpsDetails, GpsSatellite } from "../model";

// ─── Types ──────────────────────────────────────────────────────────────────

export interface NavSatInfo {
  gnssId: number;
  svId: number;
  elev: number; // degrees
  azim: number; // degrees
  cno: number; // dB-Hz
  used: boolean;
  health: number;
  quality: number;
}

export interface NavPvtInfo {
  lat: number;
  lon: number;
  height: number; // ellipsoid
  hMSL: number;
  hAcc: number;
  vAcc: number;
  gSpeed: number;
  heading: number;
  pDOP: number;
  fixType: number;
  fixOk: boolean;
  numSV: number;
  carrSoln: number; // 0=none, 1=float, 2=fixed
  timestamp: number;
}

export interface NavDopInfo {
  gDOP: number;
  pDOP: number;
  tDOP: number;
  vDOP: number;
  hDOP: number;
  nDOP: number;
  eDOP: number;
}

export interface AltitudeSample {
  time: number;
  height: number;
  hMSL: number;
}

export interface PositionSample {
  time: number;
  lat: number;
  lon: number;
  hAcc: number;
}

export interface GpsStoreState {
  // From S4 CSV
  gpsDetails: GpsDetails | null;

  // From UBX polling
  navSat: NavSatInfo[];
  navPvt: NavPvtInfo | null;
  navDop: NavDopInfo | null;

  // History ring buffers
  altitudeHistory: AltitudeSample[];
  positionHistory: PositionSample[];

  // Reference position for deviation map (mean of first N valid fixes)
  refLat: number | null;
  refLon: number | null;

  // Polling state
  lastNavSatPoll: number;
  lastNavPvtPoll: number;
  lastNavDopPoll: number;
  pendingPolls: Set<string>;
}

const HISTORY_SIZE = 200;
const REF_FIX_COUNT = 20;

const initialState: GpsStoreState = {
  gpsDetails: null,
  navSat: [],
  navPvt: null,
  navDop: null,
  altitudeHistory: [],
  positionHistory: [],
  refLat: null,
  refLon: null,
  lastNavSatPoll: 0,
  lastNavPvtPoll: 0,
  lastNavDopPoll: 0,
  pendingPolls: new Set(),
};

function createGpsStore() {
  const { subscribe, set, update } = writable<GpsStoreState>(initialState);

  let unsubscribeSocket: (() => void) | null = null;
  let pollInterval: ReturnType<typeof setInterval> | null = null;
  let processedUbxTimestamp = 0;

  // Hex commands for polling (with Fletcher checksum)
  const NAV_SAT_POLL = "B5620135000036A3"; // UBX-NAV-SAT poll
  const NAV_PVT_POLL = "B562010700000819"; // UBX-NAV-PVT poll
  const NAV_DOP_POLL = "B562010400000510"; // UBX-NAV-DOP poll

  function startPolling() {
    if (!browser) return;
    stopPolling();

    // Immediate first poll
    sendPolls();

    // Then every 5 seconds
    pollInterval = setInterval(() => {
      sendPolls();
    }, 5000);
  }

  function stopPolling() {
    if (pollInterval) {
      clearInterval(pollInterval);
      pollInterval = null;
    }
  }

  function sendPolls() {
    const now = Date.now();
    update((s) => {
      // Reset stale pending polls (timeout 10s)
      for (const key of Array.from(s.pendingPolls)) {
        const lastPoll =
          key === "nav-sat"
            ? s.lastNavSatPoll
            : key === "nav-pvt"
              ? s.lastNavPvtPoll
              : s.lastNavDopPoll;
        if (now - lastPoll > 10000) {
          s.pendingPolls.delete(key);
        }
      }

      // NAV-SAT every 5s
      if (now - s.lastNavSatPoll >= 5000 && !s.pendingPolls.has("nav-sat")) {
        socketService.sendUbx(NAV_SAT_POLL);
        s.pendingPolls.add("nav-sat");
        s.lastNavSatPoll = now;
      }
      // NAV-PVT every 5s
      if (now - s.lastNavPvtPoll >= 5000 && !s.pendingPolls.has("nav-pvt")) {
        socketService.sendUbx(NAV_PVT_POLL);
        s.pendingPolls.add("nav-pvt");
        s.lastNavPvtPoll = now;
      }
      // NAV-DOP every 10s
      if (now - s.lastNavDopPoll >= 10000 && !s.pendingPolls.has("nav-dop")) {
        socketService.sendUbx(NAV_DOP_POLL);
        s.pendingPolls.add("nav-dop");
        s.lastNavDopPoll = now;
      }
      return s;
    });
  }

  function handleSocketState(state: {
    gpsDetails: GpsDetails | null;
    ubxResponse: { timestamp: number; hex: string } | null;
  }) {
    update((s) => {
      // Process S4 GPS details
      if (state.gpsDetails && state.gpsDetails.timestamp) {
        s.gpsDetails = state.gpsDetails;
      }

      // Process UBX response
      if (
        state.ubxResponse &&
        state.ubxResponse.timestamp &&
        state.ubxResponse.timestamp !== processedUbxTimestamp
      ) {
        processedUbxTimestamp = state.ubxResponse.timestamp;
        const cleanHex = (state.ubxResponse.hex || "").replace(
          /[^0-9a-fA-F]/g,
          "",
        );
        if (cleanHex.length > 0) {
          parseUbxResponse(s, cleanHex);
        }
      }

      return s;
    });
  }

  function parseUbxResponse(s: GpsStoreState, hex: string) {
    const frames = parseUbxFrames(hex);
    for (const frame of frames) {
      if (!frame.valid) continue;

      if (frame.msgName === "NAV-SAT") {
        s.navSat = parseNavSat(hex) || [];
        s.pendingPolls.delete("nav-sat");
      } else if (frame.msgName === "NAV-PVT") {
        const pvt = parseNavPvt(hex);
        if (pvt) {
          s.navPvt = pvt;
          s.pendingPolls.delete("nav-pvt");

          // Update altitude history
          s.altitudeHistory.push({
            time: pvt.timestamp,
            height: pvt.height,
            hMSL: pvt.hMSL,
          });
          if (s.altitudeHistory.length > HISTORY_SIZE) {
            s.altitudeHistory.shift();
          }

          // Update position history
          s.positionHistory.push({
            time: pvt.timestamp,
            lat: pvt.lat,
            lon: pvt.lon,
            hAcc: pvt.hAcc,
          });
          if (s.positionHistory.length > HISTORY_SIZE) {
            s.positionHistory.shift();
          }

          // Update reference position
          if (pvt.fixOk && pvt.fixType >= 2) {
            if (s.refLat === null || s.refLon === null) {
              // Use first valid fix as reference
              if (s.positionHistory.length >= REF_FIX_COUNT) {
                const validFixes = s.positionHistory.slice(-REF_FIX_COUNT);
                s.refLat =
                  validFixes.reduce((sum, p) => sum + p.lat, 0) /
                  validFixes.length;
                s.refLon =
                  validFixes.reduce((sum, p) => sum + p.lon, 0) /
                  validFixes.length;
              }
            }
          }
        }
      } else if (frame.msgName === "NAV-DOP") {
        const dop = parseNavDop(hex);
        if (dop) {
          s.navDop = dop;
          s.pendingPolls.delete("nav-dop");
        }
      }
    }
  }

  function parseNavSat(hex: string): NavSatInfo[] | null {
    const bytes = hexToBytes(hex);
    const frames = findUbxFrames(hex);
    const f = frames.find((x) => x.classId === 0x01 && x.msgId === 0x35);
    if (!f || f.length < 8) return null;
    const p = f.payloadStart;
    const numSats = bytes[p + 4] | (bytes[p + 5] << 8);
    const sats: NavSatInfo[] = [];
    for (let i = 0; i < numSats && p + 8 + i * 12 + 11 < bytes.length; i++) {
      const off = p + 8 + i * 12;
      const gnssId = bytes[off];
      const svId = bytes[off + 1];
      const cno = bytes[off + 2];
      const elev = bytesToSigned(bytes, off + 3);
      const azim = bytes[off + 4] | (bytes[off + 5] << 8);
      const flags =
        bytes[off + 8] |
        (bytes[off + 9] << 8) |
        (bytes[off + 10] << 16) |
        (bytes[off + 11] << 24);
      const qual = flags & 0x07;
      const used = (flags & 0x40) !== 0;
      const health = (flags >> 8) & 0x03;

      sats.push({ gnssId, svId, elev, azim, cno, used, health, quality: qual });
    }
    return sats;
  }

  function parseNavPvt(hex: string): NavPvtInfo | null {
    const bytes = hexToBytes(hex);
    const frames = findUbxFrames(hex);
    const f = frames.find((x) => x.classId === 0x01 && x.msgId === 0x07);
    if (!f || f.length < 92) return null;
    const p = f.payloadStart;

    const fixType = bytes[p + 20];
    const fixOk = (bytes[p + 21] & 0x01) !== 0;
    const carrSoln = (bytes[p + 21] >> 6) & 0x03;
    const numSV = bytes[p + 23];
    const lon = readI4(bytes, p + 24) * 1e-7;
    const lat = readI4(bytes, p + 28) * 1e-7;
    const height = readI4(bytes, p + 32) * 1e-3;
    const hMSL = readI4(bytes, p + 36) * 1e-3;
    const hAcc = readU4(bytes, p + 40) * 1e-3;
    const vAcc = readU4(bytes, p + 44) * 1e-3;
    const gSpeed = readI4(bytes, p + 60) * 1e-3;
    const heading = readI4(bytes, p + 64) * 1e-5;
    const pDOP = readU2(bytes, p + 76) * 0.01;

    return {
      lat,
      lon,
      height,
      hMSL,
      hAcc,
      vAcc,
      gSpeed,
      heading,
      pDOP,
      fixType,
      fixOk,
      numSV,
      carrSoln,
      timestamp: Date.now(),
    };
  }

  function parseNavDop(hex: string): NavDopInfo | null {
    const bytes = hexToBytes(hex);
    const frames = findUbxFrames(hex);
    const f = frames.find((x) => x.classId === 0x01 && x.msgId === 0x04);
    if (!f || f.length < 18) return null;
    const p = f.payloadStart;

    return {
      gDOP: readU2(bytes, p + 0) * 0.01,
      pDOP: readU2(bytes, p + 2) * 0.01,
      tDOP: readU2(bytes, p + 4) * 0.01,
      vDOP: readU2(bytes, p + 6) * 0.01,
      hDOP: readU2(bytes, p + 8) * 0.01,
      nDOP: readU2(bytes, p + 10) * 0.01,
      eDOP: readU2(bytes, p + 12) * 0.01,
    };
  }

  function connect() {
    if (unsubscribeSocket) return;
    unsubscribeSocket = socketStore.subscribe(handleSocketState);
  }

  function disconnect() {
    stopPolling();
    if (unsubscribeSocket) {
      unsubscribeSocket();
      unsubscribeSocket = null;
    }
  }

  return {
    subscribe,
    startPolling,
    stopPolling,
    connect,
    disconnect,
  };
}

// ─── Low-level helpers (duplicated from ubxCommands to avoid circular dep) ───

function hexToBytes(hex: string): number[] {
  const bytes: number[] = [];
  hex = hex.replace(/\s/g, "");
  for (let i = 0; i + 1 < hex.length; i += 2) {
    bytes.push(parseInt(hex.substring(i, i + 2), 16));
  }
  return bytes;
}

function readU2(bytes: number[], off: number): number {
  return bytes[off] | (bytes[off + 1] << 8);
}

function readU4(bytes: number[], off: number): number {
  return (
    bytes[off] |
    (bytes[off + 1] << 8) |
    (bytes[off + 2] << 16) |
    (bytes[off + 3] << 24)
  );
}

function readI4(bytes: number[], off: number): number {
  const v = readU4(bytes, off);
  return v >= 0x80000000 ? v - 0x100000000 : v;
}

function bytesToSigned(bytes: number[], off: number): number {
  const v = bytes[off];
  return v >= 0x80 ? v - 0x100 : v;
}

function ubxChecksum(
  bytes: number[],
  start: number,
  len: number,
): { ckA: number; ckB: number } {
  let ckA = 0,
    ckB = 0;
  for (let i = 0; i < len; i++) {
    ckA = (ckA + bytes[start + i]) & 0xff;
    ckB = (ckB + ckA) & 0xff;
  }
  return { ckA, ckB };
}

function findUbxFrames(hex: string): Array<{
  start: number;
  classId: number;
  msgId: number;
  length: number;
  payloadStart: number;
  payloadEnd: number;
  valid: boolean;
}> {
  const bytes = hexToBytes(hex);
  const frames = [];
  for (let i = 0; i < bytes.length - 8; i++) {
    if (bytes[i] === 0xb5 && bytes[i + 1] === 0x62) {
      const classId = bytes[i + 2];
      const msgId = bytes[i + 3];
      const length = bytes[i + 4] | (bytes[i + 5] << 8);
      const payloadStart = i + 6;
      const payloadEnd = payloadStart + length;
      const ckPos = payloadEnd;
      if (ckPos + 1 >= bytes.length) continue;
      const ckA = bytes[ckPos];
      const ckB = bytes[ckPos + 1];
      const calc = ubxChecksum(bytes, i + 2, length + 4);
      frames.push({
        start: i,
        classId,
        msgId,
        length,
        payloadStart,
        payloadEnd,
        valid: ckA === calc.ckA && ckB === calc.ckB,
      });
      i = ckPos + 1;
    }
  }
  return frames;
}

export const gpsStore = createGpsStore();
