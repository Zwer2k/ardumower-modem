import { socketService } from './stores/socket';

// ─── Constants ──────────────────────────────────────────────────────────
const DRIVE_SPEED = 0.3;           // m/s forward speed
const MAX_ANGULAR = 0.5;           // rad/s max turn rate
const ARRIVE_DISTANCE = 1.5;       // metres — stop when this close
const ARRIVE_DISTANCE_SLOW = 5.0;  // metres — start slowing down
const SEND_INTERVAL = 200;         // ms — same as joystick / PS4
const KP_TURN = 2.0;               // proportional gain for heading correction

// ─── State ──────────────────────────────────────────────────────────────
let targetLat: number | null = null;
let targetLon: number | null = null;
let isDriving = false;
let rafId: number | null = null;
let lastSendTime = 0;
let onArriveCb: (() => void) | null = null;
let onUpdateCb: ((dist: number, bearing: number) => void) | null = null;

// GPS state injected by subscriber
let _currentGpsState: { navPvt: { lat: number; lon: number; heading: number; fixOk: boolean } | null } | null = null;

// ─── Helpers ────────────────────────────────────────────────────────────
function toRad(deg: number): number { return deg * Math.PI / 180; }
function toDeg(rad: number): number { return rad * 180 / Math.PI; }

/** Haversine distance in metres */
function haversine(lat1: number, lon1: number, lat2: number, lon2: number): number {
    const R = 6371000;
    const dLat = toRad(lat2 - lat1);
    const dLon = toRad(lon2 - lon1);
    const a = Math.sin(dLat / 2) ** 2 +
              Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.sin(dLon / 2) ** 2;
    return 2 * R * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

/** Initial bearing from (lat1,lon1) to (lat2,lon2) in degrees */
function bearing(lat1: number, lon1: number, lat2: number, lon2: number): number {
    const y = Math.sin(toRad(lon2 - lon1)) * Math.cos(toRad(lat2));
    const x = Math.cos(toRad(lat1)) * Math.sin(toRad(lat2)) -
              Math.sin(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.cos(toRad(lon2 - lon1));
    let brng = toDeg(Math.atan2(y, x));
    return (brng + 360) % 360;
}

/** Normalize angle difference to [-180, 180] */
function angleDiff(a: number, b: number): number {
    let d = (a - b + 360) % 360;
    if (d > 180) d -= 360;
    return d;
}

// ─── Main loop ──────────────────────────────────────────────────────────
function loop() {
    rafId = requestAnimationFrame(loop);

    const now = Date.now();
    if (now - lastSendTime < SEND_INTERVAL) return;

    const state = _currentGpsState;
    if (!state || !state.navPvt || !state.navPvt.fixOk) return;
    const pvt = state.navPvt;

    if (targetLat === null || targetLon === null || !isDriving) return;

    const dist = haversine(pvt.lat, pvt.lon, targetLat, targetLon);
    const targetBearing = bearing(pvt.lat, pvt.lon, targetLat, targetLon);

    // Arrived?
    if (dist < ARRIVE_DISTANCE) {
        stop();
        socketService.sendJoystickMove(0, 0);
        onArriveCb?.();
        return;
    }

    // Slow down when approaching
    let speed = DRIVE_SPEED;
    if (dist < ARRIVE_DISTANCE_SLOW) {
        speed *= (dist / ARRIVE_DISTANCE_SLOW);
        if (speed < 0.05) speed = 0.05;
    }

    // Heading correction
    let currentHeading = pvt.heading;
    if (!currentHeading || currentHeading === 0) {
        currentHeading = targetBearing;
    }

    const turnError = angleDiff(targetBearing, currentHeading);
    let angular = -(turnError / 180) * KP_TURN * MAX_ANGULAR;
    angular = Math.max(-MAX_ANGULAR, Math.min(MAX_ANGULAR, angular));

    // Slow/stop forward motion for large turns
    if (Math.abs(turnError) > 90) {
        speed = 0;
    } else if (Math.abs(turnError) > 45) {
        speed *= 0.5;
    }

    const lin = Math.round(speed * 100) / 100;
    const ang = Math.round(angular * 100) / 100;

    socketService.sendJoystickMove(lin, ang);
    lastSendTime = now;
    onUpdateCb?.(dist, targetBearing);
}

// ─── Public API ─────────────────────────────────────────────────────────

export function setTarget(lat: number, lon: number) {
    targetLat = lat;
    targetLon = lon;
}

export function getTarget(): { lat: number; lon: number } | null {
    if (targetLat === null || targetLon === null) return null;
    return { lat: targetLat, lon: targetLon };
}

export function clearTarget() {
    targetLat = null;
    targetLon = null;
}

export function start(): boolean {
    if (targetLat === null || targetLon === null) return false;
    isDriving = true;
    lastSendTime = 0;
    if (!rafId) loop();
    return true;
}

export function stop() {
    isDriving = false;
    if (rafId) {
        cancelAnimationFrame(rafId);
        rafId = null;
    }
    socketService.sendJoystickMove(0, 0);
}

export function isActive(): boolean {
    return isDriving;
}

export function onArrive(cb: () => void) {
    onArriveCb = cb;
}

export function onUpdate(cb: (dist: number, bearing: number) => void) {
    onUpdateCb = cb;
}

export function setGpsState(state: typeof _currentGpsState) {
    _currentGpsState = state;
}
