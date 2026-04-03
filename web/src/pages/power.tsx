import { useState } from 'preact/hooks';
import { MetricTile } from '../components/metric-tile';
import { usePolling } from '../hooks/use-websocket';
import { getMetric } from '../lib/telemetry-store';
import { useApi } from '../hooks/use-api';

type EnergyDay = {
  day: string;
  energyWh: number;
  avgPowerW: number;
  peakPowerW: number;
  activeSeconds: number;
};

export function Power() {
  usePolling(1000);
  const [energyRange, setEnergyRange] = useState<'week' | 'month'>('week');

  const power = getMetric('power.current_w');
  const battLevel = getMetric('battery.level_pct');
  const acOnline = getMetric('battery.ac_online');
  const charging = getMetric('battery.charging');
  const remaining = getMetric('battery.remaining_min');

  const avg1m = getMetric('power.avg_1m_w');
  const avg5m = getMetric('power.avg_5m_w');
  const avg15m = getMetric('power.avg_15m_w');

  const rangeDays = energyRange === 'week' ? 7 : 30;
  const today = new Date().toISOString().slice(0, 10);
  const startDate = new Date(Date.now() - rangeDays * 86400000).toISOString().slice(0, 10);
  const { data: dailyEnergy } = useApi<EnergyDay[]>('/energy/daily', {
    start: startDate,
    end: today,
  });

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

      <div class="grid grid-4" style="margin-bottom: 16px;">
        <MetricTile
          label="Avg (1 min)"
          value={avg1m?.value ?? '—'}
          unit="W"
          quality={avg1m?.quality}
        />
        <MetricTile
          label="Avg (5 min)"
          value={avg5m?.value ?? '—'}
          unit="W"
          quality={avg5m?.quality}
        />
        <MetricTile
          label="Avg (15 min)"
          value={avg15m?.value ?? '—'}
          unit="W"
          quality={avg15m?.quality}
        />
      </div>

      <div class="card" style="margin-bottom: 16px;">
        <div class="card-header" style="display: flex; justify-content: space-between; align-items: center;">
          <span class="card-title">Energy Trends</span>
          <div style="display: flex; gap: 4px;">
            <button
              class={`btn btn-sm ${energyRange === 'week' ? 'btn-primary' : ''}`}
              onClick={() => setEnergyRange('week')}
            >
              7 Days
            </button>
            <button
              class={`btn btn-sm ${energyRange === 'month' ? 'btn-primary' : ''}`}
              onClick={() => setEnergyRange('month')}
            >
              30 Days
            </button>
          </div>
        </div>
        {dailyEnergy && dailyEnergy.length > 0 ? (
          <table class="table">
            <thead>
              <tr>
                <th>Date</th>
                <th>Energy (Wh)</th>
                <th>Avg Power (W)</th>
                <th>Peak Power (W)</th>
                <th>Active Time</th>
              </tr>
            </thead>
            <tbody>
              {dailyEnergy.map(d => (
                <tr key={d.day}>
                  <td>{d.day}</td>
                  <td>{d.energyWh?.toFixed(1) ?? '—'}</td>
                  <td>{d.avgPowerW?.toFixed(1) ?? '—'}</td>
                  <td>{d.peakPowerW?.toFixed(1) ?? '—'}</td>
                  <td>{formatDuration(d.activeSeconds)}</td>
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

function formatDuration(seconds: number): string {
  if (!seconds) return '—';
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
}
