import { socketService } from './stores/socket';
import type { Position } from './model';

const DRIVE_SPEED = 0.15;
const MAX_ANGULAR = 0.6;
const ARRIVE_DISTANCE = 0.3;
const ARRIVE_DISTANCE_SLOW = 1.0;
const SEND_INTERVAL = 500;
const KP_TURN = 2.0;
const HEADING_MOVEMENT_THRESHOLD = 0.02;
const HEADING_OFFSET = 0; // 0 or 180 — toggle if robot drives opposite direction

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
  const raw = hasMovementHeading ? movementHeading
    : (_currentPos && _currentPos.delta > 0 ? _currentPos.delta : lastHeading);
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

    if (d < ARRIVE_DISTANCE) {
      stop();
      socketService.sendJoystickMove(0, 0);
      onArriveCb?.();
      return;
    }

    let speed = DRIVE_SPEED;
    if (d < ARRIVE_DISTANCE_SLOW) {
      speed *= (d / ARRIVE_DISTANCE_SLOW);
    }
    if (speed < 0.05) speed = 0.05;

    const currentHeading = getHeading();
    const targetBearing = bearingTo(pos.x, pos.y, targetX, targetY);
    const turnError = angleDiff(targetBearing, currentHeading);

    let angular = (turnError / 180) * KP_TURN * MAX_ANGULAR;
    angular = Math.max(-MAX_ANGULAR, Math.min(MAX_ANGULAR, angular));

    if (Math.abs(turnError) > 90) {
      speed = 0.05;
    } else if (Math.abs(turnError) > 45) {
      speed *= 0.5;
    }

    socketService.sendJoystickMove(
      Math.round(speed * 100) / 100,
      Math.round(angular * 100) / 100,
    );
    lastSendTime = now;
    onUpdateCb?.(d, targetBearing);
  }, SEND_INTERVAL);
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

  if (prevPosX !== null && prevPosY !== null) {
    const dx = pos.x - prevPosX;
    const dy = pos.y - prevPosY;
    if (dist(0, 0, dx, dy) > HEADING_MOVEMENT_THRESHOLD) {
      movementHeading = compassAngle(Math.atan2(dy, dx) * 180 / Math.PI);
      hasMovementHeading = true;
      lastHeading = movementHeading;
    }
  }

  if (pos.delta > 0) {
    lastHeading = pos.delta;
  }

  prevPosX = pos.x;
  prevPosY = pos.y;
}
