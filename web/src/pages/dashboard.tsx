import { MetricTile } from '../components/metric-tile';
import { usePolling } from '../hooks/use-websocket';
import { metricList, connected, getMetric } from '../lib/telemetry-store';

export function Dashboard() {
  // Poll for live data (WebSocket upgrade will replace this)
  usePolling(1000);

  const conn = connected.value;
  const allMetrics = metricList.value;

  const cpu = getMetric('cpu.total_pct');
  const memUsed = getMetric('mem.used_pct');
  const memAvail = getMetric('mem.available_mb');
  const diskActive = getMetric('disk.active_pct');
  const diskRead = getMetric('disk.read_bps');
  const diskWrite = getMetric('disk.write_bps');
  const netRecv = getMetric('net.recv_bps');
  const netSend = getMetric('net.send_bps');
  const battLevel = getMetric('battery.level_pct');
  const power = getMetric('power.current_w');

  return (
    <div>
      <div class="page-header">
        <h1>Dashboard</h1>
        <p>
          {conn
            ? `Live — ${allMetrics.length} metrics`
            : 'Connecting...'}
        </p>
      </div>

      <div class="grid grid-4" style="margin-bottom: 16px;">
        <MetricTile
          label="CPU"
          value={cpu?.value ?? '—'}
          unit="%"
          quality={cpu?.quality}
        />
        <MetricTile
          label="Memory Used"
          value={memUsed?.value ?? '—'}
          unit="%"
          quality={memUsed?.quality}
        />
        <MetricTile
          label="Memory Available"
          value={memAvail?.value ?? '—'}
          unit="MB"
          quality={memAvail?.quality}
        />
        <MetricTile
          label="Disk Active"
          value={diskActive?.value ?? '—'}
          unit="%"
          quality={diskActive?.quality}
        />
      </div>

      <div class="grid grid-4" style="margin-bottom: 16px;">
        <MetricTile
          label="Disk Read"
          value={diskRead?.value ?? '—'}
          unit="B/s"
          quality={diskRead?.quality}
        />
        <MetricTile
          label="Disk Write"
          value={diskWrite?.value ?? '—'}
          unit="B/s"
          quality={diskWrite?.quality}
        />
        <MetricTile
          label="Network Recv"
          value={netRecv?.value ?? '—'}
          unit="B/s"
          quality={netRecv?.quality}
        />
        <MetricTile
          label="Network Send"
          value={netSend?.value ?? '—'}
          unit="B/s"
          quality={netSend?.quality}
        />
      </div>

      {(battLevel || power) && (
        <div class="grid grid-4">
          {battLevel && (
            <MetricTile
              label="Battery"
              value={battLevel.value}
              unit="%"
              quality={battLevel.quality}
            />
          )}
          {power && (
            <MetricTile
              label="Power Draw"
              value={power.value}
              unit="W"
              quality={power.quality}
            />
          )}
        </div>
      )}
    </div>
  );
}
