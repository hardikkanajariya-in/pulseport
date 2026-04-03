import { useState, useEffect } from 'preact/hooks';
import { apiPost } from '../hooks/use-api';

interface ConfigData {
  alert_cpu_high_pct: number;
  alert_cpu_sustained_min: number;
  alert_mem_high_pct: number;
  alert_mem_sustained_min: number;
  alert_battery_low_pct: number;
  alert_power_high_w: number;
  alert_cooldown_minutes: number;
  retention_1m_days: number;
  retention_15m_days: number;
  retention_daily_days: number;
  retention_events_days: number;
}

export function Settings() {
  const [config, setConfig] = useState<ConfigData | null>(null);
  const [saving, setSaving] = useState(false);
  const [status, setStatus] = useState('');

  useEffect(() => {
    fetch('/api/v1/config')
      .then(r => r.ok ? r.json() : null)
      .then(data => { if (data) setConfig(data); })
      .catch(() => {});
  }, []);

  const handleSave = async () => {
    if (!config) return;
    setSaving(true);
    setStatus('');
    try {
      await apiPost('/config', config);
      setStatus('Saved successfully');
    } catch (e) {
      setStatus(`Error: ${e instanceof Error ? e.message : e}`);
    } finally {
      setSaving(false);
    }
  };

  const update = (key: keyof ConfigData, value: number) => {
    if (config) setConfig({ ...config, [key]: value });
  };

  return (
    <div>
      <div class="page-header">
        <h1>Settings</h1>
        <p>Configure PulsePort alert thresholds and data retention</p>
      </div>

      {config ? (
        <>
          {/* Alert Thresholds */}
          <div class="card" style="margin-bottom: 16px;">
            <div class="card-header">
              <span class="card-title">Alert Thresholds</span>
            </div>
            <div style="padding: 16px; display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
              <SettingField
                label="CPU High (%)"
                value={config.alert_cpu_high_pct}
                min={1} max={100} step={1}
                onChange={v => update('alert_cpu_high_pct', v)}
              />
              <SettingField
                label="CPU Sustained (min)"
                value={config.alert_cpu_sustained_min}
                min={1} max={60} step={1}
                onChange={v => update('alert_cpu_sustained_min', v)}
              />
              <SettingField
                label="Memory High (%)"
                value={config.alert_mem_high_pct}
                min={1} max={100} step={1}
                onChange={v => update('alert_mem_high_pct', v)}
              />
              <SettingField
                label="Memory Sustained (min)"
                value={config.alert_mem_sustained_min}
                min={1} max={60} step={1}
                onChange={v => update('alert_mem_sustained_min', v)}
              />
              <SettingField
                label="Battery Low (%)"
                value={config.alert_battery_low_pct}
                min={1} max={50} step={1}
                onChange={v => update('alert_battery_low_pct', v)}
              />
              <SettingField
                label="Power High (W)"
                value={config.alert_power_high_w}
                min={1} max={1000} step={1}
                onChange={v => update('alert_power_high_w', v)}
              />
              <SettingField
                label="Alert Cooldown (min)"
                value={config.alert_cooldown_minutes}
                min={1} max={1440} step={1}
                onChange={v => update('alert_cooldown_minutes', v)}
              />
            </div>
          </div>

          {/* Data Retention */}
          <div class="card" style="margin-bottom: 16px;">
            <div class="card-header">
              <span class="card-title">Data Retention</span>
            </div>
            <div style="padding: 16px; display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
              <SettingField
                label="1-minute data (days)"
                value={config.retention_1m_days}
                min={1} max={3650} step={1}
                onChange={v => update('retention_1m_days', v)}
              />
              <SettingField
                label="15-minute data (days)"
                value={config.retention_15m_days}
                min={1} max={3650} step={1}
                onChange={v => update('retention_15m_days', v)}
              />
              <SettingField
                label="Daily data (days)"
                value={config.retention_daily_days}
                min={1} max={3650} step={1}
                onChange={v => update('retention_daily_days', v)}
              />
              <SettingField
                label="Events (days)"
                value={config.retention_events_days}
                min={1} max={3650} step={1}
                onChange={v => update('retention_events_days', v)}
              />
            </div>
          </div>

          <div style="display: flex; gap: 12px; align-items: center; margin-bottom: 16px;">
            <button class="btn btn-primary" onClick={handleSave} disabled={saving}>
              {saving ? 'Saving...' : 'Save Settings'}
            </button>
            {status && (
              <span style={`font-size: 14px; color: ${status.startsWith('Error') ? 'var(--danger)' : 'var(--success)'}`}>
                {status}
              </span>
            )}
          </div>
        </>
      ) : (
        <div class="card" style="margin-bottom: 16px;">
          <p style="color: var(--text-secondary); font-size: 14px; padding: 16px;">
            Loading configuration...
          </p>
        </div>
      )}

      <div class="card">
        <div class="card-header">
          <span class="card-title">Data Management</span>
        </div>
        <div style="display: flex; gap: 12px; flex-wrap: wrap; padding: 16px;">
          <button class="btn btn-danger" onClick={handleDeleteAll}>
            Delete All History
          </button>
        </div>
      </div>
    </div>
  );
}

function SettingField({ label, value, min, max, step, onChange }: {
  label: string;
  value: number;
  min: number;
  max: number;
  step: number;
  onChange: (v: number) => void;
}) {
  return (
    <label style="display: flex; flex-direction: column; gap: 4px;">
      <span style="font-size: 13px; color: var(--text-secondary);">{label}</span>
      <input
        type="number"
        value={value}
        min={min}
        max={max}
        step={step}
        onInput={e => onChange(Number((e.target as HTMLInputElement).value))}
        style="padding: 6px 10px; border: 1px solid var(--border); border-radius: 6px; background: var(--bg-secondary); color: var(--text-primary); font-size: 14px; width: 100%;"
      />
    </label>
  );
}

async function handleDeleteAll() {
  if (!confirm('This will permanently delete ALL metric history, events, and energy data. Configuration will be preserved.\n\nAre you sure?')) {
    return;
  }

  try {
    const res = await fetch('/api/v1/history/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ scope: 'all' }),
    });
    const data = await res.json();
    alert(`Deleted ${data.deleted} records.`);
  } catch (e) {
    alert(`Failed: ${e instanceof Error ? e.message : e}`);
  }
}
