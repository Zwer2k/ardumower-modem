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

// Build a UBX poll message from class/id and optional payload.
// Automatically appends the correct Fletcher checksum.
function ubxPoll(classId: number, msgId: number, payloadHex = ""): string {
  const payload = hexToBytes(payloadHex);
  const len = payload.length;
  const bytes: number[] = [
    0xb5,
    0x62,
    classId,
    msgId,
    len & 0xff,
    (len >> 8) & 0xff,
    ...payload,
  ];
  let ckA = 0,
    ckB = 0;
  for (let i = 2; i < bytes.length; i++) {
    ckA = (ckA + bytes[i]) & 0xff;
    ckB = (ckB + ckA) & 0xff;
  }
  bytes.push(ckA, ckB);
  return bytesToHexString(bytes);
}

function bytesToHexString(bytes: number[]): string {
  return bytes
    .map((b) => b.toString(16).toUpperCase().padStart(2, "0"))
    .join("");
}

// Build a CFG-VALGET poll message with multiple config keys.
// Keys are 32-bit u-blox config keys (e.g. 0x10700001).
function ubxCfgValGetPoll(keys: number[]): string {
  // version=0, layer=7 (RAM+BBR+Flash), position=0
  let payloadHex = "00070000";
  for (const key of keys) {
    const bytes = [
      key & 0xff,
      (key >> 8) & 0xff,
      (key >> 16) & 0xff,
      (key >> 24) & 0xff,
    ];
    payloadHex += bytes.map((b) => b.toString(16).padStart(2, "0")).join("");
  }
  return ubxPoll(0x06, 0x8b, payloadHex);
}

export const ubxCommands: UbxCommand[] = [
  // Receiver Info
  {
    id: "mon-ver",
    name: "Version & Hardware",
    category: "Receiver",
    description: "Firmware version, hardware version, module name, extensions",
    hex: ubxPoll(0x0a, 0x04),
  },
  {
    id: "mon-hw",
    name: "Hardware Status",
    category: "Receiver",
    description: "Pin status, noise level, antenna status, jamming",
    hex: ubxPoll(0x0a, 0x09),
  },
  {
    id: "mon-rf",
    name: "RF Status",
    category: "Receiver",
    description: "Per-band jamming status, noise level, antenna status",
    hex: ubxPoll(0x0a, 0x38),
  },
  {
    id: "mon-comms",
    name: "Port Traffic",
    category: "Receiver",
    description: "Per-port communication statistics (tx/rx bytes, usage)",
    hex: ubxPoll(0x0a, 0x36),
  },
  // Satellites
  {
    id: "nav-sat",
    name: "Satellite Details",
    category: "Satellites",
    description: "All visible satellites with elevation, azimuth, C/N0, health",
    hex: ubxPoll(0x01, 0x35),
  },
  {
    id: "nav-sig",
    name: "Signal Details",
    category: "Satellites",
    description: "Per-signal details (C/N0, quality, pseudorange residual)",
    hex: ubxPoll(0x01, 0x43),
  },
  {
    id: "nav-status",
    name: "GPS Status",
    category: "Satellites",
    description: "Fix type, fix validity, differential correction status",
    hex: ubxPoll(0x01, 0x03),
  },
  // Navigation
  {
    id: "nav-pvt",
    name: "Position/Velocity/Time",
    category: "Navigation",
    description: "Full PVT solution: position, velocity, time, accuracy",
    hex: ubxPoll(0x01, 0x07),
  },
  {
    id: "nav-dop",
    name: "Dilution of Precision",
    category: "Navigation",
    description: "Geometric, position, time, vertical DOP values",
    hex: ubxPoll(0x01, 0x04),
  },
  // Configuration
  {
    id: "cfg-rate-get",
    name: "Measurement Rate",
    category: "Configuration",
    description: "Current measurement rate and navigation rate",
    hex: ubxPoll(0x06, 0x08),
  },
  {
    id: "cfg-nav5-get",
    name: "Navigation Engine",
    category: "Configuration",
    description: "Navigation engine settings (dynModel, fixMode, etc.)",
    hex: ubxPoll(0x06, 0x24),
  },
  {
    id: "cfg-valget-port1",
    name: "UART1 Config",
    category: "Configuration",
    description: "UART1 protocol and baudrate configuration",
    hex: ubxPoll(0x06, 0x8b, "0100000010700001"),
  },
  {
    id: "cfg-valget-sbas",
    name: "SBAS Settings",
    category: "Configuration",
    description: "SBAS enable/disable and mode",
    hex: ubxPoll(0x06, 0x8b, "01000000103A0001"),
  },
  {
    id: "cfg-valget-rtcm",
    name: "RTCM Input",
    category: "Configuration",
    description: "RTCM protocol enable on UART1",
    hex: ubxPoll(0x06, 0x8b, "0100000010760001"),
  },
  {
    id: "cfg-valget-uart1-proto",
    name: "UART1 Protocols",
    category: "Configuration",
    description: "UART1 input/output protocol enables",
    hex: ubxCfgValGetPoll([
      0x10730001, 0x10730002, 0x10730003, 0x10740001, 0x10740002, 0x10740004,
    ]),
  },
  {
    id: "cfg-valget-gnss",
    name: "GNSS Signals",
    category: "Configuration",
    description: "Enabled GNSS systems (GPS, GLONASS, Galileo, BeiDou)",
    hex: ubxCfgValGetPoll([0x1031001f, 0x10310025, 0x10310021, 0x10310022]),
  },
  {
    id: "cfg-valget-rate",
    name: "Rate Settings",
    category: "Configuration",
    description: "Measurement and navigation rate",
    hex: ubxCfgValGetPoll([0x30210001, 0x30210002]),
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

// ─── Generic helpers ────────────────────────────────────────────────────────

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

export function findUbxFrames(hex: string): Array<{
  start: number;
  classId: number;
  msgId: number;
  length: number;
  payloadStart: number;
  payloadEnd: number;
  ckA: number;
  ckB: number;
  ckA_calc: number;
  ckB_calc: number;
  valid: boolean;
  className: string;
  msgName: string;
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
        ckA,
        ckB,
        ckA_calc: calc.ckA,
        ckB_calc: calc.ckB,
        valid: ckA === calc.ckA && ckB === calc.ckB,
        className: ubxClassName(classId),
        msgName: ubxMessageName(classId, msgId),
      });
      i = ckPos + 1;
    }
  }
  return frames;
}

