-- Migration 001: Initial PulsePort schema
-- UP

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);
INSERT INTO schema_version (version) VALUES (1);

CREATE TABLE app_config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

CREATE TABLE devices (
    id TEXT PRIMARY KEY,
    hostname TEXT NOT NULL,
    os_name TEXT NOT NULL,
    os_version TEXT NOT NULL,
    cpu_model TEXT,
    memory_bytes INTEGER,
    created_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    updated_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

CREATE TABLE metric_registry (
    metric_key TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    unit TEXT NOT NULL,
    source TEXT NOT NULL,
    category TEXT NOT NULL
);

CREATE TABLE metric_current (
    metric_key TEXT PRIMARY KEY,
    ts INTEGER NOT NULL,
    value_real REAL,
    unit TEXT NOT NULL,
    quality TEXT NOT NULL
);

CREATE TABLE metric_1m (
    bucket_ts INTEGER NOT NULL,
    metric_key TEXT NOT NULL,
    min_value REAL,
    max_value REAL,
    avg_value REAL,
    sample_count INTEGER NOT NULL,
    quality TEXT NOT NULL,
    PRIMARY KEY (bucket_ts, metric_key)
);

CREATE TABLE metric_15m (
    bucket_ts INTEGER NOT NULL,
    metric_key TEXT NOT NULL,
    min_value REAL,
    max_value REAL,
    avg_value REAL,
    sample_count INTEGER NOT NULL,
    quality TEXT NOT NULL,
    PRIMARY KEY (bucket_ts, metric_key)
);

CREATE TABLE energy_daily (
    day_local TEXT PRIMARY KEY,
    energy_wh REAL,
    avg_power_w REAL,
    peak_power_w REAL,
    charge_energy_wh REAL,
    discharge_energy_wh REAL,
    active_seconds INTEGER NOT NULL DEFAULT 0,
    quality TEXT NOT NULL DEFAULT 'unknown',
    finalized INTEGER NOT NULL DEFAULT 0,
    updated_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

CREATE TABLE events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_ts INTEGER NOT NULL,
    severity TEXT NOT NULL,
    category TEXT NOT NULL,
    title TEXT NOT NULL,
    payload_json TEXT
);

CREATE TABLE deletions_audit (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    scope TEXT NOT NULL,
    range_start TEXT,
    range_end TEXT,
    note TEXT
);

-- Indexes for time-range queries on high-volume tables
CREATE INDEX idx_metric_1m_key_time ON metric_1m(metric_key, bucket_ts);
CREATE INDEX idx_metric_15m_key_time ON metric_15m(metric_key, bucket_ts);
CREATE INDEX idx_events_time ON events(event_ts);
CREATE INDEX idx_events_category_time ON events(category, event_ts);

-- Seed default config
INSERT INTO app_config (key, value) VALUES ('port', '9770');
INSERT INTO app_config (key, value) VALUES ('log_level', 'info');
INSERT INTO app_config (key, value) VALUES ('retention_1m_days', '90');
INSERT INTO app_config (key, value) VALUES ('retention_15m_days', '365');
INSERT INTO app_config (key, value) VALUES ('retention_daily_days', '365');
INSERT INTO app_config (key, value) VALUES ('retention_events_days', '365');

-- DOWN
-- DROP TABLE IF EXISTS deletions_audit;
-- DROP TABLE IF EXISTS events;
-- DROP TABLE IF EXISTS energy_daily;
-- DROP TABLE IF EXISTS metric_15m;
-- DROP TABLE IF EXISTS metric_1m;
-- DROP TABLE IF EXISTS metric_current;
-- DROP TABLE IF EXISTS metric_registry;
-- DROP TABLE IF EXISTS devices;
-- DROP TABLE IF EXISTS app_config;
-- DROP TABLE IF EXISTS schema_version;
