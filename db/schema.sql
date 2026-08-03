-- pdfedit tracking database schema
-- SQLite + FTS5. Live file db/pdfedit.db is gitignored; this schema + seed are committed.

PRAGMA journal_mode = WAL;

-- Components: the units of work in the project
CREATE TABLE IF NOT EXISTS components (
    id          TEXT PRIMARY KEY,          -- kebab-case id, e.g. 'rtl-writer'
    name        TEXT NOT NULL,
    status      TEXT NOT NULL DEFAULT 'planned',  -- planned|in_progress|done|blocked|deferred
    owner_agent TEXT,                      -- role id from agents/roles/
    description TEXT,
    source_dir  TEXT,                      -- where the code lives (or will)
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Tasks: tracked work items
CREATE TABLE IF NOT EXISTS tasks (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    component_id  TEXT REFERENCES components(id),
    title         TEXT NOT NULL,
    status        TEXT NOT NULL DEFAULT 'open',   -- open|in_progress|done|blocked|cancelled
    priority      INTEGER NOT NULL DEFAULT 3,     -- 1 highest
    effort_estimate TEXT,                          -- e.g. '3-5d' or '8-14w'
    assignee      TEXT,
    notes         TEXT,
    evidence_ref  TEXT,                            -- commit sha / test name when done
    created_at    TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at    TEXT NOT NULL DEFAULT (datetime('now'))
);

-- ADR index: each decision gets a file in docs/decisions/ AND a row here
CREATE TABLE IF NOT EXISTS decisions (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    adr_file   TEXT NOT NULL,               -- docs/decisions/000X-name.md
    title      TEXT NOT NULL,
    status     TEXT NOT NULL DEFAULT 'proposed',  -- proposed|accepted|superseded|rejected
    summary    TEXT NOT NULL,
    made_at    TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Research notes: findings from Rosetta or web research
CREATE TABLE IF NOT EXISTS research (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    topic    TEXT NOT NULL,
    source   TEXT,                          -- url / brief file / 'rosetta'
    summary  TEXT,
    file     TEXT,                          -- docs/research/<name>.md if written up
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Open questions / decisions needed from the user or another agent
CREATE TABLE IF NOT EXISTS questions (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    question   TEXT NOT NULL,
    asked_to   TEXT NOT NULL DEFAULT 'user',   -- user|rosetta|yolka
    status     TEXT NOT NULL DEFAULT 'open',   -- open|answered|dropped
    answer     TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Skills pinned for agent roles (source: clawhub|github|local)
CREATE TABLE IF NOT EXISTS skills (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    name     TEXT NOT NULL UNIQUE,
    role     TEXT,                          -- agent role id
    source   TEXT NOT NULL,                 -- clawhub|github|local
    vetting  TEXT NOT NULL DEFAULT 'pending', -- pending|approved|rejected
    notes    TEXT,
    added_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Imported libraries/deps register: every third-party lib we import or link
CREATE TABLE IF NOT EXISTS deps (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,
    version     TEXT,
    license     TEXT,
    source      TEXT,                 -- github/vcpkg/apt/system
    purpose     TEXT,                 -- what we use it for
    vendored    INTEGER NOT NULL DEFAULT 0,   -- 1 = code copied into our tree
    tested      INTEGER NOT NULL DEFAULT 0,   -- 1 = we ran its tests / smoke-tested
    status      TEXT NOT NULL DEFAULT 'pending', -- pending|approved|rejected
    notes       TEXT,
    added_at    TEXT NOT NULL DEFAULT (datetime('now'))
);

-- FTS5 search index over the searchable free-text fields
CREATE VIRTUAL TABLE IF NOT EXISTS search_index USING fts5(
    kind,       -- 'task'|'component'|'decision'|'research'|'question'|'skill'
    ref_id,     -- primary key of the source row
    title,
    body
);

-- Triggers keep search_index in sync
CREATE TRIGGER IF NOT EXISTS trg_task_ai AFTER INSERT ON tasks BEGIN
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('task', new.id, new.title, coalesce(new.notes,''));
END;
CREATE TRIGGER IF NOT EXISTS trg_task_au AFTER UPDATE ON tasks BEGIN
    DELETE FROM search_index WHERE kind='task' AND ref_id=old.id;
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('task', new.id, new.title, coalesce(new.notes,''));
END;
CREATE TRIGGER IF NOT EXISTS trg_comp_ai AFTER INSERT ON components BEGIN
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('component', new.id, new.name, coalesce(new.description,''));
END;
CREATE TRIGGER IF NOT EXISTS trg_comp_au AFTER UPDATE ON components BEGIN
    DELETE FROM search_index WHERE kind='component' AND ref_id=old.id;
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('component', new.id, new.name, coalesce(new.description,''));
END;
CREATE TRIGGER IF NOT EXISTS trg_dec_ai AFTER INSERT ON decisions BEGIN
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('decision', new.id, new.title, new.summary);
END;
CREATE TRIGGER IF NOT EXISTS trg_res_ai AFTER INSERT ON research BEGIN
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('research', new.id, new.topic, coalesce(new.summary,''));
END;
CREATE TRIGGER IF NOT EXISTS trg_q_ai AFTER INSERT ON questions BEGIN
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('question', new.id, new.question, '');
END;
CREATE TRIGGER IF NOT EXISTS trg_skill_ai AFTER INSERT ON skills BEGIN
    INSERT INTO search_index(kind, ref_id, title, body)
    VALUES ('skill', new.id, new.name, coalesce(new.notes,''));
END;