function ubxClassName(c: number): string {
  const names: Record<number, string> = {
    0x01: "NAV",
    0x02: "RXM",
    0x04: "INF",
    0x05: "ACK",
    0x06: "CFG",
    0x0a: "MON",
    0x0d: "TIM",
    0x13: "MGA",
    0x21: "LOG",
    0x27: "SEC",
    0x28: "HNR",
  };
  return names[c] || `0x${c.toString(16).padStart(2, "0")}`;
}

function ubxMessageName(classId: number, msgId: number): string {
  const map: Record<string, string> = {
    // NAV (0x01)
    "0x1-0x1": "NAV-POSECEF",
    "0x1-0x2": "NAV-POSLLH",
    "0x1-0x3": "NAV-STATUS",
    "0x1-0x4": "NAV-DOP",
    "0x1-0x5": "NAV-ATT",
    "0x1-0x6": "NAV-SOL",
    "0x1-0x7": "NAV-PVT",
    "0x1-0x9": "NAV-ODO",
    "0x1-0x10": "NAV-RESETODO",
    "0x1-0x11": "NAV-ORB",
    "0x1-0x12": "NAV-DGPS",
    "0x1-0x13": "NAV-SBAS",
    "0x1-0x14": "NAV-ODO", // or NAV-NMI in some versions
    "0x1-0x17": "NAV-SVIN",
    "0x1-0x18": "NAV-RELPOSNED",
    "0x1-0x20": "NAV-EOE",
    "0x1-0x22": "NAV-CLOCK",
    "0x1-0x23": "NAV-TIMEGPS",
    "0x1-0x24": "NAV-TIMEUTC",
    "0x1-0x25": "NAV-COV",
    "0x1-0x26": "NAV-VELNED",
    "0x1-0x30": "NAV-SLAS",
    "0x1-0x32": "NAV-SIG",
    "0x1-0x34": "NAV-SAT",
    "0x1-0x35": "NAV-SAT", // old ID
    "0x1-0x36": "NAV-SAT",
    "0x1-0x39": "NAV-GEOFENCE",
    "0x1-0x3b": "NAV-SVINFO",
    "0x1-0x3c": "NAV-RELPOSNED",
    "0x1-0x43": "NAV-SIG",
    "0x1-0x60": "NAV-AOPSTATUS",
    "0x1-0x61": "NAV-PSD",
    // RXM (0x02)
    "0x2-0x13": "RXM-SFRBX",
    "0x2-0x14": "RXM-MEASX",
    "0x2-0x15": "RXM-RAWX",
    "0x2-0x32": "RXM-RTCM",
    "0x2-0x41": "RXM-PMP",
    // MON (0x0a)
    "0xa-0x2": "MON-IO",
    "0xa-0x4": "MON-VER",
    "0xa-0x6": "MON-MSGPP",
    "0xa-0x7": "MON-RXBUF",
    "0xa-0x8": "MON-TXBUF",
    "0xa-0x9": "MON-HW",
    "0xa-0xb": "MON-HW2",
    "0xa-0x21": "MON-RXR",
    "0xa-0x27": "MON-PATCH",
    "0xa-0x28": "MON-GNSS",
    "0xa-0x32": "MON-SMGR",
    "0xa-0x36": "MON-COMMS",
    "0xa-0x38": "MON-RF",
    "0xa-0x39": "MON-SPAN",
    "0xa-0x3a": "MON-SYS",
    // CFG (0x06)
    "0x6-0x0": "CFG-PRT",
    "0x6-0x1": "CFG-MSG",
    "0x6-0x8": "CFG-RATE",
    "0x6-0x9": "CFG-CFG",
    "0x6-0x11": "CFG-RST",
    "0x6-0x13": "CFG-DAT",
    "0x6-0x16": "CFG-NAV5",
    "0x6-0x17": "CFG-RINV",
    "0x6-0x1e": "CFG-GNSS",
    "0x6-0x23": "CFG-LOGFILTER",
    "0x6-0x24": "CFG-NAVX5",
    "0x6-0x31": "CFG-TP5",
    "0x6-0x34": "CFG-RATE",
    "0x6-0x3b": "CFG-NAV5",
    "0x6-0x3e": "CFG-GNSS",
    "0x6-0x62": "CFG-ITFM",
    "0x6-0x69": "CFG-DGNSS",
    "0x6-0x70": "CFG-GEOFENCE",
    "0x6-0x71": "CFG-DOSC",
    "0x6-0x72": "CFG-ESRC",
    "0x6-0x84": "CFG-SMGR",
    "0x6-0x8b": "CFG-VALGET",
    "0x6-0x8c": "CFG-VALSET",
    "0x6-0x8d": "CFG-VALDEL",
  };
  const key = `0x${classId.toString(16)}-0x${msgId.toString(16)}`;
  return map[key] || `ID-0x${msgId.toString(16).padStart(2, "0")}`;
}

