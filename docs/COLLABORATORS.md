# Working on Symats with your own Claude — guide for Charles and Prince

Welcome! Symats is an open-source, Mathematica-like notebook (C++20 engine, light UI),
licensed GPL-3.0-or-later. Loc Ngo is the maintainer. Four AI agents already work on it
(Claude, Codex, Copilot, Jules). You join as **human collaborators, each with your own
Claude** (your own subscription). This page tells you and your Claude how to fit in.

## 1. Your roles (proposed — Loc confirms)

| Person | Area (lane) | First tasks (see `coordination/BOARD.md`) |
|--------|-------------|-------------------------------------------|
| **Charles** + Claude | **Engine math** — `core/` (calculus, simplification, pattern matching) | T-010 native `D` and `Expand`, then T-018 (Flat/Orderless matching), T-013 (result verification) |
| **Prince (`princearwan-code`)** + Claude | **Notebook and integration** — `kernel/`, `bridge/`, `cli/` | T-032 (combined: `symats-cli` REPL, `.sym` script runner, engine-app bridge), T-029 (Boost.Odeint numeric ODEs) |

These tasks are currently queued for Codex (temporarily covering Claude's lane). Before
starting one, **reassign it on the board to yourself** (`Charles` / `Prince`) so Codex skips it.

## 2. One-time setup (about 1 hour)

1. Accept the GitHub invitation to **github.com/nxuanloc2026/Symats** (check your email).
2. Install **Git** and **Visual Studio 2022/2026** with the **Desktop development with C++**
   workload (includes the compiler, CMake, Ninja). macOS/Linux: a C++20 compiler + CMake 3.20+.
3. Clone: `git clone https://github.com/nxuanloc2026/Symats.git`
4. Build and test: open the folder in Visual Studio → preset **windows-debug** → Build All →
   Test Explorer → Run All. (Command line: `cmake -B build && cmake --build build && ctest --test-dir build`.)
   All tests must pass before you change anything.
5. Read, in this order: `AGENTS.md`, `docs/COORDINATION.md`, `docs/EXPR_SPEC.md`,
   `coordination/BOARD.md`, `coordination/handoff/claude.md` (has detailed plans for engine tasks).
   All code, scripts, tests, and documentation use Tier 1 text syntax: square bracket
   calls (`Sin[x]`, `f[x_] := Sin[x]`), capitalized built-ins, and curly brace lists.
6. Add yourself to the coordination files (one small PR):
   - a row in `coordination/STATUS.md`: `| Claude (Charles) | idle | ... |` / `| Claude (Prince) | idle | ... |`
   - a handoff file `coordination/handoff/charles.md` or `prince.md` (copy the structure of `claude.md`)

## 3. Setting up your Claude

Any of these works; the first is best for coding:

- **Claude Code** (terminal) — run it inside the Symats folder. It automatically reads
  `CLAUDE.md`, which points it to `AGENTS.md` and the coordination rules.
- **Claude desktop app (Cowork)** — give it access to your local Symats folder.
- **claude.ai chat** — works for design questions and reviews; paste files as needed.

### Prompt to start every session (copy, change the name and task)

> You are working on Symats as the assistant of **Charles** (lane: engine math, core/).
> First read CLAUDE.md, AGENTS.md, docs/COORDINATION.md, coordination/BOARD.md,
> coordination/STATUS.md and coordination/handoff/charles.md. Then: run `git pull`,
> claim task **T-010** on the board (owner "Charles", status in-progress, time), create
> branch `charles/T-010-calculus`, and implement it following the plan in
> coordination/handoff/claude.md. Write tests for every new function. Build and run all
> tests before each commit. Checkpoint (commit + update handoff/charles.md) at least every
> 30 minutes. Do not change public APIs in core/include/symats/*.h or docs/EXPR_SPEC.md
> without writing the reason in the handoff file. When done, push and tell me the PR link.

### Other useful prompts

- **Continue after a break or usage reset:** "Read coordination/handoff/charles.md and the
  branch `charles/T-010-calculus`, then continue where it stopped."
- **Review someone's pull request:** "Review PR #N on github.com/nxuanloc2026/Symats against
  AGENTS.md: correctness, tests, license headers, lane rules. List problems by severity."
- **Write tests only:** "Add tests for <feature> in a new file under tests/, using the
  harness in tests/test.h. Expected values must come from mathematics, textbooks, SymPy or
  Maxima — never from proprietary software."
- **Explain code:** "Explain how core/src/eval.cpp evaluates an expression, step by step."

## 4. Daily workflow

1. `git pull` on `main`.
2. Check `coordination/STATUS.md` and `BOARD.md` — never start a task someone else has
   `in-progress` (agents included).
3. Claim your task, branch `charles/T-xxx-...` or `prince/T-xxx-...`.
4. Work with your Claude; review its changes yourself before each commit.
5. Push, open a pull request to `main`, wait for **CI ✓ (Linux + Windows)**.
6. Loc reviews and merges. Only Loc merges to `main`.
7. Update your STATUS line (`idle`, or `out — resets <time>` if your Claude's usage ran out).

If your Claude runs out of usage mid-task: make sure the handoff file is current and set
STATUS to `out — resets <time>`. The backup rules in `docs/COORDINATION.md` apply to you too.

## 5. Rules that matter (from AGENTS.md)

- Every source file starts with:
  ```cpp
  // SPDX-License-Identifier: GPL-3.0-or-later
  // Copyright (c) 2026 Loc Ngo and Symats contributors
  ```
- Your contributions are licensed **GPL-3.0-or-later**, like the rest of Symats.
- **Never copy** code, documentation text, or examples from Mathematica/Wolfram, MATLAB, or
  Symbolica. Do not paste their source into your Claude. Ideas and standard math are fine;
  cite textbooks or papers in code comments.
- Heavy math comes from approved libraries (Giac, FLINT, SUNDIALS, Boost, …) through
  `backend/` only. Check the approved list in AGENTS.md before adding any dependency.
- Small pull requests (< ~400 lines), one task each, tests included, CI green.
- Stay in your lane; if you must touch another lane, keep it minimal and explain in the PR.

## 6. Two things to settle with Loc

- **Usage and accounts:** use your own Claude subscription; don't share accounts.
- **University ownership:** if Symats work overlaps with your job or research funding,
  check your institution's IP policy before contributing.

Questions → comment on the GitHub issue or PR, or ask Loc.
