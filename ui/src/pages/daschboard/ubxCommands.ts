export interface UbxCommand {
  id: string;
  name: string;
  category: string;
  description: string;
  hex: string;
}

export const ubxCategories = [
  "Receiver",
  "Satellites",
  "Navigation",
  "Configuration",
  "Raw",
];

export const ubxCommands: UbxCommand[] = [
  // Receiver Info
  {
    id: "mon-ver",
    name: "Version & Hardware",
    category: "Receiver",
    description: "Firmware version, hardware version, module name, extensions",
    hex: "B5620A06000000",
  },
  {
    id: "mon-hw",
    name: "Hardware Status",
    category: "Receiver",
    description: "Pin status, noise level, antenna status, jamming",
    hex: "B5620A09000000",
  },
  {
    id: "mon-rf",
    name: "RF Status",
    category: "Receiver",
    description: "Per-band jamming status, noise level, antenna status",
    hex: "B5620A38000000",
  },
  // Satellites
  {
    id: "nav-sat",
    name: "Satellite Details",
    category: "Satellites",
    description: "All visible satellites with elevation, azimuth, C/N0, health",
    hex: "B5620135000000",
  },
  {
    id: "nav-sig",
    name: "Signal Details",
    category: "Satellites",
    description: "Per-signal details (C/N0, quality, pseudorange residual)",
    hex: "B5620143000000",
  },
  {
    id: "nav-status",
    name: "GPS Status",
    category: "Satellites",
    description: "Fix type, fix validity, differential correction status",
    hex: "B5620103000000",
  },
  // Navigation
  {
    id: "nav-pvt",
    name: "Position/Velocity/Time",
    category: "Navigation",
    description: "Full PVT solution: position, velocity, time, accuracy",
    hex: "B5620107000000",
  },
  {
    id: "nav-dop",
    name: "Dilution of Precision",
    category: "Navigation",
    description: "Geometric, position, time, vertical DOP values",
    hex: "B5620104000000",
  },
  // Configuration
  {
    id: "cfg-rate-get",
    name: "Measurement Rate",
    category: "Configuration",
    description: "Current measurement rate and navigation rate",
    hex: "B5620608000000",
  },
  {
    id: "cfg-nav5-get",
    name: "Navigation Engine",
    category: "Configuration",
    description: "Navigation engine settings (dynModel, fixMode, etc.)",
    hex: "B5620624000000",
  },
  {
    id: "cfg-valget-port1",
    name: "UART1 Config",
    category: "Configuration",
    description: "UART1 protocol and baudrate configuration",
    hex: "B562068B09000001000010700001",
  },
  {
    id: "cfg-valget-sbas",
    name: "SBAS Settings",
    category: "Configuration",
    description: "SBAS enable/disable and mode",
    hex: "B562068B090000010000103A0001",
  },
  {
    id: "cfg-valget-rtcm",
    name: "RTCM Input",
    category: "Configuration",
    description: "RTCM protocol enable on UART1",
    hex: "B562068B09000001000010760001",
  },
  // Raw
  {
    id: "raw-custom",
    name: "Custom Hex Command",
    category: "Raw",
    description: "Enter any UBX hex command manually",
    hex: "",
  },
];

export function parseUbxMonVer(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  // Find UBX header in response
  let idx = 0;
  while (idx < bytes.length - 4) {
    if (bytes[idx] === 0xb5 && bytes[idx + 1] === 0x62) break;
    idx++;
  }
  if (idx >= bytes.length - 4) return null;
  // Check class/id = 0x0A 0x04 (MON-VER)
  if (bytes[idx + 2] !== 0x0a || bytes[idx + 3] !== 0x04) return null;

  const payloadStart = idx + 6;
  const sw = bytesToAscii(bytes, payloadStart, 30).replace(/\0/g, "");
  const hw = bytesToAscii(bytes, payloadStart + 30, 10).replace(/\0/g, "");

  const extensions: string[] = [];
  const extStart = payloadStart + 40;
  if (bytes.length > extStart) {
    const extCount = Math.floor((bytes.length - extStart) / 30);
    for (let i = 0; i < extCount; i++) {
      const ext = bytesToAscii(bytes, extStart + i * 30, 30).replace(/\0/g, "");
      if (ext) extensions.push(ext);
    }
  }

  return {
    Software: sw,
    Hardware: hw,
    Extensions: extensions.join(", ") || "none",
  };
}

