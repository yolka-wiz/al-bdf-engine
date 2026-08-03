#!/usr/bin/env python3
"""pdfedit tracking database CLI.

Searchable project DB (SQLite + FTS5) for an agent-written codebase.
Every task/component/decision/research-note/question/skill is tracked here.

Usage:
  db.py init                          create db/pdfedit.db from schema.sql
  db.py status [--component ID]       full status view
  db.py search TERM                   FTS5 search across all tracked items
  db.py task-add --component ID --title "..." [--priority N] [--estimate "3-5d"] [--assignee ROLE] [--notes "..."]
  db.py task-done ID --ref SHA        close task with evidence
  db.py task-open ID [--assignee ROLE]
  db.py task-block ID --why "..."
  db.py tasks [--open] [--component ID]
  db.py comp-add --id ID --name NAME [--status planned] [--owner ROLE] [--desc "..."] [--dir "..."]
  db.py comp-status ID STATUS
  db.py decision-add --file docs/decisions/000X-x.md --title "..." [--status proposed] --summary "..."
  db.py research-add --topic "..." [--source URL] [--summary "..."] [--file docs/research/x.md]
  db.py question-add --question "..." [--asked-to user]
  db.py question-answer ID --answer "..."
  db.py questions [--open]
  db.py skill-add --name NAME --role ROLE --source github [--notes "..."]
  db.py skill-vet ID approved|rejected
  db.py dump                      raw sqlite dump (for backup/portability)

Exit codes: 0 ok, 1 usage/error.
"""

import argparse
import os
import sqlite3
import sys

DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "db", "pdfedit.db")
SCHEMA_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "db", "schema.sql")

STATUS_COLOR = {
    "done": "\033[32m", "open": "\033[33m", "in_progress": "\033[36m",
    "planned": "\033[34m", "blocked": "\033[31m", "cancelled": "\033[31m",
    "deferred": "\033[35m", "proposed": "\033[34m", "accepted": "\033[32m",
    "answered": "\033[32m", "approved": "\033[32m",
}
RESET = "\033[0m"


def conn():
    if not os.path.exists(DB_PATH):
        print(f"DB not found at {DB_PATH}. Run 'db.py init' first.", file=sys.stderr)
        sys.exit(1)
    c = sqlite3.connect(DB_PATH)
    c.row_factory = sqlite3.Row
    return c


def cstatus(s):
    return f"{STATUS_COLOR.get(s, '')}{s}{RESET}"


def cmd_init(args):
    os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
    c = sqlite3.connect(DB_PATH)
    with open(SCHEMA_PATH) as f:
        c.executescript(f.read())
    c.commit()
    print(f"Initialized {os.path.relpath(DB_PATH)}")


def cmd_status(args):
    c = conn()
    print("== Components ==")
    rows = c.execute("SELECT * FROM components ORDER BY id").fetchall()
    if not rows:
        print("  (none)")
    for r in rows:
        print(f"  {cstatus(r['status']):<12} {r['id']:<22} {r['name']}")
        if r["owner_agent"]:
            print(f"                owner: {r['owner_agent']}  dir: {r['source_dir'] or '-'}")
    print("\n== Tasks ==")
    q = "SELECT * FROM tasks"
    if args.component:
        q += " WHERE component_id = ?"
        rows = c.execute(q, (args.component,)).fetchall()
    else:
        rows = c.execute(q).fetchall()
    if not rows:
        print("  (none)")
    for r in rows:
        print(f"  #{r['id']:<4} {cstatus(r['status']):<12} [{r['component_id'] or '-'}] {r['title']}")
        if r["evidence_ref"]:
            print(f"        evidence: {r['evidence_ref']}")
    print("\n== Open decisions & questions ==")
    for r in c.execute("SELECT id, status, title FROM decisions WHERE status IN ('proposed','accepted')").fetchall():
        print(f"  [ADR {r['id']}] {cstatus(r['status'])} {r['title']}")
    for r in c.execute("SELECT id, status, question FROM questions WHERE status='open'").fetchall():
        print(f"  [Q {r['id']}] {cstatus(r['status'])} {r['question']}")


def cmd_search(args):
    c = conn()
    term = args.term
    rows = c.execute(
        "SELECT kind, ref_id, title, body FROM search_index WHERE search_index MATCH ? LIMIT 50",
        (term,),
    ).fetchall()
    if not rows:
        print(f"No matches for '{term}'")
        return
    for r in rows:
        print(f"[{r['kind']}:{r['ref_id']}] {r['title']}")
        if r["body"]:
            print(f"    {r['body'][:200]}")


def cmd_task_add(args):
    c = conn()
    cur = c.execute(
        "INSERT INTO tasks(component_id, title, priority, effort_estimate, assignee, notes, status) "
        "VALUES (?,?,?,?,?,?, 'open')",
        (args.component, args.title, args.priority, args.estimate, args.assignee, args.notes),
    )
    c.commit()
    print(f"task #{cur.lastrowid} added: {args.title}")


