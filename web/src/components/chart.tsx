import { useRef, useEffect } from 'preact/hooks';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';

interface ChartProps {
  title: string;
  data: uPlot.AlignedData;
  series: uPlot.Series[];
  width?: number;
  height?: number;
}

export function Chart({ title, data, series, height = 200 }: ChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<uPlot | null>(null);

  useEffect(() => {
    if (!containerRef.current) return;

    const opts: uPlot.Options = {
      width: containerRef.current.clientWidth,
      height,
      title,
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
  }, []);

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
