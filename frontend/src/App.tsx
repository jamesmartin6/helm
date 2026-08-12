import { useState } from "react";
import "./App.css";
import { DtcPanel } from "./components/DtcPanel";
import { FaultInjectionPanel } from "./components/FaultInjectionPanel";
import { TelemetryChart } from "./components/TelemetryChart";
import { useTelemetrySocket } from "./hooks/useTelemetrySocket";

function StatTile({ label, value, unit }: { label: string; value: string; unit?: string }) {
  return (
    <div className="stat-tile">
      <div className="stat-label">{label}</div>
      <div className="stat-value">
        {value}
        {unit && <span className="stat-unit">{unit}</span>}
      </div>
    </div>
  );
}

function App() {
  const { status, points, latest, activeDtcMask } = useTelemetrySocket();
  const [dtcRefreshToken, setDtcRefreshToken] = useState(0);

  return (
    <div className="app-root">
      <header className="app-header">
        <div>
          <h1>Helm</h1>
          <p className="app-subtitle">Electronic throttle control — live bench view</p>
        </div>
        <div className={`connection-pill connection-${status}`}>
          <span className="connection-dot" />
          {status === "open" ? "Connected" : status === "connecting" ? "Connecting…" : "Disconnected"}
        </div>
      </header>

      <div className="stat-row">
        <StatTile label="Pedal" value={latest ? latest.pedal_pct.toFixed(1) : "—"} unit="%" />
        <StatTile label="Throttle" value={latest ? latest.throttle_pct.toFixed(1) : "—"} unit="%" />
        <StatTile label="Control Error" value={latest ? latest.control_error.toFixed(2) : "—"} unit="%" />
        <StatTile label="Actuator Duty" value={latest ? latest.actuator_duty_pct.toFixed(1) : "—"} unit="%" />
        <StatTile label="Cycle" value={latest ? String(latest.cycle_id) : "—"} />
      </div>

      <main className="app-grid">
        <TelemetryChart points={points} />
        <DtcPanel
          activeDtcMask={activeDtcMask}
          failsafeActive={latest?.failsafe_active ?? false}
          refreshToken={dtcRefreshToken}
        />
        <FaultInjectionPanel onFaultTriggered={() => setDtcRefreshToken((n) => n + 1)} />
      </main>
    </div>
  );
}

export default App;
