import { useState, useMemo } from 'preact/hooks';
import { useApi, apiPost } from '../hooks/use-api';
import { Chart } from '../components/chart';

export function History() {
  const [metric, setMetric] = useState('cpu.total_pct');
  const [resolution, setResolution] = useState('metric_1m');
  const [range, setRange] = useState(3600); // seconds

  const now = Math.floor(Date.now() / 1000);
  const { data, loading, refetch } = useApi<Array<{
    key: string;
    bucket_ts: number;
    avg: number;
    min: number;
    max: number;
  }>>('/history', {
    metric,
    resolution,
    start: String(now - range),
    end: String(now),
  });

  // Fetch events for overlay
  const { data: events } = useApi<Array<{
    ts: number;
    title: string;
    severity: string;
  }>>('/events', {
    start: String(now - range),
    end: String(now),
  });

  // Build chart data from history
  const chartData = useMemo(() => {
    if (!data || data.length === 0) return null;
    const timestamps = data.map(d => d.bucket_ts);
    const avgValues = data.map(d => d.avg);
    return [timestamps, avgValues] as [number[], number[]];
  }, [data]);

  const eventMarkers = useMemo(() => {
    return events?.map(e => ({
      ts: e.ts,
      title: e.title,
      severity: e.severity,
    })) ?? [];
  }, [events]);

  const handleExport = () => {
    if (!data || data.length === 0) return;

    const header = 'timestamp,metric,min,max,avg\n';
    const rows = data.map(d =>
      `${d.bucket_ts},${d.key},${d.min},${d.max},${d.avg}`
    ).join('\n');

    const blob = new Blob([header + rows], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `pulseport-${metric}-${resolution}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleDelete = async () => {
    if (!confirm('Delete history for this metric in the selected time range?')) return;
    try {
      await apiPost('/history/delete', {
        table: resolution,
        start: now - range,
        end: now,
      });
      refetch();
    } catch (e) {
      alert(`Delete failed: ${e instanceof Error ? e.message : e}`);
    }
  };

  return (
    <div>
      <div class="page-header">
        <h1>History Explorer</h1>
        <p>Browse and export historical metrics</p>
      </div>

      <div class="card" style="margin-bottom: 16px; display: flex; gap: 12px; align-items: center; flex-wrap: wrap;">
        <label>
          Metric:
          <select value={metric} onChange={e => setMetric((e.target as HTMLSelectElement).value)}
            style="margin-left: 8px; background: var(--bg-primary); color: var(--text-primary); border: 1px solid var(--border); border-radius: 4px; padding: 4px 8px;">
            <option value="cpu.total_pct">CPU %</option>
            <option value="mem.used_pct">Memory %</option>
            <option value="disk.active_pct">Disk %</option>
            <option value="net.recv_bps">Network Recv</option>
            <option value="power.current_w">Power (W)</option>
          </select>
        </label>
        <label>
          Resolution:
          <select value={resolution} onChange={e => setResolution((e.target as HTMLSelectElement).value)}
            style="margin-left: 8px; background: var(--bg-primary); color: var(--text-primary); border: 1px solid var(--border); border-radius: 4px; padding: 4px 8px;">
            <option value="metric_1m">1 Minute</option>
            <option value="metric_15m">15 Minutes</option>
          </select>
        </label>
        <label>
          Range:
          <select value={range} onChange={e => setRange(Number((e.target as HTMLSelectElement).value))}
            style="margin-left: 8px; background: var(--bg-primary); color: var(--text-primary); border: 1px solid var(--border); border-radius: 4px; padding: 4px 8px;">
            <option value={3600}>1 Hour</option>
            <option value={86400}>24 Hours</option>
            <option value={604800}>7 Days</option>
            <option value={2592000}>30 Days</option>
          </select>
        </label>
        <button class="btn" onClick={refetch}>Refresh</button>
        <button class="btn" onClick={handleExport}>Export CSV</button>
        <button class="btn btn-danger" onClick={handleDelete}>Delete Range</button>
      </div>

      {chartData && chartData[0].length > 0 && (
        <div style="margin-bottom: 16px;">
          <Chart
            title={metric}
            data={chartData}
            series={[
              {
                label: 'Avg',
                stroke: '#6366f1',
                width: 1.5,
                fill: 'rgba(99, 102, 241, 0.1)',
              },
            ]}
            height={240}
            events={eventMarkers}
          />
        </div>
      )}

      <div class="card">
        {loading ? (
          <p style="color: var(--text-muted); padding: 24px; text-align: center;">Loading...</p>
        ) : data && data.length > 0 ? (
          <table class="table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Min</th>
                <th>Avg</th>
                <th>Max</th>
              </tr>
            </thead>
            <tbody>
              {data.slice(0, 200).map((d, i) => (
                <tr key={i}>
                  <td>{new Date(d.bucket_ts * 1000).toLocaleString()}</td>
                  <td>{d.min?.toFixed(2)}</td>
                  <td>{d.avg?.toFixed(2)}</td>
                  <td>{d.max?.toFixed(2)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        ) : (
          <p style="color: var(--text-muted); padding: 24px; text-align: center;">
            No data for the selected range
          </p>
        )}
      </div>
    </div>
  );
}
