import { useRef, useEffect } from 'preact/hooks';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';

interface EventMarker {
  ts: number;
  title: string;
  severity: string;
}

interface ChartProps {
  title: string;
  data: uPlot.AlignedData;
  series: uPlot.Series[];
  width?: number;
  height?: number;
  events?: EventMarker[];
}

function eventOverlayPlugin(events: EventMarker[]): uPlot.Plugin {
  return {
    hooks: {
      draw: [
        (u: uPlot) => {
          const ctx = u.ctx;
          if (!ctx || !events.length) return;

          const { left, top, height: plotH } = u.bbox;
          ctx.save();

          for (const ev of events) {
            const x = u.valToPos(ev.ts, 'x', true);
            if (x < left || x > left + u.bbox.width) continue;

            ctx.beginPath();
            ctx.setLineDash([4, 4]);
            ctx.strokeStyle =
              ev.severity === 'critical' ? '#ef4444' :
              ev.severity === 'warn' ? '#f59e0b' : '#6366f1';
            ctx.lineWidth = 1.5;
            ctx.moveTo(x, top);
            ctx.lineTo(x, top + plotH);
            ctx.stroke();
            ctx.setLineDash([]);

            // Label
            ctx.fillStyle = ctx.strokeStyle;
            ctx.font = '10px sans-serif';
            ctx.fillText(ev.title.slice(0, 20), x + 3, top + 12);
          }

          ctx.restore();
        },
      ],
    },
  };
}

export function Chart({ title, data, series, height = 200, events }: ChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<uPlot | null>(null);

  useEffect(() => {
    if (!containerRef.current) return;

    const plugins: uPlot.Plugin[] = [];
    if (events?.length) {
      plugins.push(eventOverlayPlugin(events));
    }

    const opts: uPlot.Options = {
      width: containerRef.current.clientWidth,
      height,
      title,
      plugins,
      cursor: { show: true, drag: { x: true, y: false } },
      scales: {
        x: { time: true },
      },
      axes: [
        {
          stroke: '#71717a',
          grid: { stroke: 'rgba(113, 113, 122, 0.15)' },
          ticks: { stroke: 'rgba(113, 113, 122, 0.15)' },
          font: '11px -apple-system, sans-serif',
        },
        {
          stroke: '#71717a',
          grid: { stroke: 'rgba(113, 113, 122, 0.15)' },
          ticks: { stroke: 'rgba(113, 113, 122, 0.15)' },
          font: '11px -apple-system, sans-serif',
          size: 60,
        },
      ],
      series: [
        { label: 'Time' },
        ...series,
      ],
    };

    chartRef.current = new uPlot(opts, data, containerRef.current);

    const resizeObserver = new ResizeObserver(entries => {
      for (const entry of entries) {
        chartRef.current?.setSize({
          width: entry.contentRect.width,
          height,
        });
      }
    });
    resizeObserver.observe(containerRef.current);

    return () => {
      resizeObserver.disconnect();
      chartRef.current?.destroy();
    };
  }, [events]);

  // Update data when it changes
  useEffect(() => {
    if (chartRef.current && data[0]?.length > 0) {
      chartRef.current.setData(data);
    }
  }, [data]);

  return (
    <div class="card">
      <div ref={containerRef} />
    </div>
  );
}
