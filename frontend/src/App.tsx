import { useEffect, useMemo, useRef, useState, useCallback } from 'react'
import {
  CheckCircle2,
  ClipboardCopy,
  DatabaseZap,
  HardDrive,
  MonitorCog,
  RadioTower,
  ShieldCheck,
  Sparkles,
  Search,
  Loader2,
  AlertTriangle,
  X,
  ChevronDown,
  ChevronUp,
  Info,
  Zap,
  Eye,
  FileText,
} from 'lucide-react'
import {
  type AppInfo,
  type DataStatus,
  type InstalledGameStatus,
  type SystemProfile,
  type Recommendation,
  type Suggestion,
  type RankedReport,
  type Game,
  getAppInfo,
  getSystemProfile,
  getDataStatus,
  getInstalledGames,
  getRecommendation,
  buildLaunchPreview,
  copyToClipboard,
} from './lib/wails'

// ─── Freshness colours ───────────────────────────────────────────────
function freshnessLabel(f: string): string {
  switch (f) {
    case 'fresh': return 'Fresh'
    case 'recent': return 'Recent'
    case 'stale': return 'Stale'
    case 'historical': return 'Historical'
    default: return f
  }
}

function freshnessClass(f: string): string {
  switch (f) {
    case 'fresh': return 'fresh'
    case 'recent': return 'recent'
    case 'stale': return 'stale'
    case 'historical': return 'historical'
    default: return ''
  }
}

function confidenceClass(c: string): string {
  switch (c) {
    case 'high': return 'confidence-high'
    case 'medium': return 'confidence-medium'
    default: return 'confidence-low'
  }
}

function kindLabel(k: string): string {
  switch (k) {
    case 'launch_option': return 'Launch Option'
    case 'env_var': return 'Env Var'
    case 'wrapper': return 'Wrapper'
    case 'workaround': return 'Workaround'
    case 'diagnostic': return 'Diagnostic'
    case 'note': return 'Note'
    default: return k
  }
}

function kindIcon(k: string): React.ReactNode {
  switch (k) {
    case 'launch_option': return <Zap size={14} />
    case 'env_var': return <FileText size={14} />
    case 'wrapper': return <Sparkles size={14} />
    case 'workaround': return <CheckCircle2 size={14} />
    case 'diagnostic': return <AlertTriangle size={14} />
    case 'note': return <Info size={14} />
    default: return <Info size={14} />
  }
}

