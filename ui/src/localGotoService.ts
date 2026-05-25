import { socketService } from './stores/socket';
import type { Position } from './model';

const DRIVE_SPEED = 0.15;
const MAX_ANGULAR = 0.6;
const ARRIVE_DISTANCE = 0.5;          // stop when within 0.5m
const ARRIVE_DISTANCE_SLOW = 1.5;      // start slowing at 1.5m
const SEND_INTERVAL = 500;
const KP_TURN = 2.0;
const HEADING_MOVEMENT_THRESHOLD = 0.02;
const HEADING_OFFSET = 0;
const ARRIVED_CONSECUTIVE = 3;         // require 3 consecutive samples < threshold

let targetX: number | null = null;
let targetY: number | null = null;
let isDriving = false;
let intervalId: number | null = null;
let lastSendTime = 0;
let onArriveCb: (() => void) | null = null;
let onUpdateCb: ((dist: number, bearing: number) => void) | null = null;

let _currentPos: Position | null = null;
let prevPosX: number | null = null;
let prevPosY: number | null = null;
let movementHeading = 0;
let hasMovementHeading = false;
let lastHeading = 0;
let arrivedCount = 0;
let prevDistance: number | null = null;
let lastPosTime = 0;

function dist(x1: number, y1: number, x2: number, y2: number): number {
  const dx = x2 - x1;
  const dy = y2 - y1;
  return Math.sqrt(dx * dx + dy * dy);
}

/** compass bearing from (fromX,fromY) to (toX,toY) in degrees, 0°=North CW */
function bearingTo(fromX: number, fromY: number, toX: number, toY: number): number {
  const atan2Deg = Math.atan2(toY - fromY, toX - fromX) * 180 / Math.PI;
  return ((90 - atan2Deg) % 360 + 360) % 360;
}

function angleDiff(a: number, b: number): number {
  let d = (a - b + 360) % 360;
  if (d > 180) d -= 360;
  return d;
}

function compassAngle(atan2Deg: number): number {
  return ((90 - atan2Deg) % 360 + 360) % 360;
}

function getHeading(): number {
  const raw = hasMovementHeading ? movementHeading : lastHeading;
  return (raw + HEADING_OFFSET) % 360;
}

function loop() {
  intervalId = window.setInterval(() => {
    const now = Date.now();
    if (now - lastSendTime < SEND_INTERVAL) return;

    const pos = _currentPos;
    if (!pos) return;

    if (targetX === null || targetY === null || !isDriving) return;

    const d = dist(pos.x, pos.y, targetX, targetY);

    // Arrival with debounce: require N consecutive samples below threshold
    if (d < ARRIVE_DISTANCE) {
      arrivedCount++;
      if (arrivedCount >= ARRIVED_CONSECUTIVE) {
        stop();
        socketService.sendJoystickMove(0, 0);
        onArriveCb?.();
        console.log('[goto] arrived');
        return;
      }
    } else {
      arrivedCount = 0;
    }

    const currentHeading = getHeading();
    const targetBearing = bearingTo(pos.x, pos.y, targetX, targetY);
    const turnError = angleDiff(targetBearing, currentHeading);
    const absErr = Math.abs(turnError);

    // Speed control
    let speed = DRIVE_SPEED;
    // Slow down when position data is stale (>3s)
    const posAge = (now - lastPosTime) / 1000;
    if (posAge > 3) {
      speed = 0.05;
    }
    // Slow down near target
    if (d < ARRIVE_DISTANCE_SLOW) {
      speed *= (d / ARRIVE_DISTANCE_SLOW);
    }
    // Slow down when turning sharply
    if (absErr > 90) {
      speed = 0.05;
    } else if (absErr > 45) {
      speed *= 0.5;
    }
    // Anti-overshoot: if we're moving away from the target
    if (prevDistance !== null && d > prevDistance + 0.05) {
      speed = 0.05;
    }
    if (speed < 0.05) speed = 0.05;

    // Angular control with dead zone
    let angular = 0;
    if (absErr > 5) {
      // turnError > 0 = target is to the RIGHT of heading.
      // The mower interprets positive angular as turn LEFT,
      // so we negate to get the correct direction.
      angular = -(turnError / 180) * KP_TURN * MAX_ANGULAR;
      angular = Math.max(-MAX_ANGULAR, Math.min(MAX_ANGULAR, angular));
    }

    console.log(
      '[goto]',
      'd:', d.toFixed(2),
      'hding:', currentHeading.toFixed(0),
      'brng:', targetBearing.toFixed(0),
      'err:', turnError.toFixed(0),
      'spd:', speed.toFixed(2),
      'ang:', angular.toFixed(2),
      'age:', posAge.toFixed(1),
    );

    socketService.sendJoystickMove(
      Math.round(speed * 100) / 100,
      Math.round(angular * 100) / 100,
    );
    prevDistance = d;
    lastSendTime = now;
    onUpdateCb?.(d, targetBearing);
  }, SEND_INTERVAL);
}

export function debugState() {
  console.log(
    '[goto] state',
    'targetX:', targetX,
    'targetY:', targetY,
    'isDriving:', isDriving,
    '_currentPos:', _currentPos,
    'movementHeading:', movementHeading,
    'hasMovementHeading:', hasMovementHeading,
    'lastHeading:', lastHeading,
    'HEADING_OFFSET:', HEADING_OFFSET,
  );
}

export function setTarget(x: number, y: number) {
  targetX = x;
  targetY = y;
}

export function getTarget(): { x: number; y: number } | null {
  if (targetX === null || targetY === null) return null;
  return { x: targetX, y: targetY };
}

export function clearTarget() {
  targetX = null;
  targetY = null;
  movementHeading = 0;
  hasMovementHeading = false;
  prevPosX = null;
  prevPosY = null;
  lastHeading = 0;
  arrivedCount = 0;
  prevDistance = null;
}

export function start(): boolean {
  if (targetX === null || targetY === null) return false;
  isDriving = true;
  lastSendTime = 0;
  if (intervalId === null) loop();
  return true;
}

export function stop() {
  isDriving = false;
  if (intervalId !== null) {
    clearInterval(intervalId);
    intervalId = null;
  }
  socketService.sendJoystickMove(0, 0);
  arrivedCount = 0;
  prevDistance = null;
}

export function isActive(): boolean {
  return isDriving;
}

export function getComputedHeading(): number {
  return getHeading();
}

export function hasValidHeading(): boolean {
  return hasMovementHeading || (_currentPos !== null && _currentPos.delta > 0);
}

export function onArrive(cb: () => void) {
  onArriveCb = cb;
}

export function onUpdate(cb: (dist: number, bearing: number) => void) {
  onUpdateCb = cb;
}

export function setPosition(pos: Position | null) {
  if (!pos) return;
  _currentPos = pos;
  lastPosTime = Date.now();

  if (prevPosX !== null && prevPosY !== null) {
    const dx = pos.x - prevPosX;
    const dy = pos.y - prevPosY;
    if (dist(0, 0, dx, dy) > HEADING_MOVEMENT_THRESHOLD) {
      const rawDeg = Math.atan2(dy, dx) * 180 / Math.PI;
      movementHeading = compassAngle(rawDeg);
      hasMovementHeading = true;
      lastHeading = movementHeading;
      console.log('[goto] heading', movementHeading.toFixed(1), 'dx:', dx.toFixed(3), 'dy:', dy.toFixed(3));
    }
  }

  // pos.delta is unreliable as heading (confirmed by user), ignore it.

  prevPosX = pos.x;
  prevPosY = pos.y;
}
