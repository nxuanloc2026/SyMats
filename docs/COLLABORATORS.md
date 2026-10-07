# Working on Symats — guide for collaborators

Welcome! Symats is an open-source, Mathematica-like notebook (C++20 engine, light UI),
licensed GPL-3.0-or-later. Loc Ngo is the maintainer.

Note: All remaining tasks on `coordination/BOARD.md` have been reassigned to **Jules**.

## 1. One-time setup (about 1 hour)

1. Accept the GitHub invitation to **github.com/nxuanloc2026/Symats** (check your email).
2. Install **Git** and **Visual Studio 2022/2026** with the **Desktop development with C++**
   workload (includes the compiler, CMake, Ninja). macOS/Linux: a C++20 compiler + CMake 3.20+.
3. Clone: `git clone https://github.com/nxuanloc2026/Symats.git`
4. Build and test: open the folder in Visual Studio → preset **windows-debug** → Build All →
   Test Explorer → Run All. (Command line: `cmake -B build && cmake --build build && ctest --test-dir build`.)
   All tests must pass before you change anything.
5. Read, in this order: `AGENTS.md`, `docs/COORDINATION.md`, `docs/EXPR_SPEC.md`,
   `coordination/BOARD.md`, `coordination/handoff/jules.md`.

## 2. Daily workflow

1. `git pull` on `main`.
2. Check `coordination/STATUS.md` and `BOARD.md`.
3. Work on assigned task, branch `jules/T-xxx-...`.
4. Build and run all tests (`ctest`) before each commit.
5. Push, open a pull request to `main`, wait for **CI ✓ (Linux + Windows)**.

## 3. Rules that matter (from AGENTS.md)

- Every source file starts with:
  ```cpp
  // SPDX-License-Identifier: GPL-3.0-or-later
  // Copyright (c) 2026 Loc Ngo and Symats contributors
  ```
- Your contributions are licensed **GPL-3.0-or-later**, like the rest of Symats.
- **Never copy** code, documentation text, or examples from Mathematica/Wolfram, MATLAB, or
  Symbolica. Cite textbooks or papers in code comments.
- Heavy math comes from approved libraries (Giac, FLINT, SUNDIALS, Boost, …) through
  `backend/` only. Check the approved list in AGENTS.md before adding any dependency.