def _task_update(id, fields):
    c = conn()
    sets = ", ".join(f"{k} = ?" for k in fields)
    vals = list(fields.values()) + [id]
    c.execute(f"UPDATE tasks SET {sets}, updated_at=datetime('now') WHERE id=?", vals)
    c.commit()


def cmd_task_done(args):
    _task_update(args.id, {"status": "done", "evidence_ref": args.ref})
    print(f"task #{args.id} marked done (ref {args.ref})")


def cmd_task_open(args):
    fields = {"status": "in_progress"}
    if args.assignee:
        fields["assignee"] = args.assignee
    _task_update(args.id, fields)
    print(f"task #{args.id} in_progress" + (f" -> {args.assignee}" if args.assignee else ""))


def cmd_task_block(args):
    _task_update(args.id, {"status": "blocked", "notes": args.why})
    print(f"task #{args.id} blocked: {args.why}")


def cmd_tasks(args):
    c = conn()
    q = "SELECT * FROM tasks"
    where, params = [], []
    if args.open:
        where.append("status IN ('open','in_progress')")
    if args.component:
        where.append("component_id = ?")
        params.append(args.component)
    if where:
        q += " WHERE " + " AND ".join(where)
    q += " ORDER BY priority, id"
    for r in c.execute(q, params).fetchall():
        print(f"#{r['id']:<4} {cstatus(r['status']):<12} p{r['priority']} "
              f"[{r['component_id'] or '-'}] {r['title']} "
              f"({r['effort_estimate'] or '?'} / {r['assignee'] or 'unassigned'})")


def cmd_comp_add(args):
    c = conn()
    c.execute(
        "INSERT INTO components(id, name, status, owner_agent, description, source_dir) VALUES (?,?,?,?,?,?)",
        (args.id, args.name, args.status, args.owner, args.desc, args.dir),
    )
    c.commit()
    print(f"component '{args.id}' added")


def cmd_comp_status(args):
    c = conn()
    c.execute("UPDATE components SET status=? WHERE id=?", (args.status, args.id))
    c.commit()
    print(f"component '{args.id}' -> {args.status}")


def cmd_decision_add(args):
    c = conn()
    cur = c.execute(
        "INSERT INTO decisions(adr_file, title, status, summary) VALUES (?,?,?,?)",
        (args.file, args.title, args.status, args.summary),
    )
    c.commit()
    print(f"ADR #{cur.lastrowid} added -> {args.file}")


def cmd_research_add(args):
    c = conn()
    cur = c.execute(
        "INSERT INTO research(topic, source, summary, file) VALUES (?,?,?,?)",
        (args.topic, args.source, args.summary, args.file),
    )
    c.commit()
    print(f"research note #{cur.lastrowid} added")


def cmd_question_add(args):
    c = conn()
    cur = c.execute(
        "INSERT INTO questions(question, asked_to, status) VALUES (?,?,'open')",
        (args.question, args.asked_to),
    )
    c.commit()
    print(f"question #{cur.lastrowid} added (asked to {args.asked_to})")


def cmd_question_answer(args):
    c = conn()
    c.execute("UPDATE questions SET status='answered', answer=? WHERE id=?", (args.answer, args.id))
    c.commit()
    print(f"question #{args.id} answered")


def cmd_questions(args):
    c = conn()
    q = "SELECT * FROM questions"
    if args.open:
        q += " WHERE status='open'"
    q += " ORDER BY id"
    for r in c.execute(q).fetchall():
        print(f"Q{r['id']} [{r['asked_to']}] {cstatus(r['status'])}: {r['question']}")
        if r["answer"]:
            print(f"    -> {r['answer']}")


def cmd_skill_add(args):
    c = conn()
    try:
        cur = c.execute(
            "INSERT INTO skills(name, role, source, vetting, notes) VALUES (?,?,?,'pending',?)",
            (args.name, args.role, args.source, args.notes),
        )
        c.commit()
        print(f"skill '{args.name}' added (vetting pending)")
    except sqlite3.IntegrityError:
        print(f"skill '{args.name}' already exists", file=sys.stderr)
        sys.exit(1)


def cmd_skill_vet(args):
    c = conn()
    c.execute("UPDATE skills SET vetting=? WHERE id=?", (args.status, args.id))
    c.commit()
    print(f"skill #{args.id} vetting -> {args.status}")


def cmd_dep_add(args):
    c = conn()
    try:
        cur = c.execute(
            "INSERT INTO deps(name, version, license, source, purpose, vendored, tested, status, notes) "
            "VALUES (?,?,?,?,?,?,?,?,?)",
            (args.name, args.version, args.license, args.source, args.purpose,
             int(args.vendored), int(args.tested), args.status, args.notes),
        )
        c.commit()
        print(f"dep '{args.name}' added (id {cur.lastrowid})")
    except sqlite3.IntegrityError:
        print(f"dep '{args.name}' already exists", file=sys.stderr)
        sys.exit(1)


