import { useApi } from '../hooks/use-api';

export function Events() {
  const now = Math.floor(Date.now() / 1000);
  const { data, loading } = useApi<Array<{
    id: number;
    ts: number;
    severity: string;
    category: string;
    title: string;
  }>>('/events', {
    start: String(now - 86400 * 7),
    end: String(now),
  });

  const severityColor = (s: string) => {
    switch (s) {
      case 'critical': return 'var(--danger)';
      case 'warn':     return 'var(--warning)';
      case 'info':     return 'var(--accent)';
      default:         return 'var(--text-muted)';
    }
  };

  return (
    <div>
      <div class="page-header">
        <h1>Events</h1>
        <p>System events, alerts, and lifecycle log</p>
      </div>

      <div class="card">
        {loading ? (
          <p style="color: var(--text-muted); padding: 24px; text-align: center;">Loading...</p>
        ) : data && data.length > 0 ? (
          <table class="table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Severity</th>
                <th>Category</th>
                <th>Title</th>
              </tr>
            </thead>
            <tbody>
              {data.map(e => (
                <tr key={e.id}>
                  <td>{new Date(e.ts * 1000).toLocaleString()}</td>
                  <td>
                    <span style={`color: ${severityColor(e.severity)}`}>
                      {e.severity}
                    </span>
                  </td>
                  <td>{e.category}</td>
                  <td>{e.title}</td>
                </tr>
              ))}
            </tbody>
          </table>
        ) : (
          <p style="color: var(--text-muted); padding: 24px; text-align: center;">
            No events recorded
          </p>
        )}
      </div>
    </div>
  );
}
