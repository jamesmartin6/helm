import { useEffect, useState } from "react";
import { fetchDtcHistory } from "../api";
import { activeDtcCodes, DTC_LABELS, type DtcEvent } from "../types";

interface Props {
  activeDtcMask: number;
  failsafeActive: boolean;
  refreshToken: number; // bump to force a history re-fetch after a fault event
}

function formatTime(iso: string): string {
  return new Date(iso).toLocaleTimeString(undefined, { hour12: false, hour: "2-digit", minute: "2-digit", second: "2-digit" });
}

export function DtcPanel({ activeDtcMask, failsafeActive, refreshToken }: Props) {
  const [history, setHistory] = useState<DtcEvent[]>([]);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    fetchDtcHistory()
      .then((events) => {
        if (!cancelled) setHistory(events);
      })
      .catch((err) => {
        if (!cancelled) setError(String(err));
      });
    return () => {
      cancelled = true;
    };
  }, [refreshToken]);

  const active = activeDtcCodes(activeDtcMask);

  return (
    <div className="panel">
      <div className="panel-header">
        <h2>Diagnostics</h2>
        <span className={`status-pill ${failsafeActive ? "status-pill-critical" : "status-pill-good"}`}>
          {failsafeActive ? "FAIL-SAFE ACTIVE" : "Nominal"}
        </span>
      </div>

      <div className="dtc-active-list">
        {active.length === 0 ? (
          <p className="chart-note">No active DTCs</p>
        ) : (
          active.map((code) => (
            <div key={code} className="dtc-chip">
              <span className="swatch swatch-critical" />
              {DTC_LABELS[code]}
              <span className="dtc-code">0x{code.toString(16).padStart(4, "0")}</span>
            </div>
          ))
        )}
      </div>

      <h3 className="panel-subheading">History</h3>
      {error && <p className="chart-note error-text">{error}</p>}
      <div className="dtc-history">
        {history.length === 0 && !error && <p className="chart-note">No DTC events recorded yet.</p>}
        {history.map((event) => (
          <div key={event.id} className="dtc-history-row">
            <span className={`swatch ${event.cleared_at ? "swatch-good" : "swatch-critical"}`} />
            <div className="dtc-history-text">
              <div className="dtc-history-name">{event.name}</div>
              <div className="dtc-history-times">
                raised {formatTime(event.raised_at)}
                {event.cleared_at ? ` → cleared ${formatTime(event.cleared_at)}` : " — still active"}
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