export function hexDump(hex: string, startOffset: number = 0): string {
  const bytes = hexToBytes(hex);
  const lines: string[] = [];
  for (let i = 0; i < bytes.length; i += 16) {
    const chunk = bytes.slice(i, i + 16);
    const hexStr = chunk.map((b) => b.toString(16).padStart(2, "0")).join(" ");
    const asciiStr = chunk
      .map((b) => (b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : "."))
      .join("");
    lines.push(
      `${(startOffset + i).toString(16).padStart(4, "0")}  ${hexStr.padEnd(48, " ")}  |${asciiStr}|`,
    );
  }
  return lines.join("\n");
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

// ─── Generic frame parser ───────────────────────────────────────────────────

export function parseUbxGeneric(hex: string): Record<string, string> | null {
  const frames = findUbxFrames(hex);
  if (frames.length === 0) return null;
  const f = frames[0];
  return {
    "UBX Class": `${f.className} (0x${f.classId.toString(16).padStart(2, "0")})`,
    "UBX Message": `${f.msgName} (0x${f.msgId.toString(16).padStart(2, "0")})`,
    "Payload Length": `${f.length} bytes`,
    Checksum: `${f.valid ? "✓ Valid" : "✗ Invalid"} (recv ${f.ckA.toString(16).toUpperCase().padStart(2, "0")} ${f.ckB.toString(16).toUpperCase().padStart(2, "0")} / calc ${f.ckA_calc.toString(16).toUpperCase().padStart(2, "0")} ${f.ckB_calc.toString(16).toUpperCase().padStart(2, "0")})`,
    "Total Frames": `${frames.length}`,
  };
}

// ─── Specific parsers ───────────────────────────────────────────────────────

export function parseUbxMonVer(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const frames = findUbxFrames(hex);
  const f = frames.find((x) => x.classId === 0x0a && x.msgId === 0x04);
  if (!f) return null;

  const p = f.payloadStart;
  const sw = bytesToAscii(bytes, p, 30).replace(/\0/g, "");
  const hw = bytesToAscii(bytes, p + 30, 10).replace(/\0/g, "");
  const rom = bytesToAscii(bytes, p + 40, 30).replace(/\0/g, "");

  const extensions: string[] = [];
  const extStart = p + 70; // SW=30, HW=10, ROM=30 => extensions start at 70
  if (bytes.length > extStart + 30) {
    const extCount = Math.floor((bytes.length - extStart) / 30);
    for (let i = 0; i < extCount; i++) {
      const ext = bytesToAscii(bytes, extStart + i * 30, 30).replace(/\0/g, "");
      if (ext) extensions.push(ext);
    }
  }

  return {
    "Software Version": sw,
    "Hardware Version": hw || "—",
    "ROM Version": rom || "—",
    Extensions: extensions.join(", ") || "none",
  };
}

export function parseUbxMonHw(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x0a && x.msgId === 0x09,
  );
  if (!f || f.length < 20) return null;
  const p = f.payloadStart;

  const pinSel =
    bytes[p] |
    (bytes[p + 1] << 8) |
    (bytes[p + 2] << 16) |
    (bytes[p + 3] << 24);
  const pinBank =
    bytes[p + 4] |
    (bytes[p + 5] << 8) |
    (bytes[p + 6] << 16) |
    (bytes[p + 7] << 24);
  const pinDir =
    bytes[p + 8] |
    (bytes[p + 9] << 8) |
    (bytes[p + 10] << 16) |
    (bytes[p + 11] << 24);
  const pinVal =
    bytes[p + 12] |
    (bytes[p + 13] << 8) |
    (bytes[p + 14] << 16) |
    (bytes[p + 15] << 24);
  const noisePerMS = bytes[p + 16] | (bytes[p + 17] << 8);
  const agcCnt = bytes[p + 18];
  const antStatus = bytes[p + 20] & 0x03;
  const antPower = (bytes[p + 20] >> 2) & 0x03;
  const jamInd = bytes[p + 21];

  const antStatusNames = ["Init", "Unknown", "OK", "Short"];
  const antPowerNames = ["Off", "On", "DontKnow"];

  return {
    "Noise Level": `${noisePerMS} / 255`,
    AGC: `${agcCnt} / 255`,
    "Antenna Status": antStatusNames[antStatus] || `Code ${antStatus}`,
    "Antenna Power": antPowerNames[antPower] || `Code ${antPower}`,
    "Jamming Indicator": `${jamInd} / 255`,
    "Pin Sel": `0x${pinSel.toString(16).toUpperCase()}`,
    "Pin Bank": `0x${pinBank.toString(16).toUpperCase()}`,
    "Pin Dir": `0x${pinDir.toString(16).toUpperCase()}`,
    "Pin Val": `0x${pinVal.toString(16).toUpperCase()}`,
  };
}

