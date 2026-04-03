import { useState } from 'preact/hooks';
import { Nav } from './components/nav';
import { Dashboard } from './pages/dashboard';
import { Power } from './pages/power';
import { History } from './pages/history';
import { Events } from './pages/events';
import { Settings } from './pages/settings';
import { Diagnostics } from './pages/diagnostics';

type Page = 'dashboard' | 'power' | 'history' | 'events' | 'settings' | 'diagnostics';

export function App() {
  const [page, setPage] = useState<Page>('dashboard');

  const renderPage = () => {
    switch (page) {
      case 'dashboard':   return <Dashboard />;
      case 'power':       return <Power />;
      case 'history':     return <History />;
      case 'events':      return <Events />;
      case 'settings':    return <Settings />;
      case 'diagnostics': return <Diagnostics />;
    }
  };

  return (
    <div class="app">
      <Nav currentPage={page} onNavigate={setPage} />
      <main class="main-content">
        {renderPage()}
      </main>
    </div>
  );
}
