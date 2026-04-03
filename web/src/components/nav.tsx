interface NavProps {
  currentPage: string;
  onNavigate: (page: string) => void;
}

const navItems = [
  { id: 'dashboard',   label: 'Dashboard',   icon: '📊' },
  { id: 'power',       label: 'Power',       icon: '⚡' },
  { id: 'history',     label: 'History',     icon: '📈' },
  { id: 'events',      label: 'Events',      icon: '🔔' },
  { id: 'settings',    label: 'Settings',    icon: '⚙️' },
  { id: 'diagnostics', label: 'Diagnostics', icon: '🔧' },
];

export function Nav({ currentPage, onNavigate }: NavProps) {
  return (
    <nav class="sidebar">
      <div class="sidebar-brand">
        <span class="sidebar-logo">◉</span>
        <span class="sidebar-title">PulsePort</span>
      </div>
      <ul class="sidebar-nav">
        {navItems.map(item => (
          <li key={item.id}>
            <button
              class={`sidebar-link ${currentPage === item.id ? 'active' : ''}`}
              onClick={() => onNavigate(item.id)}
            >
              <span class="sidebar-icon">{item.icon}</span>
              {item.label}
            </button>
          </li>
        ))}
      </ul>
      <style>{`
        .sidebar {
          position: fixed;
          left: 0; top: 0; bottom: 0;
          width: 220px;
          background: var(--bg-secondary);
          border-right: 1px solid var(--border);
          display: flex;
          flex-direction: column;
          z-index: 100;
        }
        .sidebar-brand {
          display: flex;
          align-items: center;
          gap: 10px;
          padding: 20px 16px;
          border-bottom: 1px solid var(--border);
        }
        .sidebar-logo { font-size: 20px; color: var(--accent); }
        .sidebar-title { font-size: 16px; font-weight: 600; }
        .sidebar-nav {
          list-style: none;
          padding: 8px;
          flex: 1;
        }
        .sidebar-link {
          display: flex;
          align-items: center;
          gap: 10px;
          width: 100%;
          padding: 10px 12px;
          border: none;
          border-radius: var(--radius);
          background: transparent;
          color: var(--text-secondary);
          font-size: 14px;
          cursor: pointer;
          transition: all 0.15s;
          text-align: left;
        }
        .sidebar-link:hover {
          background: var(--bg-hover);
          color: var(--text-primary);
        }
        .sidebar-link.active {
          background: rgba(59, 130, 246, 0.12);
          color: var(--accent);
        }
        .sidebar-icon { font-size: 16px; }
        @media (max-width: 640px) {
          .sidebar {
            bottom: 0; top: auto; left: 0; right: 0;
            width: 100%; height: 56px;
            flex-direction: row;
            border-right: none;
            border-top: 1px solid var(--border);
          }
          .sidebar-brand { display: none; }
          .sidebar-nav {
            display: flex;
            align-items: center;
            justify-content: space-around;
            padding: 0;
            width: 100%;
          }
          .sidebar-link { flex-direction: column; gap: 2px; font-size: 10px; padding: 6px; }
        }
      `}</style>
    </nav>
  );
}