export function parseUbxMonRf(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x0a && x.msgId === 0x38,
  );
  if (!f || f.length < 4) return null;
  const p = f.payloadStart;
  const nBlocks = bytes[p];
  const result: Record<string, string> = {};
  for (let i = 0; i < nBlocks; i++) {
    const off = p + 4 + i * 24;
    if (off + 24 > bytes.length) break;
    const blockId = bytes[off];
    const jamState = bytes[off + 1];
    const antStatus = bytes[off + 2] & 0x03;
    const antPower = (bytes[off + 2] >> 2) & 0x03;
    const postStatus = bytes[off + 3];
    const noisePerMS = bytes[off + 4] | (bytes[off + 5] << 8);
    const agcCnt = bytes[off + 6];
    const jamInd = bytes[off + 7];
    const ofsI = bytes[off + 8];
    const magI = bytes[off + 9];
    const ofsQ = bytes[off + 10];
    const magQ = bytes[off + 11];

    const jamNames: Record<number, string> = {
      0: "Unknown",
      1: "OK",
      2: "Warning",
      3: "Critical",
    };
    const antStatusNames = ["Init", "Unknown", "OK", "Short"];
    const antPowerNames = ["Off", "On", "DontKnow"];

    result[`Block ${blockId + 1}`] =
      `Noise ${noisePerMS}, AGC ${agcCnt}, Jam ${jamNames[jamState] || jamState}, Ant ${antStatusNames[antStatus]}, Pwr ${antPowerNames[antPower]}`;
    result[`  ofsI/magI`] = `${ofsI} / ${magI}`;
    result[`  ofsQ/magQ`] = `${ofsQ} / ${magQ}`;
    result[`  postStatus`] = `${postStatus}`;
  }
  result["Block Count"] = `${nBlocks}`;
  return result;
}

export function parseUbxMonComms(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x0a && x.msgId === 0x36,
  );
  if (!f || f.length < 4) return null;
  const p = f.payloadStart;

  const version = bytes[p];
  const nPorts = bytes[p + 1];
  const portSize = version === 0 ? 8 : 12;

  const portNames: Record<number, string> = {
    0: "I2C",
    1: "UART1",
    2: "UART2",
    3: "USB",
    4: "SPI",
  };

  const result: Record<string, string> = {};
  result["Version"] = `${version}`;
  result["Ports"] = `${nPorts}`;

  for (let i = 0; i < nPorts; i++) {
    const off = p + 4 + i * portSize;
    if (off + portSize > bytes.length) break;
    const portId = bytes[off] | (bytes[off + 1] << 8);
    const txBytes = readU4(bytes, off + 4);
    const txUsage = bytes[off + 8];
    const txPeak = bytes[off + 9];
    const rxBytes = readU4(bytes, off + 12);
    const rxUsage = bytes[off + 16];
    const rxPeak = bytes[off + 17];
    const overrun = bytes[off + 18] | (bytes[off + 19] << 8);

    const name = portNames[portId] || `Port${portId}`;
    result[`${name} TX`] = `${txBytes} B (${txUsage}% peak ${txPeak}%)`;
    result[`${name} RX`] = `${rxBytes} B (${rxUsage}% peak ${rxPeak}%)`;
    if (overrun > 0) {
      result[`${name} Overrun`] = `${overrun}`;
    }
  }

  return result;
}

export function parseUbxNavSat(
  hex: string,
): Array<Record<string, string | number>> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x35,
  );
  if (!f || f.length < 8) return null;
  const p = f.payloadStart;
  const numSats = bytes[p + 4] | (bytes[p + 5] << 8);
  const sats: Array<Record<string, string | number>> = [];
  for (let i = 0; i < numSats && p + 8 + i * 12 + 11 < bytes.length; i++) {
    const off = p + 8 + i * 12;
    const gnssId = bytes[off];
    const svId = bytes[off + 1];
    const cno = bytes[off + 2];
    const elev = bytesToSigned(bytes, off + 3);
    const azim = bytes[off + 4] | (bytes[off + 5] << 8);
    const prRes = bytesToSigned(bytes, off + 6) * 0.1;
    const flags =
      bytes[off + 8] |
      (bytes[off + 9] << 8) |
      (bytes[off + 10] << 16) |
      (bytes[off + 11] << 24);
    const qual = flags & 0x07;
    const used = (flags & 0x40) !== 0;
    const health = (flags >> 8) & 0x03;
    const diffCorr = (flags >> 10) & 0x01;
    const smoothed = (flags >> 11) & 0x01;

    sats.push({
      GNSS: gnssName(gnssId),
      SV: svId,
      Elev: `${elev}°`,
      Azim: `${azim}°`,
      "C/N0": `${cno} dB-Hz`,
      Quality: qual,
      Health: health,
      Used: used ? "Yes" : "No",
      "Diff Corr": diffCorr ? "Yes" : "No",
      Smoothed: smoothed ? "Yes" : "No",
      "PR Res": `${prRes.toFixed(1)} m`,
    });
  }
  return sats;
}

