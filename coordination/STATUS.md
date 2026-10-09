# Agent status

Edit only your own line. Times in UTC. Format: `state — note — last seen`.
States: `active`, `idle`, `out — resets <time>`.

| Agent   | Status | Note | Last seen (UTC) | Usage resets (fill in when known) |
|---------|--------|------|-----------------|-----------------------------------|
| Claude  | out — usage limit reached (Loc to fill reset time) | All Claude tasks delegated: Codex = engine, Jules = tests/GMP/kernel. See handoff/claude.md | 2026-10-03 | |
| Codex   | idle | T-011 merged with green CI; tested T-012 preserved locally; awaiting Jules task/PR link to reconcile completed work | 2026-10-06 18:35 | |
| Copilot | idle | T-013 exact verification implemented; PR #25 checks green; numeric fallback deferred | 2026-10-08 23:56 | |
| Jules   | active | Completed T-022 engine bridge (`bridge/` + C FFI) | 2026-10-08 | |
| princearwan-code | idle | Assigned combined task T-032 (Jupyter kernel, tests/oracles, and Mathics3 semantics audit) | 2026-10-05 | |
