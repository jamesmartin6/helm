import type { DtcEvent, FaultInjectRequest, TargetSensorName } from "./types";

const API_BASE = import.meta.env.VITE_API_BASE_URL ?? "";

export async function fetchDtcHistory(): Promise<DtcEvent[]> {
  const resp = await fetch(`${API_BASE}/dtcs`);
  if (!resp.ok) throw new Error(`GET /dtcs failed: ${resp.status}`);
  return resp.json();
}

export async function injectFault(req: FaultInjectRequest): Promise<void> {
  const resp = await fetch(`${API_BASE}/faults/inject`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(req),
  });
  if (!resp.ok) throw new Error(`POST /faults/inject failed: ${resp.status}`);
}

export async function clearFault(target_sensor: TargetSensorName): Promise<void> {
  const resp = await fetch(`${API_BASE}/faults/clear`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ target_sensor }),
  });
  if (!resp.ok) throw new Error(`POST /faults/clear failed: ${resp.status}`);
}
