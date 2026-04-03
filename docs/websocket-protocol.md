# WebSocket & Real-Time Protocol

PulsePort uses a combination of WebSocket and REST polling to deliver real-time metric updates to the web dashboard.

---

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                  Frontend (Browser)                  │
│                                                      │
│  ┌─────────────────┐     ┌────────────────────────┐  │
│  │  useWebSocket() │     │    usePolling()         │  │
│  │  (primary)      │     │    (fallback, 1s)       │  │
│  │                 │     │                          │  │
│  │  ws://host/ws   │     │  GET /api/v1/live/snap  │  │
│  └────────┬────────┘     └────────────┬─────────────┘  │
│           │                           │              │
│           └─────────┬─────────────────┘              │
│                     ▼                                │
│           ┌─────────────────┐                        │
│           │ telemetry-store │                        │
│           │ (Preact Signals)│                        │
│           └────────┬────────┘                        │
│                    │                                 │
│              UI Components                           │
│         (metric tiles, charts)                       │
└──────────────────────────────────────────────────────┘
                      │
              HTTP/WebSocket
                      │
┌──────────────────────────────────────────────────────┐
│                PulsePort Service                     │
│                                                      │
│  ┌──────────────────┐    ┌───────────────────┐       │
│  │   HTTP Server     │    │  MetricRegistry   │       │
│  │   (cpp-httplib)   │◄───│  (live values)    │       │
│  │                   │    └───────────────────┘       │
│  │  GET /api/v1/     │                               │
│  │    live/snapshot  │                               │
│  │                   │                               │
│  │  WS /ws (planned)│                               │
│  └──────────────────┘                                │
└──────────────────────────────────────────────────────┘
```

---

## Current Implementation: REST Polling

WebSocket broadcast is planned but not yet implemented in the C++ backend. The frontend uses a polling fallback that provides similar real-time behavior.

### Polling Endpoint

```
GET /api/v1/live/snapshot
```

Returns a full metric snapshot (all registered metrics and their current values).

### Polling Behavior

- **Interval:** 1 second (configurable via `usePolling(intervalMs)`)
- **Method:** Sequential poll loop — waits for the previous request to complete before starting the next
- **Connection status:** Sets `connected` signal to `true` on success, `false` on fetch error
- **Data handling:** Each response completely replaces the metric store (no delta merging needed)

### Frontend Hook: `usePolling()`

```typescript
import { usePolling } from '../hooks/use-websocket';

