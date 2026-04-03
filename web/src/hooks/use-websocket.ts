import { useEffect, useRef, useCallback } from 'preact/hooks';
import { connected, updateMetrics, type MetricSample } from '../lib/telemetry-store';

const API_BASE = '/api/v1';

export function useWebSocket() {
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<number | null>(null);

  const connect = useCallback(() => {
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    const ws = new WebSocket(`${protocol}//${location.host}/ws`);

    ws.onopen = () => {
      connected.value = true;
      // Request initial snapshot
      ws.send(JSON.stringify({ type: 'subscribe' }));
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'snapshot' || msg.type === 'delta') {
          updateMetrics(msg.metrics as MetricSample[]);
        }
      } catch {
        // Ignore malformed messages
      }
    };

    ws.onclose = () => {
      connected.value = false;
      wsRef.current = null;
      // Reconnect after delay
      reconnectTimer.current = window.setTimeout(connect, 2000);
    };

    ws.onerror = () => {
      ws.close();
    };

    wsRef.current = ws;
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
      wsRef.current?.close();
    };
  }, [connect]);
}

// Fallback polling for when WebSocket is not yet available
export function usePolling(intervalMs = 1000) {
  useEffect(() => {
    let active = true;

    const poll = async () => {
      try {
        const res = await fetch(`${API_BASE}/live/snapshot`);
        if (res.ok) {
          const data = await res.json();
          updateMetrics(data.metrics);
          connected.value = true;
        }
      } catch {
        connected.value = false;
      }
    };

    const loop = async () => {
      while (active) {
        await poll();
        await new Promise(r => setTimeout(r, intervalMs));
      }
    };

    loop();
    return () => { active = false; };
  }, [intervalMs]);
}