export function parseUbxNavSig(
  hex: string,
): Array<Record<string, string | number>> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x43,
  );
  if (!f || f.length < 8) return null;
  const p = f.payloadStart;
  const numSigs = bytes[p + 4] | (bytes[p + 5] << 8);
  const sigs: Array<Record<string, string | number>> = [];
  for (let i = 0; i < numSigs && p + 8 + i * 16 + 15 < bytes.length; i++) {
    const off = p + 8 + i * 16;
    const gnssId = bytes[off];
    const svId = bytes[off + 1];
    const sigId = bytes[off + 2];
    const freqId = bytes[off + 3];
    const prRes = bytesToSigned(bytes, off + 4) * 0.1;
    const cno = bytes[off + 6];
    const qual = bytes[off + 7] & 0x07;
    const corrSrc = (bytes[off + 7] >> 3) & 0x07;
    const iono = (bytes[off + 7] >> 6) & 0x03;
    const health = bytes[off + 8] & 0x03;
    const prSmoothed = (bytes[off + 8] >> 2) & 0x01;
    const prUsed = (bytes[off + 8] >> 3) & 0x01;
    const crUsed = (bytes[off + 8] >> 4) & 0x01;
    const doUsed = (bytes[off + 8] >> 5) & 0x01;
    const prCorrUsed = (bytes[off + 8] >> 6) & 0x01;
    const crCorrUsed = (bytes[off + 8] >> 7) & 0x01;
    const doCorrUsed = (bytes[off + 9] >> 0) & 0x01;

    sigs.push({
      GNSS: gnssName(gnssId),
      SV: svId,
      Signal: sigId,
      "C/N0": `${cno} dB-Hz`,
      Quality: qual,
      "PR Res": `${prRes.toFixed(1)} m`,
      Used: prUsed ? "Yes" : "No",
      "Diff Corr": crUsed ? "Yes" : "No",
      Health: health,
    });
  }
  return sigs;
}

export function parseUbxNavStatus(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x03,
  );
  if (!f || f.length < 16) return null;
  const p = f.payloadStart;

  const fix = bytes[p];
  const flags = bytes[p + 1];
  const fixStat = bytes[p + 2];
  const flags2 = bytes[p + 3];
  const ttff = readU4(bytes, p + 4);
  const msss = readU4(bytes, p + 8);

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
    "GPS Fix Valid": fixStat & 0x01 ? "Yes" : "No",
    "Diff Corr Applied": fixStat & 0x02 ? "Yes" : "No",
    "Week Valid": flags2 & 0x01 ? "Yes" : "No",
    "TOW Valid": flags2 & 0x02 ? "Yes" : "No",
    TTFF: `${ttff} ms`,
    "Time Since Startup": `${msss} ms`,
    "PSM State": `${(flags >> 2) & 0x07}`,
  };
}

export function parseUbxNavPvt(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x07,
  );
  if (!f || f.length < 92) return null;
  const p = f.payloadStart;

  const year = bytes[p + 4] | (bytes[p + 5] << 8);
  const month = bytes[p + 6];
  const day = bytes[p + 7];
  const hour = bytes[p + 8];
  const minute = bytes[p + 9];
  const second = bytes[p + 10];
  const validDate = bytes[p + 11] & 0x01;
  const validTime = bytes[p + 11] & 0x02;
  const fullyResolved = bytes[p + 11] & 0x04;
  const validMag = bytes[p + 11] & 0x08;

  const fixType = bytes[p + 20];
  const fixOk = bytes[p + 21] & 0x01;
  const diffSoln = bytes[p + 21] & 0x02;
  const carrSoln = (bytes[p + 21] >> 6) & 0x03;

  const numSV = bytes[p + 23];
  const lon = readI4(bytes, p + 24) * 1e-7;
  const lat = readI4(bytes, p + 28) * 1e-7;
  const height = readI4(bytes, p + 32) * 1e-3;
  const hMSL = readI4(bytes, p + 36) * 1e-3;
  const hAcc = readU4(bytes, p + 40) * 1e-3;
  const vAcc = readU4(bytes, p + 44) * 1e-3;
  const velN = readI4(bytes, p + 48) * 1e-3;
  const velE = readI4(bytes, p + 52) * 1e-3;
  const velD = readI4(bytes, p + 56) * 1e-3;
  const gSpeed = readI4(bytes, p + 60) * 1e-3;
  const heading = readI4(bytes, p + 64) * 1e-5;
  const sAcc = readU4(bytes, p + 68) * 1e-3;
  const headingAcc = readU4(bytes, p + 72) * 1e-5;
  const pDOP = readU2(bytes, p + 76) * 0.01;

  const fixNames: Record<number, string> = {
    0: "No fix",
    1: "Dead reckoning",
    2: "2D fix",
    3: "3D fix",
    4: "GNSS + DR",
    5: "Time only",
  };

  const carrNames: Record<number, string> = {
    0: "None",
    1: "Float",
    2: "Fixed",
  };

  return {
    "Date/Time": `${year}-${String(month).padStart(2, "0")}-${String(day).padStart(2, "0")} ${String(hour).padStart(2, "0")}:${String(minute).padStart(2, "0")}:${String(second).padStart(2, "0")}`,
    "Date Valid": validDate ? "Yes" : "No",
    "Time Valid": validTime ? "Yes" : "No",
    "Fix Type": fixNames[fixType] || `Unknown (${fixType})`,
    "Fix OK": fixOk ? "Yes" : "No",
    "RTK Status": carrNames[carrSoln] || `Code ${carrSoln}`,
    "Diff Solution": diffSoln ? "Yes" : "No",
    Satellites: `${numSV}`,
    Latitude: `${lat.toFixed(7)}°`,
    Longitude: `${lon.toFixed(7)}°`,
    "Height (MSL)": `${hMSL.toFixed(3)} m`,
    "Height (Ellipsoid)": `${height.toFixed(3)} m`,
    "H-Accuracy": `${hAcc.toFixed(3)} m`,
    "V-Accuracy": `${vAcc.toFixed(3)} m`,
    "Speed (Ground)": `${gSpeed.toFixed(3)} m/s`,
    "Speed (3D)": `${Math.sqrt(velN * velN + velE * velE + velD * velD).toFixed(3)} m/s`,
    Heading: `${heading.toFixed(2)}°`,
    "Speed Accuracy": `${sAcc.toFixed(3)} m/s`,
    "Heading Accuracy": `${headingAcc.toFixed(2)}°`,
    PDOP: `${pDOP.toFixed(2)}`,
  };
}