// In your component:
usePolling(1000);  // Poll every 1 second
```

**Implementation:**
```typescript
export function usePolling(intervalMs = 1000) {
  useEffect(() => {
    let active = true;

    const poll = async () => {
      try {
        const res = await fetch('/api/v1/live/snapshot');
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
```

---

## Planned Implementation: WebSocket

### Connection

```
ws://127.0.0.1:9770/ws
```

### Protocol

JSON messages over WebSocket text frames.

#### Client → Server Messages

**Subscribe:**
```json
{
  "type": "subscribe"
}
```

Requests an initial full snapshot followed by periodic delta updates.

**Unsubscribe:**
```json
{
  "type": "unsubscribe"
}
```

Stops receiving delta updates.

#### Server → Client Messages

**Snapshot** (sent on subscribe):
```json
{
  "type": "snapshot",
  "tsUtc": 1743638400,
  "metrics": [
    {
      "key": "cpu.total_pct",
      "value": 23.5,
      "unit": "%",
      "quality": "measured",
      "ts": 1743638400
    }
  ]
}
```

Contains all currently registered metrics and their latest values.

**Delta** (sent every sample tick):
```json
{
  "type": "delta",
  "tsUtc": 1743638401,
  "metrics": [
    {
      "key": "cpu.total_pct",
      "value": 25.1,
      "unit": "%",
      "quality": "measured",
      "ts": 1743638401
    },
    {
      "key": "mem.used_pct",
      "value": 67.5,
      "unit": "%",
      "quality": "measured",
      "ts": 1743638401
    }
  ]
}
```

Contains only metrics that changed since the last update. The frontend merges these with the existing store.

### Frontend Hook: `useWebSocket()`

```typescript
import { useWebSocket } from '../hooks/use-websocket';

// In your component (currently tries WS, falls back to polling internally):
useWebSocket();
```

**Implementation:**
```typescript
export function useWebSocket() {
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<number | null>(null);

  const connect = useCallback(() => {
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    const ws = new WebSocket(`${protocol}//${location.host}/ws`);

    ws.onopen = () => {
      connected.value = true;
      ws.send(JSON.stringify({ type: 'subscribe' }));
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'snapshot' || msg.type === 'delta') {
          updateMetrics(msg.metrics as MetricSample[]);
        }
      } catch { /* Ignore malformed messages */ }
    };

    ws.onclose = () => {
      connected.value = false;
      wsRef.current = null;
      reconnectTimer.current = window.setTimeout(connect, 2000);
    };

    ws.onerror = () => { ws.close(); };
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
```

### Reconnection Strategy

| Event | Behavior |
|-------|----------|
| Connection opened | Set `connected = true`, send `subscribe` |
| Message received | Parse JSON, update metric store |
| Connection closed | Set `connected = false`, reconnect after 2 seconds |
| Connection error | Close the socket (triggers `onclose` → reconnect) |
| Component unmounted | Clear reconnect timer, close socket |

The reconnection is a simple fixed-delay retry (2 seconds). No exponential backoff is implemented.

---

## Telemetry Store

The `telemetry-store.ts` module manages the client-side metric state using Preact Signals.

### Signals

| Signal | Type | Description |
|--------|------|-------------|
| `connected` | `Signal<boolean>` | Whether the data channel (WS or polling) is active |
| `metrics` | `Signal<Map<string, MetricSample>>` | Current metric values, keyed by metric key |
| `lastUpdate` | `Signal<number>` | Unix timestamp of the last received update |

### MetricSample Type

```typescript
interface MetricSample {
  key: string;
  value: number;
  unit: string;
  quality: 'measured' | 'derived' | 'estimated' | 'unknown';
  ts: number;
}
```

### Update Functions

```typescript
// Replace or merge metrics from a snapshot/delta
function updateMetrics(samples: MetricSample[]): void;
```

The `updateMetrics` function:
1. For each sample in the array, sets `metrics[sample.key] = sample`
2. Updates `lastUpdate` to the current time
3. Triggers reactive updates in any subscribed components

### Usage in Components

```tsx
import { metrics, connected } from '../lib/telemetry-store';

function CpuTile() {
  const cpu = metrics.value.get('cpu.total_pct');
  return (
    <div class="metric-tile">
      <span class="label">CPU</span>
      <span class="value">{cpu?.value.toFixed(1) ?? '—'}%</span>
      <span class={`status ${connected.value ? 'ok' : 'disconnected'}`} />
    </div>
  );
}
```

---

## Data Flow Summary

```
1-second tick
    │
    ▼
Collectors → MetricRegistry.push_sample()
    │
    ▼ (currently)                    ▼ (planned)
HTTP GET /api/v1/live/snapshot    WS broadcast_delta()
    │                                │
    ▼                                ▼
Browser fetch()                  Browser ws.onmessage()
    │                                │
    ▼                                ▼
updateMetrics() → Preact Signals → UI re-render
```

### Bandwidth

| Method | Payload/Second | Notes |
|--------|---------------|-------|
| Polling (snapshot) | ~2-5 KB | Full snapshot every request |
| WebSocket (delta) | ~0.5-1 KB | Only changed metrics |

WebSocket will reduce bandwidth by ~80% compared to polling, especially when most metrics don't change between ticks.

---

## Future Enhancements

| Feature | Status | Description |
|---------|--------|-------------|
| WebSocket broadcast | Planned | Server-side WS support via cpp-httplib |
| Selective subscription | Planned | Subscribe to specific metric keys only |
| Binary protocol | Considered | MessagePack or CBOR for lower overhead |
| Exponential backoff | Planned | Smarter reconnection with jitter |
| Connection health ping | Planned | Periodic ping/pong for stale connection detection |
