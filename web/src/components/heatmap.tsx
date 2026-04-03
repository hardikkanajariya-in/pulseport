import { useRef, useEffect } from 'preact/hooks';

interface HeatmapProps {
  /** Array of { day: string, hour: number (0-23), value: number } */
  data: Array<{ day: string; hour: number; value: number }>;
  title?: string;
  unit?: string;
  maxValue?: number;
}

const CELL_W = 28;
const CELL_H = 16;
const LABEL_W = 80;
const LABEL_H = 24;
const HOURS = 24;

function valueToColor(v: number, max: number): string {
  if (max === 0) return 'rgba(99, 102, 241, 0.05)';
  const t = Math.min(v / max, 1);
  // Blue-green to yellow to red
  if (t < 0.5) {
    const r = Math.round(99 + (245 - 99) * (t * 2));
    const g = Math.round(102 + (158 - 102) * (t * 2));
    const b = Math.round(241 - 241 * (t * 2));
    return `rgb(${r}, ${g}, ${b})`;
  }
  const r = Math.round(245 + (239 - 245) * ((t - 0.5) * 2));
  const g = Math.round(158 - 90 * ((t - 0.5) * 2));
  const b = Math.round(11 * ((t - 0.5) * 2));
  return `rgb(${r}, ${g}, ${b})`;
}

export function Heatmap({ data, title = 'Power Heatmap', unit = 'W', maxValue }: HeatmapProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  // Group data by day
  const days = [...new Set(data.map(d => d.day))].sort();
  const max = maxValue ?? Math.max(...data.map(d => d.value), 1);

  const width = LABEL_W + HOURS * CELL_W + 8;
  const height = LABEL_H + days.length * CELL_H + 8;

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const dpr = window.devicePixelRatio || 1;
    canvas.width = width * dpr;
    canvas.height = height * dpr;
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, width, height);

    // Hour labels
    ctx.fillStyle = '#71717a';
    ctx.font = '10px sans-serif';
    ctx.textAlign = 'center';
    for (let h = 0; h < HOURS; h++) {
      if (h % 3 === 0) {
        ctx.fillText(`${h}`, LABEL_W + h * CELL_W + CELL_W / 2, LABEL_H - 4);
      }
    }

    // Build lookup
    const lookup = new Map<string, number>();
    for (const d of data) {
      lookup.set(`${d.day}:${d.hour}`, d.value);
    }

    // Draw cells
    for (let di = 0; di < days.length; di++) {
      const day = days[di];
      const y = LABEL_H + di * CELL_H;

      // Day label
      ctx.fillStyle = '#71717a';
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText(day.slice(5), LABEL_W - 6, y + CELL_H - 3);

      for (let h = 0; h < HOURS; h++) {
        const v = lookup.get(`${day}:${h}`) ?? 0;
        const x = LABEL_W + h * CELL_W;

        ctx.fillStyle = valueToColor(v, max);
        ctx.fillRect(x + 1, y + 1, CELL_W - 2, CELL_H - 2);
      }
    }
  }, [data, days.length, max]);

  return (
    <div class="card" style="margin-bottom: 16px;">
      <div class="card-header">
        <span class="card-title">{title}</span>
        <span style="font-size: 12px; color: var(--text-muted); margin-left: 8px;">
          max: {max.toFixed(0)} {unit}
        </span>
      </div>
      <div style="overflow-x: auto; padding: 8px;">
        <canvas ref={canvasRef} />
      </div>
    </div>
  );
}