export function parseUbxNavDop(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x04,
  );
  if (!f || f.length < 18) return null;
  const p = f.payloadStart;

  const gDOP = readU2(bytes, p + 0) * 0.01;
  const pDOP = readU2(bytes, p + 2) * 0.01;
  const tDOP = readU2(bytes, p + 4) * 0.01;
  const vDOP = readU2(bytes, p + 6) * 0.01;
  const hDOP = readU2(bytes, p + 8) * 0.01;
  const nDOP = readU2(bytes, p + 10) * 0.01;
  const eDOP = readU2(bytes, p + 12) * 0.01;

  return {
    GDOP: `${gDOP.toFixed(2)}`,
    PDOP: `${pDOP.toFixed(2)}`,
    TDOP: `${tDOP.toFixed(2)}`,
    VDOP: `${vDOP.toFixed(2)}`,
    HDOP: `${hDOP.toFixed(2)}`,
    NDOP: `${nDOP.toFixed(2)}`,
    EDOP: `${eDOP.toFixed(2)}`,
  };
}

export function parseUbxCfgRate(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x06 && x.msgId === 0x08,
  );
  if (!f || f.length < 6) return null;
  const p = f.payloadStart;

  const measRate = readU2(bytes, p + 0);
  const navRate = readU2(bytes, p + 2);
  const timeRef = readU2(bytes, p + 4);

  return {
    "Measurement Rate": `${measRate} ms (${(1000 / measRate).toFixed(1)} Hz)`,
    "Navigation Rate": `${navRate} cycles`,
    "Time Reference":
      timeRef === 0 ? "UTC" : timeRef === 1 ? "GPS" : `Code ${timeRef}`,
  };
}

export function parseUbxCfgNav5(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x06 && x.msgId === 0x24,
  );
  if (!f || f.length < 36) return null;
  const p = f.payloadStart;

  const dynModel = bytes[p + 2];
  const fixMode = bytes[p + 3];
  const fixedAlt = readI4(bytes, p + 4) * 0.01;
  const fixedAltVar = readU4(bytes, p + 8) * 0.0001;
  const minElev = bytesToSigned(bytes, p + 12);
  const drLimit = bytes[p + 13];
  const pDop = readU2(bytes, p + 14) * 0.1;
  const tDop = readU2(bytes, p + 16) * 0.1;
  const pAcc = readU2(bytes, p + 18);
  const tAcc = readU2(bytes, p + 20);
  const staticHoldThresh = bytes[p + 22];
  const dgnssTimeout = bytes[p + 23];
  const cnoThreshNumSats = bytes[p + 24];
  const cnoThresh = bytes[p + 25];
  const staticHoldMaxDist = readU2(bytes, p + 28);

  const dynNames: Record<number, string> = {
    0: "Portable",
    2: "Stationary",
    3: "Pedestrian",
    4: "Automotive",
    5: "Sea",
    6: "Airborne <1g",
    7: "Airborne <2g",
    8: "Airborne <4g",
    9: "Wrist",
    10: "Bike",
  };

  const fixNames: Record<number, string> = {
    1: "2D only",
    2: "3D only",
    3: "Auto 2D/3D",
  };

  return {
    "Dynamic Model": dynNames[dynModel] || `Code ${dynModel}`,
    "Fix Mode": fixNames[fixMode] || `Code ${fixMode}`,
    "Fixed Altitude": `${fixedAlt.toFixed(2)} m`,
    "Fixed Alt Var": `${fixedAltVar.toFixed(4)} m²`,
    "Min Elevation": `${minElev}°`,
    "DR Limit": `${drLimit} s`,
    "Max PDOP": `${pDop.toFixed(1)}`,
    "Max TDOP": `${tDop.toFixed(1)}`,
    "Pos Accuracy": `${pAcc} m`,
    "Time Accuracy": `${tAcc} m`,
    "Static Hold": `${staticHoldThresh} cm/s`,
    "Static Max Dist": `${staticHoldMaxDist} m`,
    "DGNSS Timeout": `${dgnssTimeout} s`,
    "C/N0 Threshold": `${cnoThreshNumSats} sats @ ${cnoThresh} dB-Hz`,
  };
}

