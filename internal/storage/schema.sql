-- ProtonSage SQLite schema.
-- ProtonDB report data is sourced from https://github.com/bdefore/protondb-data
-- and must retain ODbL/DbCL attribution in docs/exports/import metadata.

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
    source_url TEXT NOT NULL,
    license TEXT NOT NULL,
    started_at TEXT NOT NULL,
    finished_at TEXT,
    report_count INTEGER NOT NULL DEFAULT 0,
    skipped_count INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS games (
    appid INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    launcher TEXT NOT NULL DEFAULT 'steam'
);

CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    import_run_id INTEGER NOT NULL REFERENCES import_runs(id) ON DELETE CASCADE,
    source_id TEXT NOT NULL REFERENCES sources(id),
    source_report_id TEXT NOT NULL,
    appid INTEGER NOT NULL REFERENCES games(appid) ON DELETE CASCADE,
    title TEXT,
    timestamp TEXT NOT NULL,
    verdict TEXT,
    rating TEXT,
    notes TEXT,
    launch_options TEXT,
    proton_version TEXT,
    raw_json TEXT NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_reports_appid_timestamp ON reports(appid, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_reports_source_report ON reports(source_id, source_report_id);

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

CREATE VIRTUAL TABLE IF NOT EXISTS games_fts USING fts5(
    name,
    content='games',
    content_rowid='appid'
);

CREATE TRIGGER IF NOT EXISTS games_ai AFTER INSERT ON games BEGIN
    INSERT INTO games_fts(rowid, name) VALUES (new.appid, new.name);
END;

CREATE TRIGGER IF NOT EXISTS games_ad AFTER DELETE ON games BEGIN
    INSERT INTO games_fts(games_fts, rowid, name) VALUES ('delete', old.appid, old.name);
END;

CREATE TRIGGER IF NOT EXISTS games_au AFTER UPDATE ON games BEGIN
    INSERT INTO games_fts(games_fts, rowid, name) VALUES ('delete', old.appid, old.name);
    INSERT INTO games_fts(rowid, name) VALUES (new.appid, new.name);
END;

CREATE VIRTUAL TABLE IF NOT EXISTS reports_fts USING fts5(
    title,
    notes,
    launch_options,
    content='reports',
    content_rowid='id'
);

CREATE TRIGGER IF NOT EXISTS reports_ai AFTER INSERT ON reports BEGIN
    INSERT INTO reports_fts(rowid, title, notes, launch_options) VALUES (new.id, new.title, new.notes, new.launch_options);
END;

CREATE TRIGGER IF NOT EXISTS reports_ad AFTER DELETE ON reports BEGIN
    INSERT INTO reports_fts(reports_fts, rowid, title, notes, launch_options) VALUES ('delete', old.id, old.title, old.notes, old.launch_options);
END;

CREATE TRIGGER IF NOT EXISTS reports_au AFTER UPDATE ON reports BEGIN
    INSERT INTO reports_fts(reports_fts, rowid, title, notes, launch_options) VALUES ('delete', old.id, old.title, old.notes, old.launch_options);
    INSERT INTO reports_fts(rowid, title, notes, launch_options) VALUES (new.id, new.title, new.notes, new.launch_options);
END;