// ─── Main App ────────────────────────────────────────────────────────
function App() {
  const [info, setInfo] = useState<AppInfo | null>(null)
  const [profile, setProfile] = useState<SystemProfile | null>(null)
  const [dataStatus, setDataStatus] = useState<DataStatus | null>(null)
  const [installedGames, setInstalledGames] = useState<InstalledGameStatus[]>([])
  const [selectedGame, setSelectedGame] = useState<InstalledGameStatus | null>(null)
  const [recommendation, setRecommendation] = useState<Recommendation | null>(null)
  const [selectedSuggestions, setSelectedSuggestions] = useState<Set<string>>(new Set())
  const [searchQuery, setSearchQuery] = useState('')
  const [loadingGames, setLoadingGames] = useState(false)
  const [loadingRec, setLoadingRec] = useState(false)
  const [copied, setCopied] = useState(false)
  const [gamesError, setGamesError] = useState<string | null>(null)
  const [recError, setRecError] = useState<string | null>(null)
  const [showAllSuggestions, setShowAllSuggestions] = useState(false)
  const [showEvidence, setShowEvidence] = useState(false)
  const previewTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  // Load static info
  useEffect(() => {
    void getAppInfo().then(setInfo)
    void getSystemProfile().then(setProfile)
    void getDataStatus().then(setDataStatus)
  }, [])

  // Load installed games on mount
  useEffect(() => {
    let cancelled = false
    setLoadingGames(true)
    setGamesError(null)
    getInstalledGames()
      .then((games) => {
        if (!cancelled) setInstalledGames(games ?? [])
      })
      .catch((err: Error) => {
        if (!cancelled) setGamesError(err.message ?? 'Failed to load installed games')
      })
      .finally(() => {
        if (!cancelled) setLoadingGames(false)
      })
    return () => { cancelled = true }
  }, [])

  // Load recommendation when game selected
  useEffect(() => {
    if (!selectedGame?.hasProtonDbReports || !selectedGame.protonDbAppId) {
      setRecommendation(null)
      setSelectedSuggestions(new Set())
      return
    }
    let cancelled = false
    setLoadingRec(true)
    setRecError(null)
    getRecommendation(selectedGame.protonDbAppId)
      .then((rec) => {
        if (!cancelled) {
          setRecommendation(rec)
          // Auto-select top suggestions
          const topIds = rec.suggestions
            .filter((s) => s.kind !== 'diagnostic' && s.kind !== 'note')
            .slice(0, 3)
            .map((s) => s.id)
          setSelectedSuggestions(new Set(topIds))
        }
      })
      .catch((err: Error) => {
        if (!cancelled) setRecError(err.message ?? 'Recommendation failed')
      })
      .finally(() => {
        if (!cancelled) setLoadingRec(false)
      })
    return () => { cancelled = true }
  }, [selectedGame])

  // Compute launch preview
  const preview = useMemo(() => {
    if (!recommendation || selectedSuggestions.size === 0) return null
    const selected = recommendation.suggestions.filter((s) => selectedSuggestions.has(s.id))
    // For now compute locally — proper call would be async through buildLaunchPreview
    return computePreviewLocal(selected, selectedGame?.game.existingLaunchOptions ?? '')
  }, [recommendation, selectedSuggestions, selectedGame])

  const toggleSuggestion = useCallback((id: string) => {
    setSelectedSuggestions((prev) => {
      const next = new Set(prev)
      if (next.has(id)) next.delete(id)
      else next.add(id)
      return next
    })
  }, [])

  const handleCopy = useCallback(async (text: string) => {
    const ok = await copyToClipboard(text)
    if (ok) {
      setCopied(true)
      if (previewTimer.current) clearTimeout(previewTimer.current)
      previewTimer.current = setTimeout(() => setCopied(false), 2000)
    }
  }, [])

  const filteredGames = useMemo(() => {
    if (!searchQuery.trim()) return installedGames
    const q = searchQuery.toLowerCase()
    return installedGames.filter((g) => g.game.name.toLowerCase().includes(q))
  }, [installedGames, searchQuery])

  // Filter suggestions for display
  const visibleSuggestions = useMemo(() => {
    if (!recommendation) return []
    const actionable = recommendation.suggestions.filter(
      (s) => s.kind !== 'diagnostic' && s.kind !== 'note',
    )
    const notes = recommendation.suggestions.filter(
      (s) => s.kind === 'diagnostic' || s.kind === 'note',
    )
    if (showAllSuggestions) return [...actionable, ...notes]
    return [...actionable.slice(0, 8), ...notes.slice(0, 3)]
  }, [recommendation, showAllSuggestions])

  const hasMoreSuggestions = useMemo(() => {
    if (!recommendation) return false
    return recommendation.suggestions.length > 11
  }, [recommendation])

  const topReport = recommendation?.rankedReports?.[0]
  const existingLaunchOptions = selectedGame?.game.existingLaunchOptions

  return (
    <main className="shell">
      {/* Header */}
      <header className="header">
        <div className="header__brand">
          <span className="header__logo">PS</span>
          <div>
            <h1 className="header__title">ProtonSage</h1>
            <span className="header__sub">Local Proton intelligence for Steam</span>
          </div>
        </div>
        <div className="header__status">
          <StatusBadge icon={<ShieldCheck size={14} />} label="Safe" value="Copy / Export only" tone="green" />
          <StatusBadge icon={<DatabaseZap size={14} />} label="Data" value={dataStatus ? `${dataStatus.reportCount?.toLocaleString() ?? '—'} reports` : '—'} tone="cyan" />
          <StatusBadge icon={<MonitorCog size={14} />} label="System" value={profile?.gpuVendor ?? '—'} tone="amber" />
          <span className="header__ver">v{info?.version ?? '…'}</span>
        </div>
      </header>

      {/* Data status bar (shown when no data) */}
      {dataStatus && dataStatus.reportCount === 0 && (
        <div className="nodata-bar">
          <AlertTriangle size={16} />
          <span>No ProtonDB data imported. Use <code>protonsage import-fixture</code> or <code>protonsage download-snapshot</code> first, then re-open this app.</span>
        </div>
      )}

      {/* Main workspace */}
      <div className="workspace">
        {/* Game list sidebar */}
        <aside className="sidebar panel">
          <div className="sidebar__head">
            <h2>Installed Games</h2>
            <span className="sidebar__count">{installedGames.length > 0 ? `${installedGames.length} games` : ''}</span>
          </div>

          <div className="search-box">
            <Search size={16} />
            <input
              type="text"
              placeholder="Filter games…"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
            />
          </div>

          <div className="sidebar__list">
            {loadingGames && (
              <div className="sidebar__loading">
                <Loader2 size={20} className="spin" />
                <span>Scanning Steam libraries…</span>
              </div>
            )}
            {gamesError && (
              <div className="sidebar__error">
                <AlertTriangle size={18} />
                <span>{gamesError}</span>
              </div>
            )}
            {!loadingGames && !gamesError && filteredGames.length === 0 && (
              <div className="sidebar__empty">
                {installedGames.length === 0
                  ? 'No Steam games found. Ensure Steam is installed and libraries are accessible.'
                  : 'No games match your search.'}
              </div>
            )}
            {filteredGames.map((g) => (
              <button
                key={g.game.appId}
                className={`game-row ${selectedGame?.game.appId === g.game.appId ? 'game-row--active' : ''}`}
                onClick={() => { setSelectedGame(g); setShowAllSuggestions(false); setShowEvidence(false) }}
              >
                <span className="game-row__name">{g.game.name}</span>
                <span className="game-row__meta">
                  <span className="game-row__appid">appid {g.game.appId}</span>
                  {g.hasProtonDbReports ? (
                    <span className={`badge badge--${g.reportCount > 50 ? 'green' : g.reportCount > 10 ? 'cyan' : 'amber'}`}>
                      {g.reportCount} report{g.reportCount !== 1 ? 's' : ''}
                    </span>
                  ) : (
                    <span className="badge badge--muted">no data</span>
                  )}
                </span>
              </button>
            ))}
          </div>

          {/* System profile summary */}
          {profile && (
            <div className="sidebar__profile">
              <div className="sidebar__profile-title">
                <MonitorCog size={14} /> System Profile
              </div>
              <div className="sidebar__profile-grid">
                <ProfileField label="GPU" value={`${profile.gpuModel ?? '—'}`} />
                <ProfileField label="Driver" value={profile.gpuDriver ?? '—'} />
                <ProfileField label="CPU" value={profile.cpu ?? '—'} />
                <ProfileField label="RAM" value={profile.ramGb ? `${profile.ramGb.toFixed(1)} GB` : '—'} />
                <ProfileField label="Distro" value={profile.distro ?? '—'} />
                <ProfileField label="Session" value={profile.sessionType ?? '—'} />
              </div>
            </div>
          )}
        </aside>

        {/* Main recommendation panel */}
        <section className="rec panel">
          {!selectedGame ? (
            <div className="rec__empty">
              <RadioTower size={48} strokeWidth={1.2} />
              <h2>Select a game</h2>
              <p>Choose an installed game from the sidebar to see ProtonDB-based recommendations and launch options.</p>
            </div>
          ) : loadingRec ? (
            <div className="rec__loading">
              <Loader2 size={32} className="spin" />
              <span>Analyzing ProtonDB reports…</span>
            </div>
          ) : recError ? (
            <div className="rec__error">
              <AlertTriangle size={28} />
              <span>{recError}</span>
            </div>
          ) : recommendation ? (
            <RecommendationView
              recommendation={recommendation}
              selectedSuggestions={selectedSuggestions}
              onToggle={toggleSuggestion}
              preview={preview}
              existingLaunchOptions={existingLaunchOptions ?? ''}
              copied={copied}
              onCopy={handleCopy}
              showAllSuggestions={showAllSuggestions}
              onToggleShowAll={() => setShowAllSuggestions((v) => !v)}
              hasMoreSuggestions={hasMoreSuggestions}
              showEvidence={showEvidence}
              onToggleEvidence={() => setShowEvidence((v) => !v)}
            />
          ) : !selectedGame.hasProtonDbReports ? (
            <div className="rec__empty">
              <X size={40} strokeWidth={1.5} />
              <h2>{selectedGame.game.name}</h2>
              <p>No ProtonDB reports found for this game. Import more data or check back later.</p>
            </div>
          ) : null}
        </section>
      </div>

      <footer className="footer">
        <span>{info?.stack ?? 'Go core + Wails + React'}</span>
        <span>Data: ODbL/DbCL · <a href="https://github.com/bdefore/protondb-data" target="_blank" rel="noreferrer">bdefore/protondb-data</a></span>
      </footer>
    </main>
  )
}

