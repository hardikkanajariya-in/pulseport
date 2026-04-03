export function Settings() {
  return (
    <div>
      <div class="page-header">
        <h1>Settings</h1>
        <p>Configure PulsePort service settings</p>
      </div>

      <div class="card" style="margin-bottom: 16px;">
        <div class="card-header">
          <span class="card-title">Server</span>
        </div>
        <p style="color: var(--text-secondary); font-size: 14px;">
          Configuration management will be available in a future release.
          Edit <code style="color: var(--accent);">config.json</code> directly and restart the service.
        </p>
      </div>

      <div class="card">
        <div class="card-header">
          <span class="card-title">Data Management</span>
        </div>
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <button class="btn btn-danger" onClick={handleDeleteAll}>
            Delete All History
          </button>
        </div>
      </div>
    </div>
  );
}

async function handleDeleteAll() {
  if (!confirm('This will permanently delete ALL metric history, events, and energy data. Configuration will be preserved.\n\nAre you sure?')) {
    return;
  }
  if (!confirm('This action is IRREVERSIBLE. Type "delete" in the next prompt to confirm.')) {
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
