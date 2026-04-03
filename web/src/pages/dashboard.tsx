import { useState } from 'preact/hooks';
import { MetricTile } from '../components/metric-tile';
import { usePolling } from '../hooks/use-websocket';
import { metricList, connected, getMetric } from '../lib/telemetry-store';

export function Dashboard() {
  usePolling(1000);
  const [showCores, setShowCores] = useState(false);

  const conn = connected.value;
  const allMetrics = metricList.value;

  const cpu = getMetric('cpu.total_pct');
  const memUsed = getMetric('mem.used_pct');
  const memAvail = getMetric('mem.available_mb');
  const memCommitted = getMetric('mem.committed_pct');
  const diskActive = getMetric('disk.active_pct');
  const diskRead = getMetric('disk.read_bps');
  const diskWrite = getMetric('disk.write_bps');
  const netRecv = getMetric('net.recv_bps');
  const netSend = getMetric('net.send_bps');
  const battLevel = getMetric('battery.level_pct');
  const power = getMetric('power.current_w');

  // Discover per-core CPU metrics dynamically
  const coreMetrics = allMetrics
    .filter(m => m.key.startsWith('cpu.core.') && m.key.endsWith('.pct'))
    .sort((a, b) => {
      const na = parseInt(a.key.split('.')[2]);
      const nb = parseInt(b.key.split('.')[2]);
      return na - nb;
    });

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
        {memCommitted && (
          <MetricTile
            label="Committed"
            value={memCommitted.value}
            unit="%"
            quality={memCommitted.quality}
          />
        )}
        {!memCommitted && (
          <MetricTile
            label="Disk Active"
            value={diskActive?.value ?? '—'}
            unit="%"
            quality={diskActive?.quality}
          />
        )}
      </div>

      {memCommitted && (
        <div class="grid grid-4" style="margin-bottom: 16px;">
          <MetricTile
            label="Disk Active"
            value={diskActive?.value ?? '—'}
            unit="%"
            quality={diskActive?.quality}
          />
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
        </div>
      )}

      {!memCommitted && (
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
      )}

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

      {coreMetrics.length > 0 && (
        <div class="card" style="margin-top: 16px;">
          <div class="card-header" style="cursor: pointer; user-select: none;"
               onClick={() => setShowCores(!showCores)}>
            <span class="card-title">
              {showCores ? '▾' : '▸'} Per-Core CPU ({coreMetrics.length} cores)
            </span>
          </div>
          {showCores && (
            <div class="grid grid-4" style="padding: 12px; gap: 8px;">
              {coreMetrics.map(core => {
                const idx = core.key.split('.')[2];
                return (
                  <MetricTile
                    key={core.key}
                    label={`Core ${idx}`}
                    value={core.value}
                    unit="%"
                    quality={core.quality}
                  />
                );
              })}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
