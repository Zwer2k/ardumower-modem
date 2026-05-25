import { socketService } from './stores/socket';
import type { Position } from './model';

const DRIVE_SPEED = 0.3;
const MAX_ANGULAR = 0.5;
const ARRIVE_DISTANCE = 0.3;
const ARRIVE_DISTANCE_SLOW = 1.0;
const SEND_INTERVAL = 200;
const KP_TURN = 2.0;

let targetX: number | null = null;
let targetY: number | null = null;
let isDriving = false;
let intervalId: number | null = null;
let lastSendTime = 0;
let onArriveCb: (() => void) | null = null;
let onUpdateCb: ((dist: number, bearing: number) => void) | null = null;

let _currentPos: Position | null = null;

function dist(x1: number, y1: number, x2: number, y2: number): number {
  const dx = x2 - x1;
  const dy = y2 - y1;
  return Math.sqrt(dx * dx + dy * dy);
}

function angleTo(fromX: number, fromY: number, toX: number, toY: number): number {
  return Math.atan2(toY - fromY, toX - fromX) * 180 / Math.PI;
}

function angleDiff(a: number, b: number): number {
  let d = (a - b + 360) % 360;
  if (d > 180) d -= 360;
  return d;
}

function loop() {
  intervalId = window.setInterval(() => {
    const now = Date.now();
    if (now - lastSendTime < SEND_INTERVAL) return;

    const pos = _currentPos;
    if (!pos) return;

    if (targetX === null || targetY === null || !isDriving) return;

    const d = dist(pos.x, pos.y, targetX, targetY);
    const targetAngle = angleTo(pos.x, pos.y, targetX, targetY);

    if (d < ARRIVE_DISTANCE) {
      stop();
      socketService.sendJoystickMove(0, 0);
      onArriveCb?.();
      return;
    }

    let speed = DRIVE_SPEED;
    if (d < ARRIVE_DISTANCE_SLOW) {
      speed *= (d / ARRIVE_DISTANCE_SLOW);
      if (speed < 0.05) speed = 0.05;
    }

    let currentHeading = pos.delta;
    if (!currentHeading || currentHeading === 0) {
      currentHeading = targetAngle;
    }

    const turnError = angleDiff(targetAngle, currentHeading);
    let angular = -(turnError / 180) * KP_TURN * MAX_ANGULAR;
    angular = Math.max(-MAX_ANGULAR, Math.min(MAX_ANGULAR, angular));

    if (Math.abs(turnError) > 90) {
      speed = 0;
    } else if (Math.abs(turnError) > 45) {
      speed *= 0.5;
    }

    socketService.sendJoystickMove(
      Math.round(speed * 100) / 100,
      Math.round(angular * 100) / 100,
    );
    lastSendTime = now;
    onUpdateCb?.(d, targetAngle);
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

export function onArrive(cb: () => void) {
  onArriveCb = cb;
}

export function onUpdate(cb: (dist: number, bearing: number) => void) {
  onUpdateCb = cb;
}

export function setPosition(pos: Position | null) {
  _currentPos = pos;
}
