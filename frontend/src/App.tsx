import { useEffect, useMemo, useState } from 'react'
import {
  CheckCircle2,
  ClipboardCopy,
  DatabaseZap,
  HardDrive,
  MonitorCog,
  RadioTower,
  ShieldCheck,
  Sparkles,
  TerminalSquare,
} from 'lucide-react'
import { AppInfo, SystemProfile, getAppInfo, getSystemProfile } from './lib/wails'

const snapshot = {
  filename: 'reports_jun1_2026.tar.gz',
  source: 'bdefore/protondb-data',
  policy: 'Latest snapshot only by default',
}

const suggestions = [
  {
    label: 'MANGOHUD=1 %command%',
    kind: 'overlay',
    confidence: 'medium',
    sources: '3 fresh reports',
  },
  {
    label: 'gamemoderun %command%',
    kind: 'performance',
    confidence: 'high',
    sources: '7 fresh reports',
  },
  {
    label: 'PROTON_LOG=1 %command%',
    kind: 'diagnostic',
    confidence: 'low',
    sources: 'debug only',
  },
]

function App() {
  const [info, setInfo] = useState<AppInfo | null>(null)
  const [profile, setProfile] = useState<SystemProfile | null>(null)
  const [selected, setSelected] = useState(() => new Set(['gamemoderun %command%']))

  useEffect(() => {
    void getAppInfo().then(setInfo)
    void getSystemProfile().then(setProfile)
  }, [])

  const launchPreview = useMemo(() => {
    const parts = suggestions.filter((item) => selected.has(item.label)).map((item) => item.label.replace(' %command%', ''))
    if (parts.length === 0) return '%command%'
    return `${parts.join(' ')} %command%`
  }, [selected])

  const toggleSuggestion = (label: string) => {
    setSelected((current) => {
      const next = new Set(current)
      if (next.has(label)) next.delete(label)
      else next.add(label)
      return next
    })
  }

  return (
    <main className="shell">
      <section className="hero panel">
        <div className="hero__copy">
          <div className="eyebrow"><RadioTower size={15} /> Local-only Proton intelligence</div>
          <h1>ProtonSage</h1>
          <p>
            Fresh ProtonDB evidence, Steam library context and system matching — presented as a launch-option workbench
            you can trust before anything touches Steam.
          </p>
          <div className="hero__actions">
            <button className="button button--primary"><Sparkles size={18} /> Build recommendation</button>
            <button className="button button--ghost"><TerminalSquare size={18} /> CLI ready</button>
          </div>
        </div>
        <div className="hero__orb" aria-hidden="true">
          <div className="orb__ring" />
          <div className="orb__core">PS</div>
        </div>
      </section>

      <section className="status-grid">
        <StatusCard
          icon={<ShieldCheck />}
          title="Safety mode"
          value="Copy/export only"
          detail="No Steam config write path exists in the PoC."
          tone="green"
        />
        <StatusCard
          icon={<DatabaseZap />}
          title="ProtonDB"
          value={snapshot.filename}
          detail={`${snapshot.source} · ${snapshot.policy}`}
          tone="cyan"
        />
        <StatusCard
          icon={<MonitorCog />}
          title="System profile"
          value={profile?.gpuVendor || 'Detecting…'}
          detail={[profile?.distro, profile?.kernel].filter(Boolean).join(' · ') || 'Read-only local detection'}
          tone="amber"
        />
        <StatusCard
          icon={<HardDrive />}
          title="Steam scan"
          value="Read-only"
          detail="libraryfolders.vdf + appmanifest_*.acf foundation"
          tone="steel"
        />
      </section>

      <section className="workspace">
        <div className="panel game-list">
          <div className="section-title">
            <span>Installed games</span>
            <small>mock UI smoke test</small>
          </div>
          {['Cyberpunk 2077', 'Baldur’s Gate 3', 'Elden Ring', 'Puzzle Proton'].map((game, index) => (
            <button className={`game-row ${index === 0 ? 'game-row--active' : ''}`} key={game}>
              <span className="game-row__name">{game}</span>
              <span className="game-row__meta">appid {index === 3 ? '123' : 1000 + index}</span>
            </button>
          ))}
        </div>

        <div className="panel recommendation">
          <div className="section-title">
            <span>Recommendation preview</span>
            <small>rules engine placeholder</small>
          </div>
          <div className="confidence-strip">
            <div>
              <strong>Freshness wins</strong>
              <span>Old reports are context, not truth.</span>
            </div>
            <div>
              <strong>System-aware</strong>
              <span>GPU/driver/kernel differences become visible.</span>
            </div>
            <div>
              <strong>Cited</strong>
              <span>Every suggestion must point back to reports.</span>
            </div>
          </div>

          <div className="suggestions">
            {suggestions.map((item) => (
              <label className="suggestion" key={item.label}>
                <input
                  type="checkbox"
                  checked={selected.has(item.label)}
                  onChange={() => toggleSuggestion(item.label)}
                />
                <span className="suggestion__check"><CheckCircle2 size={18} /></span>
                <span className="suggestion__body">
                  <code>{item.label}</code>
                  <span>{item.kind} · {item.confidence} confidence · {item.sources}</span>
                </span>
              </label>
            ))}
          </div>

          <div className="launch-preview">
            <div>
              <span className="label">Launch option preview</span>
              <code>{launchPreview}</code>
            </div>
            <button className="button button--copy"><ClipboardCopy size={17} /> Copy</button>
          </div>
        </div>
      </section>

      <footer className="footer">
        <span>{info?.stack || 'Go core + Wails + React'}</span>
        <span>{info?.version || '0.1.0-dev'}</span>
      </footer>
    </main>
  )
}

function StatusCard({
  icon,
  title,
  value,
  detail,
  tone,
}: {
  icon: React.ReactNode
  title: string
  value: string
  detail: string
  tone: 'green' | 'cyan' | 'amber' | 'steel'
}) {
  return (
    <article className={`status-card status-card--${tone}`}>
      <div className="status-card__icon">{icon}</div>
      <div>
        <span className="status-card__title">{title}</span>
        <strong>{value}</strong>
        <p>{detail}</p>
      </div>
    </article>
  )
}

export default App
