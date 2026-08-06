# 003 — Skills Sourcing & AGENTS.md Conventions

- **Date:** 2026-08-04
- **Author:** research-agent (Rosetta brief #003)
- **Status:** draft — findings for orchestrator review; no skills were installed by this brief
- **Method:** GitHub REST API (repo + search + trees), raw.githubusercontent.com fetches, ClawHub REST API (`/api/search` works; content endpoint 404), awesome-list README greps, local `skill-vetter` protocol (red flags: credential reads, exfiltration URLs, obfuscation, permission scope).
- **Tagging:** `[V]` = verified by direct API/raw fetch; `[I]` = inferred or not fully verifiable.

---

# PART A — Skill sourcing & vetting

## A.0 Trust tiers applied (from skill-vetter)

1. Official orgs (Qt Company, Anthropic, OpenAI, Trail of Bits, NVIDIA) → full review but lower suspicion
2. Known authors / high-star → moderate scrutiny
3. New/unknown sources → maximum scrutiny
4. Anything touching credentials/secrets → human approval, never auto-install

## A.1 Tier 1 — RECOMMENDED

### 1. TheQtCompanyRnD/agent-skills — official Qt skills `[V]`
- URL: https://github.com/TheQtCompanyRnD/agent-skills | author: The Qt Company (official org)
- Stars: 343 | License: **BSD-3-Clause** (LICENSE file: `LicenseRef-Qt-Commercial OR BSD-3-Clause` — API reports "NOASSERTION" because of the dual header; BSD-3 path is fine for us) | Last push: 2026-07-09 (active) | Size: 514 KB, 188 files
- **Risk: 🟢 LOW** — official vendor org; tree contains only SKILL.md/README/references plus one lint script (`qt-cpp-review/references/lint-scripts/qt_review_lint.py`); no network calls, no credential access, no obfuscation in tree scan.
- Skills inside: `qt-cmake-project`, `qt-cpp-docs`, `qt-cpp-review` (with review checklist, deprecated-API ref, lint script), `qt-qml-docs`, `qt-qml-review`, `qt-qml-profiler`, `qt-figma-*` (design-token/QML generation — irrelevant, v1 has no GUI).
- **Already vendored** in this repo: `skills/{qt-cmake-project,qt-cpp-docs,qt-cpp-review}` (commit 9f93a90). ✔
- **Roles:** core-agent (qt-cpp-review for review passes; qt-cmake-project for build hygiene), cli-agent (qt-cmake-project), rtl-agent (qt-cpp-docs for QtCore API lookup), test-agent (qt-cpp-review lint gate). Skip qt-qml-* until a GUI milestone exists (AGENTS.md §1 bans GUI).

### 2. trailofbits/skills — security/QA skills marketplace (highest-value find) `[V]`
- URL: https://github.com/trailofbits/skills | author: Trail of Bits (reputable security firm)
- Stars: 6,418 | License: **CC-BY-SA-4.0** | Active (pushed 2026-07+), 1,045 files, Claude Code plugin marketplace (also Codex/Gemini-compatible)
- **Risk: 🟡 MEDIUM** — license is share-alike (fine for vendored *documentation/skills* with attribution + SA notice; do not copy code into `src/`); content is high-quality security engineering. No red-flag patterns observed in tree (prompt/checklist based skills; the repo even publishes `trailofbits/overtly-malicious-skills` as a scanner test corpus — good sign of security hygiene).
- **Relevant plugins:**
  - `testing-handbook-skills` → skills: **`libfuzzer`**, **`ossfuzz`**, `cargo-fuzz`, `fuzzing-dictionary`, `fuzzing-obstacles`, `testing-handbook-generator` — exactly our fuzzing stack (libFuzzer + OSS-Fuzz for a C++ PDF parser).
  - **`c-review`** → structured **multi-agent C/C++ code review**: worker + dedup-judge + false-positive-judge subagents, ~30 review clusters (`cpp-semantics`, `undefined-behavior`, `integer-overflow`, `memory-leak`, `race-condition`, `iterator-invalidation`, `exception-safety`, `move-semantics`, `smart-pointer`, `object-lifecycle`, …). Directly matches our "structured multi-agent code review" requirement.
  - Also: `static-analysis`, `property-based-testing`, `mutation-testing`, `differential-review`, `second-opinion`, `spec-to-code-compliance`, `constant-time-analysis`, `sharp-edges`.
- **Roles:** test-agent (libfuzzer/ossfuzz/fuzzing-dictionary — fuzz the parser per coding-standard §5 "PDFs are hostile input"), orchestrator + core-agent (c-review for review passes), research-agent (methodology).

### 3. anthropics/skills → `skills/pdf` — official Anthropic PDF skill `[V]`
- URL: https://github.com/anthropics/skills/tree/main/skills/pdf | Stars: 166,026 | Active
- License: **Proprietary per skill** — `SKILL.md` frontmatter says `license: Proprietary. LICENSE.txt has complete terms` `[V]` → **do not vendor into the repo**; use from the agent library for research/tooling.
- **Risk: 🟡 MEDIUM** (license constraint, not code risk; official org, Python/CLI tooling around pypdf/reportlab-class tools).
- Use: research-agent (PDF spec workflows, corpus generation, form/OCR tooling), test-agent (fixture generation ideas). Not for core library work — we *are* the PDF library.

### 4. openai/skills → `skills/.curated/pdf` — OpenAI PDF skill (Codex catalog) `[V]`
- URL: https://github.com/openai/skills | Stars: 24,464 | Active
- License: **Apache-2.0** per-skill (`skills/.curated/pdf/LICENSE.txt` verified) `[V]` → vendorable with attribution.
- **Risk: 🟢 LOW** (official org, permissive license). Same role mapping as #3. (Catalog also has `.curated` skills for aspnet-core, cloudflare-deploy, etc. — mostly irrelevant.)

### 5. coderabbitai/skills — CodeRabbit code-review + autofix `[V]`
- URL: https://github.com/coderabbitai/skills | Stars: 145 | License: **MIT** | Active
- Contents: `skills/code-review/SKILL.md` (severity-grouped findings), `skills/autofix/SKILL.md`, `agents/`, commands.
- **Risk: 🟡 MEDIUM** — review runs through the CodeRabbit **cloud service** (diffs leave the machine). Acceptable for an OSS repo; unsuitable for private code.
- Role: orchestrator's review pass. For fully offline review prefer trailofbits `c-review` or an in-house review checklist (local Hermes already has `github-code-review` / `requesting-code-review` / `security-auditor` skills).

## A.2 Tier 2 — conditional / reference only

### 6. rollysys/cpp-perf-skills — C++ performance optimization `[V]`
- URL: https://github.com/rollysys/cpp-perf-skills | Stars: 12 | **License: NONE — cannot vendor** | Stale (2026-03) | 24 MB (mostly ARM reference PDFs: Cortex-A78 optimization guides, perf-book)
- Vet: `SKILL.md` is a legitimate ARM/X86 perf workflow (NEON/SIMD, cache behavior, benchmarking); design doc mentions SSH to *user-specified* target boards (`~/.ssh/id_rsa`, `192.168.1.100`) — functional remote profiling, not exfiltration; no obfuscation. `model.eval()`-style false positives: none here; README hits were plain URLs.
- **Risk: 🟡 MEDIUM** (no license + SSH-key usage). Verdict: reference reading for core-agent perf work; do not install into the library.

### 7. maystudios/claude-skills `[V]`
- URL: https://github.com/maystudios/claude-skills | Stars: 15 | MIT | UE5 + llama.cpp + OpenCode skills.
- Vet: `eval(` in `2d-pixel-asset/scripts/process_asset.py` is PyTorch `model.eval()` — **false positive**; `audio-to-midi/scripts/transcribe.py` uses subprocess + URLs (model download). No credential access.
- **Risk: 🟢 LOW.** Verdict: **skip** — Unreal/llama.cpp scope doesn't map to our roles.

### 8. jftuga/transcript-critic `[V]`
- URL: https://github.com/jftuga/transcript-critic | Stars: 32 | MIT | Small (21 KB), 8 files.
- whisper.cpp audio/video transcription + structured critical analysis. Vet: URLs in scripts are tooling references (whisper/yt-dlp), no exfiltration.
- **Risk: 🟢 LOW.** Verdict: **skip** — audio transcription is out of scope for every role (kept from parent's candidate list for completeness).

### 9. alirezarezvani/claude-skills — mega-pack (345+ skills) `[V]`
- URL: https://github.com/alirezarezvani/claude-skills | Stars: 23,735 | MIT | **4,721 files** (mirrored into `.codex/`, `.gemini/`, `.hermes/`, `.vibe/` trees)
- **Risk: 🔴 HIGH as a pack** — contains `env-secrets-manager` and `secrets-vault-manager` skills (credential-adjacent → red flag per skill-vetter; requires human approval even for cherry-picks). Also contains useful-looking `code-reviewer`, `adversarial-reviewer`, `api-design-reviewer`.
- Verdict: **do NOT wholesale install**; per-skill cherry-pick only, each run through the full vetting checklist, secrets skills excluded.

### 10. jthack/ffuf_claude_skill `[V]`
- URL: https://github.com/jthack/ffuf_claude_skill | Stars: 204 | No license | Stale (2025-10)
- Web-fuzzing only (ffuf) — not our C++ fuzzing domain. **Skip.**

### 11. PSPDFKit-labs/nutrient-agent-skill `[V]`
- URL: https://github.com/PSPDFKit-labs/nutrient-agent-skill | Stars: 15 | No license
- ⚠️ **Uploads documents to the Nutrient DWS cloud API** (data-exfiltration surface by design). **Skip** for code work; note as `[I]` option if we ever need doc-conversion convenience.

### 12. ClawHub (clawhub.ai) `[I]`
- Browsable via REST: `/api/search?q=<term>` works (SPA for humans; per-skill content endpoint 404 — content must be verified in a browser or via GitHub mirror before install).
- Searches: `qt` → no C++/Qt skills of value (winforms-to-qt-mapper is C#→Qt migration); `fuzz` → nothing (only fuzzy-string tools); `clang` → 0; `harfbuzz` → 0; `pdf` → `awspace/pdf` (47,319 dl — mirror of anthropics/pdf), `yang1002378395-cmyk/pdf-processor-cn` (3,830 dl), `paudyyin/pdf`; `c++` → `ivangdavila/cpp` "avoid common C++ mistakes" (2,287 dl); `code review` → `wpank/code-review` (17,835 dl), `caingao/smart-code-review`, `theshadowrose/code-review-sr`.
- Verdict: nothing that beats Tier 1. Optional deep-dive later: `ivangdavila/cpp` (C++ pitfalls — likely overlaps qt-cpp-review) and `wpank/code-review` (systematic review patterns) — content unverified `[I]`.

## A.3 PDF-specific skills (from awesome lists) `[V]` unless noted

From ComposioHQ/awesome-claude-skills, VoltAgent/awesome-agent-skills, travisvn/awesome-claude-skills (README greps):

| Skill | Source | License | Note |
|---|---|---|---|
| anthropics/pdf | anthropics/skills | Proprietary | canonical PDF toolkit (text/tables/merge/forms/OCR) — research-agent |
| openai/pdf | openai/skills | Apache-2.0 | read/create/review PDFs |
| MiniMax-AI/minimax-pdf `[I]` | GitHub | — | token-based design system, 15 cover styles — doc-gen, not core |
| lovstudio/any2pdf | GitHub | MIT | markdown→PDF typesetting |
| deusyu/translate-book | GitHub | MIT | book translation via subagents |
| PSPDFKit nutrient | GitHub | none | ⚠️ cloud upload — skip |
| awspace/pdf (ClawHub) | ClawHub | — | 47k dl mirror of anthropics/pdf |

All are *document-processing tooling* (Python/CLI) → useful to research-agent for corpus/tooling work, **not** for core-library development (we *are* the PDF library).

## A.4 Gaps — no existing skills found → author in-house

1. **HarfBuzz / text shaping: ZERO skills found** (GitHub search + ClawHub). Highest-need gap: rtl-agent owns the FriBidi→HarfBuzz→ToUnicode pipeline (role file, ADR-0003). → Author a vendored doc-skill `skills/harfBuzz-shaping/` (HB API refs, `HB_DIRECTION_RTL`, `hb_font_funcs`/cluster mapping, subset-GID renumbering pitfalls, lam-alef, `/ActualText`). `[I]` recommendation.
2. **Fuzzing:** covered by trailofbits `testing-handbook-skills` (libfuzzer, ossfuzz, fuzzing-dictionary) — install that; optionally add a small repo-specific skill for harness/corpus integration with `ctest` + ASAN/UBSAN (test-agent's "fuzz-ish malformed-PDF smoke" becomes real fuzzing). `[I]`
3. **clang tooling:** nothing found. coding-standard §3 only gates `clang-format`; add `clang-tidy` / `clang --analyze` gate (CI or in-house skill). `[I]`
4. **Multi-agent code review:** trailofbits `c-review` (offline) + coderabbitai (cloud) + existing local Hermes skills (`github-code-review`, `requesting-code-review`, `security-auditor`) + NeoLabHQ/context-engineering-kit `subagent-driven-development` `[I]` (dispatch pattern for the orchestrator).

## A.5 Vetting summary

| # | Repo | Stars | License | Pushed | Risk | Roles | Verdict |
|---|---|---|---|---|---|---|---|
| 1 | TheQtCompanyRnD/agent-skills | 343 | BSD-3-Clause `[V]` | 2026-07 | 🟢 LOW | core, cli, rtl, test | ✅ vendored already; keep |
| 2 | trailofbits/skills | 6,418 | CC-BY-SA-4.0 | active | 🟡 MED | test, core, orch | ✅ install (libfuzzer/ossfuzz + c-review) |
| 3 | anthropics/skills (pdf) | 166,026 | Proprietary `[V]` | 2026-07 | 🟡 MED | research | ⚠️ library-only, don't vendor |
| 4 | openai/skills (pdf) | 24,464 | Apache-2.0 `[V]` | 2026-07 | 🟢 LOW | research | ✅ install w/ attribution |
| 5 | coderabbitai/skills | 145 | MIT | 2026-07 | 🟡 MED | orch | ⚠️ cloud service; optional |
| 6 | rollysys/cpp-perf-skills | 12 | **none** | 2026-03 | 🟡 MED | core | ❌ no license; reference-only |
| 7 | maystudios/claude-skills | 15 | MIT | 2026-07 | 🟢 LOW | — | ❌ skip (UE5/llama scope) |
| 8 | jftuga/transcript-critic | 32 | MIT | 2026-04 | 🟢 LOW | — | ❌ skip (audio) |
| 9 | alirezarezvani/claude-skills | 23,735 | MIT | 2026-07 | 🔴 HIGH | — | ⛔ no wholesale install; cherry-pick only |
| 10 | jthack/ffuf_claude_skill | 204 | none | 2025-10 | 🟡 MED | — | ❌ web fuzzing only |
| 11 | PSPDFKit nutrient | 15 | none | 2026-03 | 🔴 HIGH | — | ❌ cloud upload |
| 12 | ClawHub items | varies | varies | varies | 🟡 | research/orch | ⚠️ verify content in browser `[I]` |

## A.6 Install plan (for orchestrator)

1. `trailofbits/skills` → vendor `testing-handbook-skills` (libfuzzer, ossfuzz, fuzzing-dictionary) + `c-review` under `skills/` with `LICENSE`/attribution (CC-BY-SA-4.0 notice); install same skills into the local Hermes skill library (skill_manage) pinned to test-agent/orchestrator.
2. `openai/skills` pdf → vendor with Apache-2.0 notice (research-agent); `anthropics/skills` pdf → agent-library only (proprietary).
3. Author in-house: `skills/harfBuzz-shaping/` (rtl-agent), fuzz-harness skill (test-agent), clang-tidy gate (test-agent/core-agent).
4. Record per-skill installs in the DB `skills` table (roles README already references it).

---

# PART B — AGENTS.md conventions from notable OSS projects

## B.1 Real examples collected (all fetched from `raw.githubusercontent.com`, default branch — `[V]`)

| Repo | Size | Sections | Notable binding rules |
|---|---|---|---|
| **vllm-project/vllm** https://github.com/vllm-project/vllm/blob/main/AGENTS.md | 5.6 KB | Contribution Policy (Mandatory): duplicate-work checks, no low-value busywork PRs, accountability, **fail-closed**; Development Workflow: env setup, tests, linters, coding style, commit messages; Domain guides | Pure code-agent PRs **not allowed**; human must review every line + run relevant tests; PR description for AI work **must** state it; never `system python3`/bare `pip` — `uv` only; pre-commit always |
| **ray-project/ray** https://github.com/ray-project/ray/blob/master/AGENTS.md | 3.7 KB | Same template family as vLLM (Anyscale) | PRs ignoring policy may be **closed without review**; **every commit signed off** (`-s`); fail-closed |
| **openai/openai-agents-python** https://github.com/openai/openai-agents-python/blob/main/AGENTS.md | 24 KB | Policies & Mandatory Rules (**mandatory skill usage**: `$code-change-verification`, `$openai-knowledge`, `$implementation-strategy`, `$implementation-final-review`, `$pr-draft-summary`; git worktree & branch safety; **scope discipline & complexity reset**; ExecPlans; public API compatibility; platform/docs/security review); Project Structure; Operation Guide (prereqs, dev workflow, **mandatory local run order**, utilities, PR & commit guidelines); **Code Review Rules** | Before final response: run the full mandatory order; every abstraction must map to a stated requirement; treat 2nd related review finding as a complexity signal; PR draft block required at handoff |
| **grafana/grafana** https://github.com/grafana/grafana/blob/main/AGENTS.md | 9.6 KB | Project Overview; Principles; Comments policy; **Human Review Gates**; Commands (build/test/lint/codegen); Architecture; Key notes; tool-specific instructions | **Versioned** (`<!-- version: 2.0.0 -->`); **directory-scoped AGENTS.md files** (`docs/AGENTS.md`, alerting squad); only "why" comments; no links in comments |
| **langchain-ai/langchain** https://github.com/langchain-ai/langchain/blob/main/AGENTS.md | 18.6 KB | Architecture/context (monorepo); dev tools & commands; env/dependency mgmt; config files; PR/commit titles; Core principles: stable public interfaces, code quality, **testing requirements**, **security & risk assessment**, docs standards; CI/CD | Conventional Commits with **mandatory scope**; preserve public signatures; **all Python MUST have type hints**; **every feature/bugfix MUST have unit tests**; unit tests never make network calls; deps only via `uv sync`, justified |
| **astral-sh/uv** https://github.com/astral-sh/uv/blob/main/AGENTS.md | 1.5 KB | Terse rule list only (no narrative) | ALWAYS test-style parity + coverage check; **PREFER integration tests over unit**; PREFER `insta` snapshots; NEVER release-profile builds unless asked; ALWAYS `SAFETY` comments for `unsafe`; **NEVER assume failures are pre-existing**; NEVER bulk-update lockfile |
| stubs: pytorch/pytorch AGENTS.md → points to CLAUDE.md; microsoft/TypeScript AGENTS.md → maintenance-mode warning | | | |

## B.2 Section synthesis (what real AGENTS.md files contain)

1. **Mandatory policy gate** (vLLM, Ray, OpenAI) — contribution rules with enforcement teeth ("may be closed without review", "automatic banning")
2. **Human review gates / accountability** (vLLM, Ray, Grafana) — no pure-agent PRs; human reviews every line; review gates before merge
3. **Commands** (all) — exact build/test/lint/format commands, often per subsystem
4. **Testing rules + mandatory run order** (OpenAI, vLLM, LangChain, uv)
5. **Code review rules** (OpenAI, Grafana) — separate review section with baseline expectations
6. **Scope discipline** (OpenAI) — narrowest behavior, one source of truth per concern
7. **Git/commit rules** (Ray sign-off, LangChain scope-mandatory Conventional Commits, no-wip)
8. **Fail-closed behavior** (vLLM, Ray) — when verification is impossible, stop and report; never claim success
9. **Architecture map** (Grafana, LangChain) — repo layout so agents don't guess
10. **Structure**: versioning + directory-scoped files (Grafana) / role-scoped files (repo `agents/roles/`)

## B.3 Binding-rule patterns ("the teeth")

- **Determinism of process**: mandatory local run order before any final response (OpenAI); pre-commit hooks always (vLLM); test-style parity (uv).
- **Evidence**: PR description must state AI-assisted work and tests run (vLLM/Ray); PR draft block at handoff (OpenAI); task closure gated on evidence (our DB `--ref` — already stronger than most).
- **Testing**: every feature MUST have tests (LangChain); prefer integration tests (uv); never assume pre-existing failures (uv).
- **Commits**: `-s` sign-off (Ray); Conventional Commits with mandatory scope (LangChain); explain WHY (LangChain PR guidelines).
- **Fail-closed**: "if you can't verify, don't claim" (vLLM/Ray) — same spirit as our §2.7.
- **Review**: dedicated Code Review Rules section + mandatory review skills (OpenAI); Human Review Gates (Grafana).
- **Skill pinning**: mandatory skill usage for defined work types (OpenAI) — skills as enforced procedure, not optional hints.

## B.4 Assessment of `/workspace/albdf/AGENTS.md`

**Already strong (vs. the examples):**
- Determinism-first output rule (§2.1) — exceeds most examples
- Evidence-gated task closure via tracking DB with `--ref` (§3) — exceeds vLLM/Ray PR-evidence rules
- "Never fake results" (§2.7) — matches vLLM/Ray fail-closed
- Small commits + Conventional Commits + no-wip (§2.5, §6) — matches LangChain/Ray
- Headless/test-driven/ADRs — good project-specific teeth

**Weaknesses & missing (each mapped to an example):**

| # | Gap | Reference example |
|---|---|---|
| W1 | **No Definition-of-Done checklist** (build → format gate → unit → golden → DB evidence → commit) as a single mandatory sequence | OpenAI "mandatory local run order"; vLLM workflow |
| W2 | **No mandatory review pass before merge** — with 3 parallel agents, no cross-agent review is mandated; no "review gate" language | Grafana Human Review Gates; OpenAI Code Review Rules |
| W3 | **No fail-closed clause for ambiguous tasks** — §7 says "ask", doesn't forbid guessing/partial completion | vLLM/Ray "fail-closed behavior" |
| W4 | **No parallel-work collision discipline** — max 3 agents but no merge-base/`pull --rebase`-before-commit rule, no per-area ownership table | OpenAI worktree/branch safety; LangChain branch naming |
| W5 | **No environment pins / reproducible env block** (compiler, Qt 6.8, CMake, presets) — versions live in coding-standard but not in AGENTS.md commands | OpenAI Prerequisites; LangChain uv env mgmt |
| W6 | **No commit sign-off / evidence-in-PR-body requirement** | Ray `-s`; vLLM PR description rules |
| W7 | **No ASAN/UBSAN + clang-tidy gates in §4 Build & test** (test-agent role plans ASAN CI — promote to binding text) | OpenAI run order; LangChain testing requirements |
| W8 | **No binding-language hierarchy** (MUST/ALWAYS vs PREFER vs NEVER) — all rules read equally | uv's terse PREFER/ALWAYS/NEVER list |
| W9 | **AGENTS.md doesn't reference `agents/roles/*.md`** — role contracts exist but aren't wired into the binding text | Grafana directory-scoped files |
| W10 | **No skill-usage policy** — which skills are mandatory per role (vendored qt skills, future fuzzing skills) | OpenAI mandatory skill usage |
| W11 | **No AGENTS.md self-versioning / change policy** | Grafana version comment |
| W12 | **No coverage/quality targets** (e.g., "every feature lands with tests; golden diffs reviewed") — partially present; make numeric/explicit | LangChain "every feature MUST be covered by unit tests" |

## B.5 Concrete recommended additions (draft, ready to paste)

1. **§0 preamble**: add `<!-- version: 0.2.0 -->` and "Re-read this file and your `agents/roles/<role>.md` at session start; if a rule here changes, update this file, not your habits."
2. **New §8 — Definition of Done (all boxes required before `db.py task-done`):**
   - [ ] `clang-format --dry-run --Werror` clean on every touched file
   - [ ] Debug build compiles warnings-as-errors; `ctest --output-on-failure` green (offscreen)
   - [ ] ASAN/UBSAN build + test pass for the touched area (when available)
   - [ ] Golden tests re-run where rendering/output changed; diffs reviewed, never silent `--update`
   - [ ] `db.py task-done <id> --ref <sha>` with real commit
   - [ ] Commit message: Conventional Commit, body explains WHY
3. **New §9 — Review protocol:** every task lands via a review pass by the orchestrator or a designated reviewer agent (trailofbits `c-review` clusters or in-house checklist); no agent merges its own work; reviewer checks: determinism, hostile-input handling, headless-ness, golden diffs, DB evidence.
4. **New §10 — Parallel-work discipline (max 3 agents):** per-area ownership table (core-agent → `src/core/`; cli-agent → `src/cli/`; rtl-agent → `src/core/rtl-*`; test-agent → `src/tests/`, `scripts/`); `git pull --rebase` before every commit; small commits to shrink collision surface; on conflict: orchestrator serializes, never force-push.

   > *Correction note (2026-08-06): the `src/core/` / `src/cli/` paths in this
   > drafted §10 refer to the pre-rename layout. Those scaffolds were removed;
   > core-agent now owns `src/Pdf4QtLibCore/` and cli-agent owns `src/PdfTool/`.
   > This recommendation is reproduced verbatim as a historical snapshot.*
5. **New §11 — Environment & gates:** pinned toolchain (GCC/Clang ≥ …, Qt 6.8 LTS, CMake ≥ 3.21, C++20); one canonical build command block incl. ASAN build and `ctest`; clang-tidy gate once added.
6. **Amend §2:** add rule 9 — **fail-closed**: "If a task is ambiguous or unverifiable, STOP and ask the orchestrator. Do not guess, do not partially claim, do not invent evidence." (vLLM/Ray pattern)
7. **Amend §6:** add "commit sign-off optional; PR/merge descriptions must include an evidence block (tests run, golden diff summary, DB task ref)". (Ray/vLLM pattern)
8. **New §12 — Skill usage:** table mapping roles → mandatory skills (`qt-cpp-review`, `qt-cmake-project`, `qt-cpp-docs` now; `testing-handbook-skills/libfuzzer`, `ossfuzz` for test-agent; `c-review` for review passes; in-house `harfbuzz-shaping` for rtl-agent once authored). (OpenAI mandatory-skill pattern)
9. **Amend §7:** reorder the "if unsure" chain to include `agents/roles/<role>.md` before the DB.

## B.6 Sources

- vLLM AGENTS.md: https://github.com/vllm-project/vllm/blob/main/AGENTS.md `[V]`
- Ray AGENTS.md: https://github.com/ray-project/ray/blob/master/AGENTS.md `[V]`
- OpenAI agents-python AGENTS.md: https://github.com/openai/openai-agents-python/blob/main/AGENTS.md `[V]`
- Grafana AGENTS.md: https://github.com/grafana/grafana/blob/main/AGENTS.md `[V]`
- LangChain AGENTS.md: https://github.com/langchain-ai/langchain/blob/main/AGENTS.md `[V]`
- uv AGENTS.md: https://github.com/astral-sh/uv/blob/main/AGENTS.md `[V]`
- Anthropic guidance: https://www.anthropic.com/engineering/writing-agents-md-files `[I]` (URL known; extract backend unavailable — verify before citing in docs)
- trailofbits/skills: https://github.com/trailofbits/skills `[V]`; overtly-malicious-skills corpus: https://github.com/trailofbits/overtly-malicious-skills `[V]`
- Qt agent-skills: https://github.com/TheQtCompanyRnD/agent-skills `[V]`
- anthropics/skills: https://github.com/anthropics/skills `[V]`; openai/skills: https://github.com/openai/skills `[V]`
- Awesome lists (grep source): ComposioHQ/awesome-claude-skills, VoltAgent/awesome-agent-skills, travisvn/awesome-claude-skills `[V]`
- ClawHub API: https://clawhub.ai/api/search `[V]` (content endpoint 404 `[I]`)
