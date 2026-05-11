export namespace Settings {
  export interface General {
    name: string;
    encryption: boolean;
    password: number;
    has_password: boolean;
  }
  export interface Web {
    protected: boolean;
    username: string;
    password: string;
    has_password: boolean;
  }
  export interface WiFi {
    mode: "sta" | "ap" | "off";
    sta_ssid: string;
    sta_psk: string;
    has_sta_psk: boolean;
    ap_ssid: string;
    ap_psk: string;
    has_ap_psk: boolean;
  }
  export interface Bluetooth {
    enabled: boolean;
    pin_enabled: boolean;
    pin: number;
    has_pin: boolean;
  }

  export interface PS4Controller {
    enabled: boolean;
    use_ps4_mac: boolean;
    ps4_mac: string;
  }
  export interface Mqtt {
    enabled: boolean;
    server: string;
    prefix: string;
    username: string;
    password: string;
    has_password: boolean;
    publish_status: boolean;
    publish_format: "json" | "text" | "both";
    publish_interval: number;
    ha: boolean;
    iob: boolean;
  }
  export interface Prometheus {
    enabled: boolean;
  }
}

export interface Settings {
  initialized: boolean;
  revision: number;
  general: Settings.General;
  web: Settings.Web;
  wifi: Settings.WiFi;
  bluetooth: Settings.Bluetooth;
  ps4controller: Settings.PS4Controller;
  mqtt: Settings.Mqtt;
  prometheus: Settings.Prometheus;
}

export interface Info {
  git_hash: string;
  git_time: string;
  git_tag: string;
  uptime: number;
  bt_mac: string;
}

export interface Status {
  uptime: number;
}

export interface ChangeEventValue {
  event: Event;
  value: any;
}

export interface Point {
  x: number;
  y: number;
}

export interface Position extends Point {
  delta: number;
  solution: number;
  age: number;
  accuracy: number;
  visible_satellites: number;
  visible_satellites_dgps: number;
  mow_point_index: number;
}

export interface ValueDescriptions {
  job: { [jobId: number]: string };
  posSolution: { [jobId: number]: string };
  logLevel: number;
}

export const SensorDescriptions: { [sensorId: number]: string } = {
  0: "none",
  1: "bat undervoltage",
  2: "obstacle",
  3: "gps fix timeout",
  4: "imu timeout",
  5: "imu tilt",
  6: "kidnapped",
  7: "overload",
  8: "motor error",
  9: "gps invalid",
  10: "odometry error",
  11: "no route",
  12: "mem overflow",
  13: "bumper",
  14: "sonar",
  15: "lift",
  16: "rain",
  17: "stop button",
  18: "temp out of range",
};

export interface Stats {
  idle: number;
  charge: number;
  mow: number;
  mow_invalid: number;
  mow_float: number;
  mow_fix: number;
  mow_traveled: number;
  gps_chk_sum_errors: number;
  dgps_chk_sum_errors: number;
  invalid_recoveries: number;
  float_recoveries: number;
  imu_triggered: number;
  gps_motion_timeout: number;
  sonar_triggered: number;
  bumper_triggered: number;
  obstacles: number;
  gps_jumps: number;
  max_cycle: number;
  max_dpgs_age: number;
  serial_buffer_size: number;
  free_memory: number;
  reset_cause: number;
  temp_min: number;
  temp_max: number;
}

export interface State {
  timestamp: number;
  battery_voltage: number;
  position: Position;
  target: Point;
  job: number;
  sensor: number;
  amps: number;
  map_crc: number;
}

export interface SensorSummary {
  timestamp: number;
  sonar_left: number;
  sonar_center: number;
  sonar_right: number;
  sonar_obstacle: boolean;
  sonar_near_obstacle: boolean;
  bumper_left: boolean;
  bumper_right: boolean;
  bumper_obstacle: boolean;
  bumper_near_obstacle: boolean;
  lidar_obstacle: boolean;
  lidar_near_obstacle: boolean;
  lift_triggered: boolean;
  rain_triggered: boolean;
}

export interface GpsSatellite {
  gnssId: number;
  svId: number;
  sigId: number;
  cno: number;
  qualityInd: number;
  prUsed: boolean;
  crCorrUsed: boolean;
  prRes: number;
}

export interface GpsDetails {
  timestamp: number;
  numSV: number;
  numSVdgps: number;
  solution: number;
  hAccuracy: number;
  vAccuracy: number;
  dgpsAge: number;
  satellites: GpsSatellite[];
}

export interface DesiredState {
  speed: number;
  mower_motor_enabled: boolean;
  finish_and_restart: boolean;
  op: number;
  fix_timeout: number;
}

export interface ModemLog {
  log: LogLine[];
}

export enum LogLevel {
  COMM = 32,
  DBG = 16,
  INFO = 8,
  WARN = 4,
  ERR = 2,
  CRIT = 1,
}

export type LogLevelDescT = { [nr: number]: string };

export let LogLevelDesc: LogLevelDescT = {
  32: "COMM",
  16: "DBG",
  8: "INFO",
  4: "WARN",
  2: "ERR",
  1: "CRIT",
};

export interface LogLine {
  nr: number;
  level: LogLevel;
  text: String;
  freeHeap: number;
}

export interface ConsoleLine {
  nr: number;
  isSend: boolean;
  text: string;
}

export interface ConsoleResponseData {
  lines: ConsoleLine[];
}

// Add map as a new response type
export interface UbxResponse {
  timestamp: number;
  hex: string;
}

export enum ResponseDataType {
  hello = 0,
  mowerState,
  mowerStats,
  desiredState,
  modemLog,
  mowerConsole,
  map,
  sensorSummary,
  gpsDetails,
  ubxResponse,
}

// Map data for WebSocket
export interface MapPoint {
  X: number;
  Y: number;
}

export interface MapRaw {
  perimeter: MapPoint[];
  exclusions: MapPoint[][];
  waypoints?: MapPoint[];
  dockpoints?: MapPoint[];
}

export interface ResponseSocketMessage {
  type: ResponseDataType;
  data: State | DesiredState | ModemLog | ConsoleResponseData | MapRaw;
}

export enum RequestDataType {
  hello = 0,
  modemLogSettings,
  mowerConsoleRequest,
  requestGpsDetails,
  stopGpsDetails,
  requestSensorSummary,
  stopSensorSummary,
  requestUbx,
}

export interface ModemLogSettings {
  logLevel: number;
}

export interface ConsoleRequestData {
  cmd: string;
}

export interface RequestSocketMessage {
  type: RequestDataType;
  data: ModemLogSettings | ConsoleRequestData;
}
