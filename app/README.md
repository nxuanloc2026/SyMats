# Symats editor shell

This is the first T-016 app slice. It renders a MathLive expression cell and
provides a plain-text editor. Run `pnpm install`, `pnpm dev`, `pnpm test`, or
`pnpm build` in this directory.

The mode button requires `globalThis.symatsConverter` with two asynchronous
methods: `mathJsonToText(mathJson)` and `textToMathJson(text)`. The adapter must
call Symats' C++ converter through `bridge/`: MathJSON → Expr → text and
text → Expr → MathJSON. Without that bridge, math editing remains available
and the button explains why text mode is unavailable. Conversion errors leave
the current input untouched.

MathLive and Compute Engine are pinned in `package.json`. Plot cells and Tauri
packaging are separate T-016 tasks.
