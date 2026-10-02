# Coordination protocol — four agents, with fallback

Applies to Claude, Codex, GitHub Copilot, and Jules. **Read this before every work session.**

Agents cannot talk to each other and can be cut off at any moment when their usage
limit runs out. So all coordination lives in files in this repository:

| File | Purpose | Who edits |
|------|---------|-----------|
| `coordination/BOARD.md` | The task list: owner, backup, status, last update | Each agent edits **only rows it owns or takes over**; Loc edits anything |
| `coordination/STATUS.md` | One line per agent: active / out of usage / reset time | Each agent edits **only its own line**; Loc edits anything |
| `coordination/handoff/<agent>.md` | Live notes so anyone can continue that agent's task | **Only that agent** (or its backup during a takeover, in a marked section) |

## 1. Start of every session (all agents)

1. `git pull` (cloud agents: you already start from the latest `main`).
2. Read `coordination/STATUS.md`, `coordination/BOARD.md`, and your own handoff file.
3. Set your STATUS line to `active` with the current date/time (UTC).
4. Pick work in this order:
   1. Your own `in-progress` task (continue from your handoff note).
   2. Your own next `todo` task.
   3. **Fallback work** — a task whose owner is `out` (see §3) and for which you are backup.
   4. **Always-safe work** (§4).

## 2. While working — checkpoint so nothing is lost

You may be cut off without warning. Therefore:

- Claim a task before starting: set its row to `in-progress`, your name, and the time.
- Work on a branch named `<agent>/<task-id>-short-name` (e.g. `claude/T-001-evaluator`).
- **Checkpoint at least every ~30 minutes of work or after each meaningful step:**
  1. commit (`WIP: T-001 pattern matcher parses Blank` is fine),
  2. update your handoff file: what is done, what is next, files touched, open questions,
  3. update the `Updated` time on your BOARD row.
- Update the handoff **before** a long step, not after — the session may end during it.
- Tasks must be small enough to finish in one session (≈ < 400 changed lines).
  If a task is bigger, split it into new BOARD rows first.

## 3. When an agent is out of usage

### How others know
- **Explicit:** the agent (if it can) or Loc sets its STATUS line to
  `out — resets <date/time>`.
- **Implicit (timeout):** a task is **stale** if its row is `in-progress` and
  `Updated` is older than **12 hours**. Treat its owner as `out` for that task.

### What the backup does
1. Read the owner's handoff file and its branch.
2. If the task is **blocking other work** (BOARD column `Blocks`), continue it:
   - create a new branch `takeover/<task-id>` from the owner's branch (never rewrite theirs),
   - set the row's owner to `<backup> (for <owner>)`,
   - write progress in the owner's handoff file under a heading `## Takeover by <backup>`.
3. If the task is **not blocking**, leave it and do other `todo` work in the absent
   agent's lane only if it is marked `backup-ok`; otherwise do always-safe work.
4. Never make design changes in another agent's lane during a takeover
   (no renaming public APIs, no changing `docs/EXPR_SPEC.md`). Note proposals in the handoff.

### When the agent returns
1. Read STATUS, BOARD, and its handoff (including any `Takeover` section).
2. If a backup finished the task, review its PR instead of redoing it.
3. If the takeover is half done, continue from the `takeover/` branch.
4. Set STATUS back to `active`.

### Backup matrix

| Lane (see AGENTS.md) | Primary | 1st backup | 2nd backup |
|------|---------|-----------|-----------|
| `core/` engine, D/Expand, verification; `backend/` adapters (Giac, FLINT, SUNDIALS) | Claude | Codex | Copilot |
| `convert/`, `app/` (MathLive editor, Plotly rendering) | Codex | Claude | Copilot |
| `third_party/` builds, `bridge/`, `cli/`, CMake, CI, installers | Copilot | Codex | Claude |
| `tests/` | Jules | Copilot | Claude |
| PR review | any agent other than the author | next available | Loc |

If **only one agent is left**, it does: blocking tasks in any lane (smallest first) →
tests → reviews → docs. If **none** are available, Loc reviews and merges open PRs.

## 4. Always-safe work (anyone, anytime)

These never conflict with other agents' in-progress work:
- Add tests for already-merged code (put new files in `tests/`, don't edit others' test files mid-task).
- Review open pull requests and leave comments.
- Fix compiler warnings or documentation typos in files no one has `in-progress`.
- Split a large BOARD task into smaller rows (don't change its owner).

## 5. End of session

1. Commit and push your branch; open or update the PR (`Closes #<issue>` / task id in title).
2. Update the handoff file and BOARD row (`review` when the PR is ready, else `in-progress`).
3. Set your STATUS line: `idle`, or `out — resets <time>` if you know your limit is reached.

## 6. Rules for Loc (the maintainer)

- You are the only one who merges to `main` and the only one who changes owners
  outside of takeovers.
- When you notice an agent hit its limit, set its STATUS line to `out — resets <time>`;
  that is the clearest signal to the others.
- Record each agent's limit reset pattern in STATUS.md once you know it.