// ─── Recommendation View ─────────────────────────────────────────────
function RecommendationView({
  recommendation,
  selectedSuggestions,
  onToggle,
  preview,
  existingLaunchOptions,
  copied,
  onCopy,
  showAllSuggestions,
  onToggleShowAll,
  hasMoreSuggestions,
  showEvidence,
  onToggleEvidence,
}: {
  recommendation: Recommendation
  selectedSuggestions: Set<string>
  onToggle: (id: string) => void
  preview: { text: string; applied: Suggestion[]; conflicts: string[] } | null
  existingLaunchOptions: string
  copied: boolean
  onCopy: (text: string) => void
  showAllSuggestions: boolean
  onToggleShowAll: () => void
  hasMoreSuggestions: boolean
  showEvidence: boolean
  onToggleEvidence: () => void
}) {
  const { suggestions, rankedReports = [], warnings = [], game, summary } = recommendation

  // Count by kind
  const actionableCount = suggestions.filter((s) => s.kind !== 'diagnostic' && s.kind !== 'note').length
  const noteCount = suggestions.filter((s) => s.kind === 'note').length
  const diagCount = suggestions.filter((s) => s.kind === 'diagnostic').length

  const freshCount = rankedReports.filter((r) => r.freshness === 'fresh').length
  const recentCount = rankedReports.filter((r) => r.freshness === 'recent').length

  return (
    <div className="rec__content">
      {/* Game header */}
      <div className="rec__header">
        <div>
          <h2>{game.name}</h2>
          <span className="rec__meta">
            <span className="badge badge--cyan">{rankedReports.length} report{rankedReports.length !== 1 ? 's' : ''}</span>
            {freshCount > 0 && <span className="badge badge--green">{freshCount} fresh</span>}
            {recentCount > 0 && <span className="badge badge--amber">{recentCount} recent</span>}
          </span>
        </div>
      </div>

      {/* Summary */}
      <div className="rec__summary">
        <p>{summary}</p>
        {warnings.length > 0 && (
          <div className="rec__warnings">
            {warnings.map((w, i) => (
              <div key={i} className="rec__warning">
                <AlertTriangle size={14} /> {w}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Confidence strip */}
      <div className="confidence-strip">
        <div>
          <strong>Freshness wins</strong>
          <span>Recent reports weigh more than stale ones.</span>
        </div>
        <div>
          <strong>System-aware</strong>
          <span>GPU/driver/kernel differences shown.</span>
        </div>
        <div>
          <strong>Cited</strong>
          <span>Every suggestion points to source reports.</span>
        </div>
      </div>

      {/* Suggestions */}
      <div className="rec__section">
        <div className="rec__section-head">
          <h3>Suggestions</h3>
          <span className="rec__section-meta">
            {actionableCount} actionable
            {noteCount > 0 && ` · ${noteCount} notes`}
            {diagCount > 0 && ` · ${diagCount} diagnostics`}
          </span>
        </div>
        <div className="suggestions">
          {(() => {
            const allSuggestions = showAllSuggestions ? suggestions : (() => {
              const actionable = suggestions.filter((s) => s.kind !== 'diagnostic' && s.kind !== 'note')
              const notes = suggestions.filter((s) => s.kind === 'note')
              const diags = suggestions.filter((s) => s.kind === 'diagnostic')
              return [...actionable.slice(0, 8), ...notes.slice(0, 3), ...diags.slice(0, 2)]
            })()
            return allSuggestions.map((s) => {
              const isActionable = s.kind !== 'diagnostic' && s.kind !== 'note'
              const isActionableNote = s.kind === 'note'
              const isSelected = selectedSuggestions.has(s.id)
              return (
                <label key={s.id} className={`suggestion ${isSelected ? 'suggestion--selected' : ''} ${!isActionable ? 'suggestion--info' : ''}`}>
                  {isActionable ? (
                    <input
                      type="checkbox"
                      checked={isSelected}
                      onChange={() => onToggle(s.id)}
                    />
                  ) : null}
                  <span className={`suggestion__check ${isActionableNote ? 'suggestion__check--note' : ''}`}>
                    {isSelected ? <CheckCircle2 size={18} /> : kindIcon(s.kind)}
                  </span>
                  <span className="suggestion__body">
                    <code>{s.snippet}</code>
                    <span>
                      <span className={`badge badge--sm badge--${confidenceClass(s.confidence)}`}>{s.confidence}</span>
                      {' · '}
                      {kindLabel(s.kind)}
                      {' · '}
                      {s.occurrences}×
                      {s.systemSimilarity > 0 && ` · sim ${Math.round(s.systemSimilarity * 100)}%`}
                      {s.conflictNotes && s.conflictNotes.length > 0 && (
                        <span className="suggestion__conflict"> ⚠ {s.conflictNotes[0]}</span>
                      )}
                    </span>
                  </span>
                </label>
              )
            })
          })()}
          {hasMoreSuggestions && (
            <button className="button button--ghost button--sm" onClick={onToggleShowAll}>
              {showAllSuggestions ? <><ChevronUp size={14} /> Show less</> : <><ChevronDown size={14} /> Show all {suggestions.length} suggestions</>}
            </button>
          )}
        </div>
      </div>

      {/* Launch preview */}
      {preview && selectedSuggestions.size > 0 && (
        <div className="rec__section">
          <div className="rec__section-head">
            <h3>Launch Option Preview</h3>
            <div className="safety-badge">
              <ShieldCheck size={13} /> Copy / export only — no Steam writes
            </div>
          </div>
          <div className="launch-preview">
            <div className="launch-preview__code">
              <code>{preview.text}</code>
            </div>
            <div className="launch-preview__actions">
              <button
                className={`button ${copied ? 'button--copied' : 'button--copy'}`}
                onClick={() => onCopy(preview.text)}
              >
                <ClipboardCopy size={17} /> {copied ? 'Copied!' : 'Copy'}
              </button>
            </div>
          </div>
          {preview.conflicts.length > 0 && (
            <div className="rec__conflict-list">
              {preview.conflicts.map((c, i) => (
                <div key={i} className="rec__conflict"><AlertTriangle size={13} /> {c}</div>
              ))}
            </div>
          )}
          {existingLaunchOptions && (
            <div className="rec__existing">
              <Info size={14} /> Current Steam launch options: <code>{existingLaunchOptions}</code>
            </div>
          )}
        </div>
      )}

      {/* Evidence toggle */}
      <div className="rec__section">
        <button className="button button--ghost button--sm" onClick={onToggleEvidence}>
          <Eye size={14} /> {showEvidence ? 'Hide evidence' : 'Show top evidence'}
        </button>
        {showEvidence && rankedReports.length > 0 && (
          <div className="evidence-list">
            {rankedReports.slice(0, 5).map((r) => (
              <div key={r.report.sourceReportId} className="evidence-row">
                <div className="evidence-row__head">
                  <span className={`badge badge--sm badge--${freshnessClass(r.freshness)}`}>{freshnessLabel(r.freshness)}</span>
                  <span className="evidence-row__rating">{r.report.rating || '—'}</span>
                  <span className="evidence-row__date">{r.report.timestamp ? new Date(r.report.timestamp).toLocaleDateString() : '—'}</span>
                  <span className="evidence-row__sim">sim {Math.round(r.systemSimilarity * 100)}%</span>
                </div>
                {r.report.launchOptions && (
                  <code className="evidence-row__launch">{r.report.launchOptions}</code>
                )}
                {r.report.notes && (
                  <p className="evidence-row__notes">{r.report.notes.slice(0, 200)}{r.report.notes.length > 200 ? '…' : ''}</p>
                )}
                {r.similarity?.summary && (
                  <span className="evidence-row__similarity">{r.similarity.summary}</span>
                )}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  )
}

// ─── Small Components ────────────────────────────────────────────────

function StatusBadge({
  icon,
  label,
  value,
  tone,
}: {
  icon: React.ReactNode
  label: string
  value: string
  tone: 'green' | 'cyan' | 'amber' | 'steel'
}) {
  return (
    <div className={`status-badge status-badge--${tone}`}>
      {icon}
      <span className="status-badge__label">{label}</span>
      <span className="status-badge__value">{value}</span>
    </div>
  )
}

function ProfileField({ label, value }: { label: string; value: string }) {
  return (
    <div className="profile-field">
      <span className="profile-field__label">{label}</span>
      <span className="profile-field__value">{value}</span>
    </div>
  )
}

// ─── Local preview computation (matches Go backend logic) ────────────
function computePreviewLocal(
  selected: Suggestion[],
  existing: string,
): { text: string; applied: Suggestion[]; conflicts: string[] } | null {
  if (selected.length === 0) return null

  // Extract env vars, wrappers, game args from selected suggestions
  const envVars: string[] = []
  const wrappers: string[] = []
  const gameArgs: string[] = []
  const applied: Suggestion[] = []

  for (const s of selected) {
    const snippet = s.snippet
    if (!snippet || snippet.toLowerCase() === '(none)' || snippet.toLowerCase() === 'none') continue

    // Check for %command% or %COMMAND%
    const cmdIdxLower = snippet.toLowerCase().indexOf('%command%')
    if (cmdIdxLower !== -1) {
      const cmdIdx = snippet.toLowerCase().indexOf('%command%')
      const beforeCommand = snippet.slice(0, cmdIdx).trim()
      const afterCommand = snippet.slice(cmdIdx + '%command%'.length).trim()
      if (beforeCommand) {
        // Parse as space-separated tokens for env vars and wrappers
        const tokens = shellSplit(beforeCommand)
        for (const t of tokens) {
          if (t.includes('=') && /^[A-Z_]/.test(t)) envVars.push(t)
          else wrappers.push(t)
        }
      }
      if (afterCommand) gameArgs.push(afterCommand)
      applied.push(s)
      continue
    }

    // Pure env var
    if (/^[A-Z_][A-Z0-9_]*=/.test(snippet)) {
      envVars.push(snippet)
      applied.push(s)
      continue
    }

    // Wrapper or game arg
    if (/^[a-z]/.test(snippet) && !snippet.includes('=')) {
      // Could be a wrapper command or game arg
      if (['gamemoderun', 'mangohud', 'gamescope', 'prime-run', 'game-performance', 'obs-gamecapture', 'dlss-swapper'].some((w) => snippet.toLowerCase().startsWith(w))) {
        wrappers.push(snippet)
      } else {
        gameArgs.push(snippet)
      }
      applied.push(s)
      continue
    }

    // Fallback — treat as game arg
    gameArgs.push(snippet)
    applied.push(s)
  }

  // Handle existing launch options
  let existingEnvVars: string[] = []
  let existingWrappers: string[] = []
  let existingGameArgs: string[] = []

  if (existing) {
    const existingCmdIdx = existing.toLowerCase().indexOf('%command%')
    if (existingCmdIdx !== -1) {
      const beforeCmd = existing.slice(0, existingCmdIdx).trim()
      const afterCmd = existing.slice(existingCmdIdx + '%command%'.length).trim()
      if (beforeCmd) {
        const tokens = shellSplit(beforeCmd)
        for (const t of tokens) {
          if (t.includes('=') && /^[A-Z_]/.test(t)) existingEnvVars.push(t)
          else existingWrappers.push(t)
        }
      }
      if (afterCmd) existingGameArgs.push(afterCmd)
    } else {
      // No %command% — treat as game args appended
      existingGameArgs.push(existing)
    }
  }

  // Detect env var conflicts
  const envVarMap = new Map<string, string[]>()
  const allEnvVars = [...existingEnvVars, ...envVars]
  for (const ev of allEnvVars) {
    const eqIdx = ev.indexOf('=')
    if (eqIdx !== -1) {
      const key = ev.slice(0, eqIdx)
      const val = ev.slice(eqIdx + 1)
      if (!envVarMap.has(key)) envVarMap.set(key, [])
      envVarMap.get(key)!.push(val)
    }
  }
  const conflicts: string[] = []
  for (const [key, vals] of envVarMap) {
    if (vals.length > 1) {
      const unique = [...new Set(vals)]
      if (unique.length > 1) {
        conflicts.push(`Conflicting values for ${key}: ${unique.join(', ')}`)
      }
    }
  }

  // Compose: existing env/wrappers, new env/wrappers, %command%, existing args, new args
  const parts: string[] = []
  parts.push(...existingEnvVars)
  parts.push(...envVars)
  parts.push(...existingWrappers)
  parts.push(...wrappers)
  parts.push('%command%')
  parts.push(...existingGameArgs)
  parts.push(...gameArgs)

  return {
    text: parts.filter(Boolean).join(' '),
    applied,
    conflicts,
  }
}

function shellSplit(s: string): string[] {
  const result: string[] = []
  let current = ''
  let inQuote = false
  let quoteChar = ''
  for (const ch of s) {
    if (inQuote) {
      if (ch === quoteChar) {
        inQuote = false
        current += ch
      } else {
        current += ch
      }
    } else if (ch === '"' || ch === "'") {
      inQuote = true
      quoteChar = ch
      current += ch
    } else if (ch === ' ') {
      if (current) result.push(current)
      current = ''
    } else {
      current += ch
    }
  }
  if (current) result.push(current)
  return result
}

export default App