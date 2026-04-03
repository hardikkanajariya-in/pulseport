import { MetricTile } from '../components/metric-tile';
import { usePolling } from '../hooks/use-websocket';
import { getMetric } from '../lib/telemetry-store';
import { useApi } from '../hooks/use-api';

export function Power() {
  usePolling(1000);

  const power = getMetric('power.current_w');
  const battLevel = getMetric('battery.level_pct');
  const acOnline = getMetric('battery.ac_online');
  const charging = getMetric('battery.charging');
  const remaining = getMetric('battery.remaining_min');

  // Daily energy for last 30 days
  const today = new Date().toISOString().slice(0, 10);
  const thirtyDaysAgo = new Date(Date.now() - 30 * 86400000).toISOString().slice(0, 10);
  const { data: dailyEnergy } = useApi<Array<{
    day: string;
    energyWh: number;
    avgPowerW: number;
  }>>('/energy/daily', { start: thirtyDaysAgo, end: today });

  return (
    <div>
      <div class="page-header">
        <h1>Power & Energy</h1>
        <p>Real-time power draw and energy consumption tracking</p>
      </div>

      <div class="grid grid-4" style="margin-bottom: 16px;">
        <MetricTile
          label="Current Power"
          value={power?.value ?? '—'}
          unit="W"
          quality={power?.quality}
        />
        <MetricTile
          label="Battery Level"
          value={battLevel?.value ?? '—'}
          unit="%"
          quality={battLevel?.quality}
        />
        <MetricTile
          label="AC Power"
          value={acOnline?.value === 1 ? 'Connected' : 'Battery'}
          unit=""
          quality={acOnline?.quality}
        />
        <MetricTile
          label={charging?.value === 1 ? 'Charging' : 'Time Remaining'}
          value={charging?.value === 1 ? 'Yes' : remaining?.value ?? '—'}
          unit={charging?.value === 1 ? '' : 'min'}
          quality={remaining?.quality}
        />
      </div>

      <div class="card" style="margin-bottom: 16px;">
        <div class="card-header">
          <span class="card-title">Daily Energy (Last 30 Days)</span>
        </div>
        {dailyEnergy && dailyEnergy.length > 0 ? (
          <table class="table">
            <thead>
              <tr>
                <th>Date</th>
                <th>Energy (Wh)</th>
                <th>Avg Power (W)</th>
              </tr>
            </thead>
            <tbody>
              {dailyEnergy.map(d => (
                <tr key={d.day}>
                  <td>{d.day}</td>
                  <td>{d.energyWh?.toFixed(1) ?? '—'}</td>
                  <td>{d.avgPowerW?.toFixed(1) ?? '—'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        ) : (
          <p style="color: var(--text-muted); padding: 24px; text-align: center;">
            No energy data recorded yet
          </p>
        )}
      </div>
    </div>
  );
}
