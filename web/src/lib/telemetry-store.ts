import { signal, computed } from '@preact/signals';

export interface MetricSample {
  key: string;
  value: number;
  unit: string;
  quality: string;
  ts: number;
}

// Live metric state
export const metrics = signal<Map<string, MetricSample>>(new Map());
export const connected = signal(false);
export const lastUpdate = signal(0);

// Derived values
export const metricList = computed(() => Array.from(metrics.value.values()));

export function updateMetrics(samples: MetricSample[]) {
  const m = new Map(metrics.value);
  for (const s of samples) {
    m.set(s.key, s);
  }
  metrics.value = m;
  lastUpdate.value = Date.now();
}

export function getMetric(key: string): MetricSample | undefined {
  return metrics.value.get(key);
}