export function parseUbxCfgValget(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x06 && x.msgId === 0x8b,
  );
  if (!f || f.length < 4) return null;
  const p = f.payloadStart;

  const version = bytes[p];
  const layers = bytes[p + 1];
  const layerNames: Record<number, string> = {
    0: "RAM",
    1: "BBR",
    2: "Flash",
    7: "Default",
  };
  const activeLayer = layerNames[layers & 0x07] || `Code ${layers & 0x07}`;

  const result: Record<string, string> = {
    Version: `${version}`,
    "Active Layer": activeLayer,
  };

  let off = p + 4;
  const cfgEntries: string[] = [];
  while (off + 4 < bytes.length) {
    const key =
      bytes[off] |
      (bytes[off + 1] << 8) |
      (bytes[off + 2] << 16) |
      (bytes[off + 3] << 24);
    off += 4;
    const cfgName = ubxConfigKeyName(key);
    if (off + 1 <= bytes.length) {
      const val = bytes[off];
      off += 1;
      cfgEntries.push(`${cfgName || `0x${key.toString(16)}`} = ${val}`);
    } else {
      cfgEntries.push(`${cfgName || `0x${key.toString(16)}`} = ?`);
      break;
    }
  }
  result["Config Entries"] = cfgEntries.join("; ") || "none";
  return result;
}

// ─── Low-level helpers ────────────────────────────────────────────────────────

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

function ubxConfigKeyName(key: number): string | null {
  const map: Record<number, string> = {
    0x10700001: "CFG-UART1-BAUDRATE",
    0x10760001: "CFG-UART1INPROT-RTCM3X",
    0x103a0001: "CFG-SBAS-ENABLE",
    0x10730001: "CFG-UART1-ENABLED",
    0x10700002: "CFG-UART1-STOPBITS",
    0x10700003: "CFG-UART1-DATABITS",
    0x10700004: "CFG-UART1-PARITY",
    0x10730002: "CFG-UART1INPROT-NMEA",
    0x10730003: "CFG-UART1INPROT-UBX",
    0x10740001: "CFG-UART1OUTPROT-UBX",
    0x10740002: "CFG-UART1OUTPROT-NMEA",
    0x10740004: "CFG-UART1OUTPROT-RTCM3X",
    0x1031001f: "CFG-SIGNAL-GPS_ENA",
    0x10310025: "CFG-SIGNAL-GLO_ENA",
    0x10310021: "CFG-SIGNAL-GAL_ENA",
    0x10310022: "CFG-SIGNAL-BDS_ENA",
    0x30210001: "CFG-RATE-MEAS",
    0x30210002: "CFG-RATE-NAV",
  };
  return map[key] || null;
}

export interface UbxFrameResult {
  classId: number;
  msgId: number;
  className: string;
  msgName: string;
  length: number;
  payloadStart: number;
  payloadEnd: number;
  valid: boolean;
  specific:
    | Record<string, string>
    | Array<Record<string, string | number>>
    | null;
}

export function parseUbxFrames(hex: string): UbxFrameResult[] {
  const frames = findUbxFrames(hex);
  return frames.map((f) => {
    const parser = getParserForFrame(f.classId, f.msgId);
    let specific = null;
    if (parser) {
      try {
        specific = parser(hex);
      } catch (e) {
        specific = { "Parse Error": String(e) };
      }
    }
    return {
      classId: f.classId,
      msgId: f.msgId,
      className: f.className,
      msgName: f.msgName,
      length: f.length,
      payloadStart: f.payloadStart,
      payloadEnd: f.payloadEnd,
      valid: f.valid,
      specific,
    };
  });
}

// ─── Parser dispatcher ────────────────────────────────────────────────────────

export function parseUbxNavOdo(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x14,
  );
  if (!f || f.length < 20) return null;
  const p = f.payloadStart;
  const iTOW = readU4(bytes, p);
  const version = bytes[p + 4];
  const distance = readU4(bytes, p + 8);
  const totalDistance = readU4(bytes, p + 12);
  const distanceStd = readU4(bytes, p + 16);
  return {
    "Time of Week": `${iTOW} ms`,
    Version: `${version}`,
    Distance: `${distance} m`,
    "Total Distance": `${totalDistance} m`,
    "Distance StdDev": `${distanceStd} m`,
  };
}

