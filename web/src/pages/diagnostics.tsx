import { useApi } from '../hooks/use-api';

interface DiagnosticsData {
  version: string;
  uptime: number;
  lastSampleTime: number;
  lastFlushTime: number;
  wsConnections: number;
  dbSizeBytes: number;
  walSizeBytes: number;
  registeredMetrics: number;
  serviceMode: string;
}

export function Diagnostics() {
  const { data, loading } = useApi<DiagnosticsData>('/diagnostics');

  const formatBytes = (b: number): string => {
    if (b < 1024) return `${b} B`;
    if (b < 1024 * 1024) return `${(b / 1024).toFixed(1)} KB`;
    return `${(b / (1024 * 1024)).toFixed(1)} MB`;
  };

  const formatUptime = (s: number): string => {
    const days = Math.floor(s / 86400);
    const hours = Math.floor((s % 86400) / 3600);
    const mins = Math.floor((s % 3600) / 60);
    if (days > 0) return `${days}d ${hours}h ${mins}m`;
    if (hours > 0) return `${hours}h ${mins}m`;
    return `${mins}m ${s % 60}s`;
  };

  return (
    <div>
      <div class="page-header">
        <h1>Diagnostics</h1>
        <p>Internal service status and self-metrics</p>
      </div>

      <div class="card">
        {loading || !data ? (
          <p style="color: var(--text-muted); padding: 24px; text-align: center;">Loading...</p>
        ) : (
          <table class="table">
            <tbody>
              <tr><td>Version</td><td>{data.version}</td></tr>
              <tr><td>Service Mode</td><td>{data.serviceMode}</td></tr>
              <tr><td>Uptime</td><td>{formatUptime(data.uptime)}</td></tr>
              <tr><td>Registered Metrics</td><td>{data.registeredMetrics}</td></tr>
              <tr><td>WebSocket Connections</td><td>{data.wsConnections}</td></tr>
              <tr><td>Database Size</td><td>{formatBytes(data.dbSizeBytes)}</td></tr>
              <tr><td>WAL Size</td><td>{formatBytes(data.walSizeBytes)}</td></tr>
              <tr>
                <td>Last Sample</td>
                <td>{data.lastSampleTime
                  ? new Date(data.lastSampleTime * 1000).toLocaleString()
                  : '—'}</td>
              </tr>
              <tr>
                <td>Last Flush</td>
                <td>{data.lastFlushTime
                  ? new Date(data.lastFlushTime * 1000).toLocaleString()
                  : '—'}</td>
              </tr>
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}
