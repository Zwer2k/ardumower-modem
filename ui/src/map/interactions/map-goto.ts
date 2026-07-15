import { writable } from "svelte/store";
import type { SocketState } from "../../stores/socket";
import { socketService } from "../../stores/socket";

export interface GotoState {
  targetSet: boolean;
  targetPos: { x: number; y: number } | null;
  isDriving: boolean;
  targetDist: number;
  targetBearing: number;
}

export function createGotoState() {
  return writable<GotoState>({
    targetSet: false,
    targetPos: null,
    isDriving: false,
    targetDist: 0,
    targetBearing: 0,
  });
}

export function updateGotoFromMowerPosition(
  state: GotoState,
  mowerPos: { x: number; y: number } | null
): GotoState {
  if (!mowerPos || !state.targetPos) return state;
  const mx = mowerPos.x;
  const my = -mowerPos.y;
  const tx = state.targetPos.x;
  const ty = state.targetPos.y;
  const dx = tx - mx;
  const dy = ty - my;
  return {
    ...state,
    targetDist: Math.sqrt(dx * dx + dy * dy),
    targetBearing: (Math.atan2(dx, -dy) * 180) / Math.PI,
  };
}

export function setGotoTarget(x: number, y: number): Partial<GotoState> {
  return {
    targetSet: true,
    targetPos: { x, y },
    isDriving: false,
    targetDist: 0,
    targetBearing: 0,
  };
}

export function startDrive(targetPos: { x: number; y: number } | null): Partial<GotoState> {
  if (!targetPos) return {};
  socketService.sendNavigateTo(targetPos.x, -targetPos.y);
  return { isDriving: true };
}

export function stopDrive(): Partial<GotoState> {
  socketService.sendJoystickMove(0, 0);
  return { isDriving: false };
}

export function clearTarget(): Partial<GotoState> {
  socketService.sendJoystickMove(0, 0);
  return {
    targetSet: false,
    targetPos: null,
    isDriving: false,
    targetDist: 0,
    targetBearing: 0,
  };
}

export function canShowGoto(
  $socketStore: SocketState,
  edit: boolean,
  showManage: boolean,
  showCalculate: boolean,
  needsUpload: boolean
): boolean {
  return (
    !edit &&
    !showManage &&
    !showCalculate &&
    !needsUpload &&
    $socketStore.state !== null
  );
}

export function hasMap($MapStore: { map?: { perimeter?: { points?: unknown[] } } }): boolean {
  return ($MapStore?.map?.perimeter?.points?.length ?? 0) > 0;
}