export function parseUbxNavRelposned(
  hex: string,
): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x3c,
  );
  if (!f || f.length < 40) return null;
  const p = f.payloadStart;
  const iTOW = readU4(bytes, p);
  const version = bytes[p + 4];
  const refStationId = readU2(bytes, p + 6);
  const relPosN = readI4(bytes, p + 8) * 0.01;
  const relPosE = readI4(bytes, p + 12) * 0.01;
  const relPosD = readI4(bytes, p + 16) * 0.01;
  const relPosLength = readI4(bytes, p + 20) * 0.01;
  const relPosHeading = readI4(bytes, p + 24) * 1e-5;
  const relPosNHP = bytesToSigned(bytes, p + 28) * 0.001;
  const relPosEHP = bytesToSigned(bytes, p + 29) * 0.001;
  const relPosDHP = bytesToSigned(bytes, p + 30) * 0.001;
  const relPosLengthHP = bytesToSigned(bytes, p + 31) * 0.001;
  const relPosHeadingHP = bytesToSigned(bytes, p + 32) * 1e-5;
  const accN = readU4(bytes, p + 36) * 0.01;
  const accE = readU4(bytes, p + 40) * 0.01;
  const accD = readU4(bytes, p + 44) * 0.01;
  const accLength = readU4(bytes, p + 48) * 0.01;
  const flags = readU4(bytes, p + 56);
  const gnssFixOk = flags & 0x01;
  const diffSoln = (flags >> 1) & 0x01;
  const relPosValid = (flags >> 2) & 0x01;
  const carrSoln = (flags >> 3) & 0x03;
  const isMoving = (flags >> 5) & 0x01;
  const refPosMiss = (flags >> 6) & 0x01;
  const refObsMiss = (flags >> 7) & 0x01;

  const carrNames: Record<number, string> = {
    0: "None",
    1: "Float",
    2: "Fixed",
  };

  return {
    "Time of Week": `${iTOW} ms`,
    Version: `${version}`,
    "Ref Station ID": `${refStationId}`,
    "Rel Pos N": `${(relPosN + relPosNHP).toFixed(3)} cm`,
    "Rel Pos E": `${(relPosE + relPosEHP).toFixed(3)} cm`,
    "Rel Pos D": `${(relPosD + relPosDHP).toFixed(3)} cm`,
    "Baseline Length": `${(relPosLength + relPosLengthHP).toFixed(3)} cm`,
    Heading: `${(relPosHeading + relPosHeadingHP).toFixed(4)}°`,
    "Accuracy N": `${accN.toFixed(2)} cm`,
    "Accuracy E": `${accE.toFixed(2)} cm`,
    "Accuracy D": `${accD.toFixed(2)} cm`,
    "Accuracy Length": `${accLength.toFixed(2)} cm`,
    "Fix OK": gnssFixOk ? "Yes" : "No",
    "Diff Soln": diffSoln ? "Yes" : "No",
    "Rel Pos Valid": relPosValid ? "Yes" : "No",
    "Carr Soln": carrNames[carrSoln] || `Code ${carrSoln}`,
    Moving: isMoving ? "Yes" : "No",
  };
}

export function parseUbxNavDgps(hex: string): Record<string, string> | null {
  const bytes = hexToBytes(hex);
  const f = findUbxFrames(hex).find(
    (x) => x.classId === 0x01 && x.msgId === 0x12,
  );
  if (!f || f.length < 16) return null;
  const p = f.payloadStart;
  const iTOW = readU4(bytes, p);
  const age = readI4(bytes, p + 4);
  const baseId = readI2(bytes, p + 8);
  const baseHealth = readI2(bytes, p + 10);
  const numCh = bytes[p + 12];
  const status = bytes[p + 13];
  const statusNames: Record<number, string> = {
    0: "None",
    1: "PR+PRRC correction",
    2: "PR+PRRC+CP correction",
  };
  return {
    "Time of Week": `${iTOW} ms`,
    Age: `${age} ms`,
    "Base Station ID": `${baseId}`,
    "Base Health": `${baseHealth}`,
    Channels: `${numCh}`,
    Status: statusNames[status] || `Code ${status}`,
  };
}

export function getParserForFrame(
  classId: number,
  msgId: number,
):
  | ((
      hex: string,
    ) => Record<string, string> | Array<Record<string, string | number>> | null)
  | null {
  const key = `${classId}-${msgId}`;
  switch (key) {
    case "10-4":
      return parseUbxMonVer;
    case "10-9":
      return parseUbxMonHw;
    case "10-56":
      return parseUbxMonRf;
    case "10-54":
      return parseUbxMonComms;
    case "1-20":
      return parseUbxNavOdo;
    case "1-18":
      return parseUbxNavDgps;
    case "1-60":
      return parseUbxNavRelposned;
    case "1-53":
      return parseUbxNavSat;
    case "1-67":
      return parseUbxNavSig;
    case "1-3":
      return parseUbxNavStatus;
    case "1-7":
      return parseUbxNavPvt;
    case "1-4":
      return parseUbxNavDop;
    case "6-8":
      return parseUbxCfgRate;
    case "6-36":
      return parseUbxCfgNav5;
    case "6-139":
      return parseUbxCfgValget;
    default:
      return null;
  }
}

export function getParser(commandId: string) {
  switch (commandId) {
    case "mon-ver":
      return parseUbxMonVer;
    case "mon-hw":
      return parseUbxMonHw;
    case "mon-rf":
      return parseUbxMonRf;
    case "nav-sat":
      return parseUbxNavSat;
    case "nav-sig":
      return parseUbxNavSig;
    case "nav-status":
      return parseUbxNavStatus;
    case "nav-pvt":
      return parseUbxNavPvt;
    case "nav-dop":
      return parseUbxNavDop;
    case "cfg-rate-get":
      return parseUbxCfgRate;
    case "cfg-nav5-get":
      return parseUbxCfgNav5;
    case "cfg-valget-port1":
    case "cfg-valget-sbas":
    case "cfg-valget-rtcm":
      return parseUbxCfgValget;
    default:
      return null;
  }
}
