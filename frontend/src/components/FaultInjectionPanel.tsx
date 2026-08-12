import { useState } from "react";
import { clearFault, injectFault } from "../api";
import type { FaultTypeName, TargetSensorName } from "../types";

interface Props {
  onFaultTriggered: () => void;
}

interface FaultButton {
  label: string;
  description: string;
  fault_type: FaultTypeName;
  target_sensor: TargetSensorName;
  value: number;
}

const FAULT_BUTTONS: FaultButton[] = [
  {
    label: "Stuck Pedal Sensor",
    description: "Freezes pedal sensor A at 90%, disagreeing with sensor B",
    fault_type: "stuck",
    target_sensor: "pedal_a",
    value: 90,
  },
  {
    label: "Throttle Sensor Mismatch",
    description: "Forces throttle sensor B to disagree with sensor A",
    fault_type: "mismatch",
    target_sensor: "throttle_b",
    value: 0,
  },
  {
    label: "Sensor Out of Range",
    description: "Pushes throttle sensor A outside the valid ADC range",
    fault_type: "out_of_range",
    target_sensor: "throttle_a",
    value: 0,
  },
];

export function FaultInjectionPanel({ onFaultTriggered }: Props) {
  const [pending, setPending] = useState<string | null>(null);
  const [message, setMessage] = useState<string | null>(null);

  async function handleInject(btn: FaultButton) {
    setPending(btn.label);
    setMessage(null);
    try {
      await injectFault({ fault_type: btn.fault_type, target_sensor: btn.target_sensor, value: btn.value });
      setMessage(`Injected: ${btn.label}. Watch the chart and diagnostics panel.`);
      onFaultTriggered();
    } catch (err) {
      setMessage(`Failed to inject fault: ${err}`);
    } finally {
      setPending(null);
    }
  }

  async function handleClearAll() {
    setPending("clear");
    setMessage(null);
    try {
      const targets: TargetSensorName[] = ["pedal_a", "pedal_b", "throttle_a", "throttle_b"];
      await Promise.all(targets.map((t) => clearFault(t)));
      setMessage("Cleared all fault overrides.");
      onFaultTriggered();
    } catch (err) {
      setMessage(`Failed to clear faults: ${err}`);
    } finally {
      setPending(null);
    }
  }

  return (
    <div className="panel">
      <div className="panel-header">
        <h2>Fault Injection</h2>
        <span className="panel-subtitle">Trigger a fault and watch the control loop respond live</span>
      </div>

      <div className="fault-button-grid">
        {FAULT_BUTTONS.map((btn) => (
          <button
            key={btn.label}
            className="fault-button"
            disabled={pending !== null}
            onClick={() => handleInject(btn)}
          >
            <span className="fault-button-label">{btn.label}</span>
            <span className="fault-button-desc">{btn.description}</span>
            {pending === btn.label && <span className="fault-button-pending">sending…</span>}
          </button>
        ))}
      </div>

      <button className="clear-all-button" disabled={pending !== null} onClick={handleClearAll}>
        {pending === "clear" ? "Clearing…" : "Clear All Faults"}
      </button>

      {message && <p className="chart-note fault-message">{message}</p>}
    </div>
  );
}
