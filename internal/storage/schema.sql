-- ProtonSage initial SQLite schema draft.
-- ProtonDB report data is sourced from https://github.com/bdefore/protondb-data
-- and must retain ODbL/DbCL attribution in docs/exports.

CREATE TABLE IF NOT EXISTS sources (
    id TEXT PRIMARY KEY,
    kind TEXT NOT NULL,
    url TEXT NOT NULL,
    license TEXT NOT NULL,
    imported_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS import_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id TEXT NOT NULL REFERENCES sources(id),
    snapshot_filename TEXT NOT NULL,
    snapshot_date TEXT NOT NULL,
    started_at TEXT NOT NULL,
    finished_at TEXT,
    report_count INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS games (
    appid INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    launcher TEXT NOT NULL DEFAULT 'steam'
);

CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id TEXT NOT NULL REFERENCES sources(id),
    appid INTEGER NOT NULL,
    title TEXT,
    timestamp TEXT NOT NULL,
    verdict TEXT,
    rating TEXT,
    notes TEXT,
    launch_options TEXT,
    proton_version TEXT,
    raw_json TEXT
);

CREATE INDEX IF NOT EXISTS idx_reports_appid_timestamp ON reports(appid, timestamp DESC);

CREATE TABLE IF NOT EXISTS report_system_info (
    report_id INTEGER PRIMARY KEY REFERENCES reports(id) ON DELETE CASCADE,
    gpu_vendor TEXT,
    gpu_model TEXT,
    gpu_driver TEXT,
    cpu TEXT,
    ram_gb REAL,
    distro TEXT,
    kernel TEXT,
    desktop TEXT,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS launch_option_suggestions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    appid INTEGER NOT NULL,
    snippet TEXT NOT NULL,
    kind TEXT NOT NULL,
    occurrences INTEGER NOT NULL DEFAULT 1,
    newest_report_timestamp TEXT,
    confidence TEXT,
    source_report_ids TEXT NOT NULL
);

-- FTS tables will be added with the importer once exact searchable columns settle.
