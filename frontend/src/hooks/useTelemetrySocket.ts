import { useEffect, useRef, useState } from "react";
import type { ConnectionStatus, TelemetryPoint, WsMessage } from "../types";

const MAX_POINTS = 600; // 6s of history at 100Hz, or more at a slower observed rate

function wsUrl(): string {
  const base = import.meta.env.VITE_API_BASE_URL ?? `${window.location.protocol}//${window.location.host}`;
  const wsProto = base.startsWith("https") ? "wss" : "ws";
  const host = base.replace(/^https?:\/\//, "");
  return `${wsProto}://${host}/ws/telemetry`;
}

export interface TelemetrySocketState {
  status: ConnectionStatus;
  points: TelemetryPoint[];
  latest: TelemetryPoint | null;
  activeDtcMask: number;
}

export function useTelemetrySocket(): TelemetrySocketState {
  const [status, setStatus] = useState<ConnectionStatus>("connecting");
  const [points, setPoints] = useState<TelemetryPoint[]>([]);
  const [activeDtcMask, setActiveDtcMask] = useState(0);
  const reconnectTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    let cancelled = false;
    let socket: WebSocket | null = null;

    function connect() {
      if (cancelled) return;
      setStatus("connecting");
      socket = new WebSocket(wsUrl());

      socket.onopen = () => {
        if (!cancelled) setStatus("open");
      };

      socket.onmessage = (event) => {
        const msg: WsMessage = JSON.parse(event.data);
        if (msg.type === "telemetry") {
          const { type: _type, ...point } = msg;
          setPoints((prev) => {
            const next = [...prev, point as TelemetryPoint];
            return next.length > MAX_POINTS ? next.slice(next.length - MAX_POINTS) : next;
          });
          setActiveDtcMask(point.active_dtc_mask);
        } else if (msg.type === "dtc_change") {
          setActiveDtcMask(msg.active_dtc_mask);
        }
      };

      socket.onclose = () => {
        if (cancelled) return;
        setStatus("closed");
        reconnectTimer.current = setTimeout(connect, 1500);
      };

      socket.onerror = () => {
        socket?.close();
      };
    }

    connect();

    return () => {
      cancelled = true;
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
      socket?.close();
    };
  }, []);

  return {
    status,
    points,
    latest: points.length > 0 ? points[points.length - 1] : null,
    activeDtcMask,
  };
}
