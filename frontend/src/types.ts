// Mirrors backend/app/schemas.py and the WS message shapes serial_ingest.py
// broadcasts. Keep in sync if either changes.

export interface TelemetryPoint {
  cycle_id: number;
  timestamp_ms: number;
  pedal_pct: number;
  throttle_pct: number;
  control_error: number;
  actuator_duty_pct: number;
  active_dtc_mask: number;
  failsafe_active: boolean;
  recorded_at: string;
}

export interface DtcEvent {
  id: number;
  code: number;
  name: string;
  raised_at: string;
  cleared_at: string | null;
  cause: string | null;
}

// DTC bitmask values, mirrored from firmware/src/protocol.h.
export const DTC_PEDAL_PLAUSIBILITY = 0x0001;
export const DTC_THROTTLE_PLAUSIBILITY = 0x0002;
export const DTC_SENSOR_OUT_OF_RANGE = 0x0004;
export const DTC_COMMS_TIMEOUT = 0x0008;
export const DTC_WATCHDOG_TRIP = 0x0010;
export const DTC_ACTUATOR_FAULT = 0x0020;

export const DTC_LABELS: Record<number, string> = {
  [DTC_PEDAL_PLAUSIBILITY]: "Pedal Plausibility",
  [DTC_THROTTLE_PLAUSIBILITY]: "Throttle Plausibility",
  [DTC_SENSOR_OUT_OF_RANGE]: "Sensor Out of Range",
  [DTC_COMMS_TIMEOUT]: "Comms Timeout",
  [DTC_WATCHDOG_TRIP]: "Watchdog Trip",
  [DTC_ACTUATOR_FAULT]: "Actuator Fault",
};

export function activeDtcCodes(mask: number): number[] {
  return Object.keys(DTC_LABELS)
    .map(Number)
    .filter((code) => (mask & code) !== 0);
}

export type FaultTypeName = "stuck" | "out_of_range" | "mismatch" | "comms_dropout";
export type TargetSensorName = "pedal_a" | "pedal_b" | "throttle_a" | "throttle_b";

export interface FaultInjectRequest {
  fault_type: FaultTypeName;
  target_sensor: TargetSensorName;
  value: number;
}

export type WsMessage =
  | ({ type: "telemetry" } & TelemetryPoint)
  | { type: "dtc_change"; active_dtc_mask: number };

export type ConnectionStatus = "connecting" | "open" | "closed";