def cmd_dep_status(args):
    c = conn()
    fields = {"status": args.status}
    if args.tested is not None:
        fields["tested"] = int(args.tested)
    sets = ", ".join(f"{k} = ?" for k in fields)
    vals = list(fields.values()) + [args.id]
    c.execute(f"UPDATE deps SET {sets} WHERE id=?", vals)
    c.commit()
    print(f"dep #{args.id} -> status={args.status}")


def cmd_deps(args):
    c = conn()
    q = "SELECT * FROM deps"
    if args.only:
        q += " WHERE status = ?"
        rows = c.execute(q, (args.only,)).fetchall()
    else:
        rows = c.execute(q).fetchall()
    if not rows:
        print("(no deps registered)")
        return
    for r in rows:
        v = "V" if r["vendored"] else " "
        t = "T" if r["tested"] else " "
        print(f"#{r['id']:<3} [{v}{t}] {r['name']:<16} {r['version'] or '-':<12} "
              f"{r['license'] or '-':<12} {cstatus(r['status']):<10} {r['purpose'] or ''}")


def cmd_dump(args):
    os.system(f"sqlite3 '{DB_PATH}' .dump")


def main():
    p = argparse.ArgumentParser(description="pdfedit tracking DB")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("init")
    sp = sub.add_parser("status"); sp.add_argument("--component")
    sp = sub.add_parser("search"); sp.add_argument("term")

    sp = sub.add_parser("task-add")
    sp.add_argument("--component", required=True); sp.add_argument("--title", required=True)
    sp.add_argument("--priority", type=int, default=3); sp.add_argument("--estimate")
    sp.add_argument("--assignee"); sp.add_argument("--notes")
    sp = sub.add_parser("task-done"); sp.add_argument("id", type=int); sp.add_argument("--ref", required=True)
    sp = sub.add_parser("task-open"); sp.add_argument("id", type=int); sp.add_argument("--assignee")
    sp = sub.add_parser("task-block"); sp.add_argument("id", type=int); sp.add_argument("--why", required=True)
    sp = sub.add_parser("tasks"); sp.add_argument("--open", action="store_true"); sp.add_argument("--component")

    sp = sub.add_parser("comp-add")
    sp.add_argument("--id", required=True); sp.add_argument("--name", required=True)
    sp.add_argument("--status", default="planned"); sp.add_argument("--owner")
    sp.add_argument("--desc"); sp.add_argument("--dir")
    sp = sub.add_parser("comp-status"); sp.add_argument("id"); sp.add_argument("status")

    sp = sub.add_parser("decision-add")
    sp.add_argument("--file", required=True); sp.add_argument("--title", required=True)
    sp.add_argument("--status", default="proposed"); sp.add_argument("--summary", required=True)
    sp = sub.add_parser("research-add")
    sp.add_argument("--topic", required=True); sp.add_argument("--source"); sp.add_argument("--summary"); sp.add_argument("--file")

    sp = sub.add_parser("question-add"); sp.add_argument("--question", required=True); sp.add_argument("--asked-to", default="user")
    sp = sub.add_parser("question-answer"); sp.add_argument("id", type=int); sp.add_argument("--answer", required=True)
    sp = sub.add_parser("questions"); sp.add_argument("--open", action="store_true")

    sp = sub.add_parser("skill-add"); sp.add_argument("--name", required=True); sp.add_argument("--role", required=True)
    sp.add_argument("--source", required=True, choices=["clawhub", "github", "local"]); sp.add_argument("--notes")
    sp = sub.add_parser("skill-vet"); sp.add_argument("id", type=int); sp.add_argument("status", choices=["approved", "rejected"])

    sp = sub.add_parser("dep-add")
    sp.add_argument("--name", required=True); sp.add_argument("--version"); sp.add_argument("--license")
    sp.add_argument("--source", default="github"); sp.add_argument("--purpose")
    sp.add_argument("--vendored", action="store_true"); sp.add_argument("--tested", action="store_true")
    sp.add_argument("--status", default="pending"); sp.add_argument("--notes")
    sp = sub.add_parser("dep-status"); sp.add_argument("id", type=int); sp.add_argument("status", choices=["pending", "approved", "rejected"])
    sp.add_argument("--tested", type=int, choices=[0, 1])
    sp = sub.add_parser("deps"); sp.add_argument("--only", choices=["pending", "approved", "rejected"])

    sub.add_parser("dump")

    args = p.parse_args()
    globals()[f"cmd_{args.cmd.replace('-', '_')}"](args)


if __name__ == "__main__":
    main()
