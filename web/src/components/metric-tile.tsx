interface MetricTileProps {
  label: string;
  value: string | number;
  unit: string;
  quality?: string;
  trend?: 'up' | 'down' | 'stable';
}

export function MetricTile({ label, value, unit, quality }: MetricTileProps) {
  const qualityClass = quality ? `quality-${quality}` : '';

  return (
    <div class="card metric-tile">
      <div class="metric-label">
        {label}
        {quality && quality !== 'measured' && (
          <span class={`quality-badge ${qualityClass}`} style="margin-left: 8px;">
            {quality}
          </span>
        )}
      </div>
      <div class="metric-value">
        {typeof value === 'number' ? formatValue(value) : value}
        <span class="metric-unit">{unit}</span>
      </div>
    </div>
  );
}

function formatValue(v: number): string {
  if (Math.abs(v) >= 1_000_000) return (v / 1_000_000).toFixed(1) + 'M';
  if (Math.abs(v) >= 1_000) return (v / 1_000).toFixed(1) + 'K';
  if (Number.isInteger(v)) return v.toString();
  return v.toFixed(1);
}
