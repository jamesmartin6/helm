import {
  CartesianGrid,
  Legend,
  Line,
  LineChart,
  ReferenceArea,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import type { TelemetryPoint } from "../types";

interface Props {
  points: TelemetryPoint[];
}

// Contiguous cycle_id ranges where failsafe was active, so we can shade
// those spans on the chart -- makes the fault-to-failsafe transition
// visually obvious without needing a second axis or a separate plot.
function failsafeBands(points: TelemetryPoint[]): Array<[number, number]> {
  const bands: Array<[number, number]> = [];
  let start: number | null = null;
  for (const p of points) {
    if (p.failsafe_active && start === null) {
      start = p.cycle_id;
    } else if (!p.failsafe_active && start !== null) {
      bands.push([start, p.cycle_id]);
      start = null;
    }
  }
  if (start !== null && points.length > 0) {
    bands.push([start, points[points.length - 1].cycle_id]);
  }
  return bands;
}

function formatTooltipValue(value: unknown, name: unknown): [string, string] {
  const num = typeof value === "number" ? value.toFixed(2) : String(value);
  return [`${num}%`, String(name)];
}

export function TelemetryChart({ points }: Props) {
  const bands = failsafeBands(points);

  return (
    <div className="panel">
      <div className="panel-header">
        <h2>Throttle Position</h2>
        <span className="panel-subtitle">Commanded pedal vs. actual throttle, live</span>
      </div>
      <ResponsiveContainer width="100%" height={320}>
        <LineChart data={points} margin={{ top: 8, right: 16, left: 0, bottom: 0 }}>
          <CartesianGrid strokeDasharray="3 3" stroke="var(--border)" vertical={false} />
          <XAxis
            dataKey="cycle_id"
            tick={{ fill: "var(--text-muted)", fontSize: 12 }}
            stroke="var(--border)"
            tickFormatter={(v: number) => `${v}`}
            label={{ value: "control cycle", position: "insideBottom", offset: -2, fill: "var(--text-muted)", fontSize: 12 }}
          />
          <YAxis
            domain={[-20, 100]}
            tick={{ fill: "var(--text-muted)", fontSize: 12 }}
            stroke="var(--border)"
            label={{ value: "%", angle: -90, position: "insideLeft", fill: "var(--text-muted)", fontSize: 12 }}
          />
          <Tooltip
            formatter={formatTooltipValue}
            labelFormatter={(v) => `cycle ${v}`}
            contentStyle={{
              background: "var(--surface-2)",
              border: "1px solid var(--border)",
              borderRadius: 6,
              color: "var(--text-primary)",
              fontSize: 13,
            }}
          />
          <Legend wrapperStyle={{ fontSize: 13, color: "var(--text-secondary)" }} />
          {bands.map(([from, to], i) => (
            <ReferenceArea
              key={i}
              x1={from}
              x2={to}
              fill="var(--failsafe-band)"
              stroke="none"
              ifOverflow="extendDomain"
            />
          ))}
          <Line
            type="monotone"
            dataKey="pedal_pct"
            name="Pedal (commanded)"
            stroke="var(--series-pedal)"
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
          />
          <Line
            type="monotone"
            dataKey="throttle_pct"
            name="Throttle (actual)"
            stroke="var(--series-throttle)"
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
          />
          <Line
            type="monotone"
            dataKey="control_error"
            name="Control error"
            stroke="var(--series-error)"
            strokeWidth={1.5}
            strokeDasharray="4 3"
            dot={false}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
      {bands.length > 0 && (
        <p className="chart-note">
          <span className="swatch swatch-failsafe" /> shaded region = fail-safe active
        </p>
      )}
    </div>
  );
}