export function parseUbxNavSat(
  hex: string,
): Array<Record<string, string | number>> | null {
  const bytes = hexToBytes(hex);
  if (bytes.length < 16) return null;
  if (bytes[0] !== 0xb5 || bytes[1] !== 0x62) return null;
  if (bytes[2] !== 0x01 || bytes[3] !== 0x35) return null;

  const numSats = bytes[6] | (bytes[7] << 8);
  const sats: Array<Record<string, string | number>> = [];
  for (let i = 0; i < numSats && 8 + i * 12 + 11 < bytes.length; i++) {
    const off = 8 + i * 12;
    const gnssId = bytes[off];
    const svId = bytes[off + 1];
    const elev = bytes[off + 5];
    const azim = bytes[off + 6] | (bytes[off + 7] << 8);
    const cno = bytes[off + 8];
    const qual = bytes[off + 2];
    const flags = bytes[off + 3];
    const health = bytes[off + 4];
    const used = (flags & 0x08) !== 0;

    sats.push({
      GNSS: gnssName(gnssId),
      SV: svId,
      Elev: `${elev}°`,
      Azim: `${azim}°`,
      "C/N0": `${cno} dB-Hz`,
      Quality: qual,
      Health: health,
      Used: used ? "Yes" : "No",
    });
  }
  return sats;
}

export function parseUbxCfgRate(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  if (bytes.length < 12) return null;
  if (bytes[0] !== 0xb5 || bytes[1] !== 0x62) return null;
  if (bytes[2] !== 0x06 || bytes[3] !== 0x08) return null;

  const measRate = bytes[6] | (bytes[7] << 8);
  const navRate = bytes[8] | (bytes[9] << 8);
  const timeRef = bytes[10] | (bytes[11] << 8);

  return {
    "Measurement Rate": `${measRate} ms (${(1000 / measRate).toFixed(1)} Hz)`,
    "Navigation Rate": `${navRate} cycles`,
    "Time Reference": timeRef === 0 ? "UTC" : timeRef === 1 ? "GPS" : "Other",
  };
}

export function parseUbxNavStatus(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  if (bytes.length < 12) return null;
  if (bytes[0] !== 0xb5 || bytes[1] !== 0x62) return null;
  if (bytes[2] !== 0x01 || bytes[3] !== 0x03) return null;

  const fix = bytes[6];
  const flags = bytes[7];
  const fixStat = bytes[8];
  const flags2 = bytes[9];
  const ttff =
    bytes[10] | (bytes[11] << 8) | (bytes[12] << 16) | (bytes[13] << 24);
  const msss =
    bytes[14] | (bytes[15] << 8) | (bytes[16] << 16) | (bytes[17] << 24);

  const fixNames: Record<number, string> = {
    0x00: "No fix",
    0x01: "Dead reckoning only",
    0x02: "2D fix",
    0x03: "3D fix",
    0x04: "GNSS + dead reckoning",
    0x05: "Time only fix",
  };

  return {
    "Fix Type": fixNames[fix] || `Unknown (${fix})`,
    "GPS Fix": fixStat & 0x01 ? "Valid" : "Invalid",
    "Diff Corr": fixStat & 0x02 ? "Applied" : "None",
    "Week Valid": flags2 & 0x01 ? "Yes" : "No",
    "TOW Valid": flags2 & 0x02 ? "Yes" : "No",
    TTFF: `${ttff} ms`,
    "Time Since Startup": `${msss} ms`,
  };
}

function hexToBytes(hex: string): number[] {
  const bytes: number[] = [];
  hex = hex.replace(/\s/g, "");
  for (let i = 0; i + 1 < hex.length; i += 2) {
    bytes.push(parseInt(hex.substring(i, i + 2), 16));
  }
  return bytes;
}

function bytesToAscii(bytes: number[], start: number, len: number): string {
  let s = "";
  for (let i = 0; i < len && start + i < bytes.length; i++) {
    const c = bytes[start + i];
    s += c >= 0x20 && c < 0x7f ? String.fromCharCode(c) : "\0";
  }
  return s;
}

function gnssName(id: number): string {
  const names: Record<number, string> = {
    0: "GPS",
    1: "SBAS",
    2: "Galileo",
    3: "BeiDou",
    4: "IMES",
    5: "QZSS",
    6: "GLONASS",
    7: "NavIC",
  };
  return names[id] || `GNSS-${id}`;
}

export function getParser(commandId: string) {
  switch (commandId) {
    case "mon-ver":
      return parseUbxMonVer;
    case "nav-sat":
      return parseUbxNavSat;
    case "cfg-rate-get":
      return parseUbxCfgRate;
    case "nav-status":
      return parseUbxNavStatus;
    default:
      return null;
  }
}
