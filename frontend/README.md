# Helm Frontend

Live telemetry dashboard: a real-time chart of commanded pedal vs. actual
throttle position and control error, a diagnostics panel showing active +
historical DTCs, and a fault-injection panel for triggering faults straight
from the UI and watching the control loop respond.

## Setup

```
npm install
cp .env.example .env.local   # points at the backend; adjust if not localhost:8000
npm run dev
```

Needs the backend running (`../backend`), which in turn needs the firmware
running under QEMU (`../firmware`) for live data -- see the root
`progress.md` for the full stack startup sequence.

## Build

```
npm run build
```

## Notes

- `useTelemetrySocket` auto-reconnects the WebSocket on drop (1.5s backoff)
  and caps in-memory history at 600 points so the chart doesn't grow
  unbounded on a long-running session.
- The chart shades the background wherever `failsafe_active` was true in
  the visible window, so a fault-to-failsafe transition is visible at a
  glance without a second y-axis.
- Colors follow a single-axis, fixed-order categorical palette (never a
  dual-axis chart, never color assigned by data rank) -- see the `dataviz`
  design notes in this project's development history if extending the
  chart with more series.
