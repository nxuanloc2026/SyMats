# Symats notebook front end

Run `pnpm install` and `pnpm dev` in `app/` for the UI preview. The page is a
bare notebook: CodeMirror 6 text cells, MathLive math cells, KaTeX/STIX Two
output, and inline Plotly plots. Shift+Enter runs and advances; Ctrl+M switches
input mode. The workspace panel is hidden until opened.

The front end calls `window.symatsHost`, supplied by the engine bridge (T-022):

| Method | Request | Response |
| --- | --- | --- |
| `runCell(input, {signal})` | `{mode:'text', text}` or `{mode:'math', mathJson}` | Array of `{line, latex, text, status, error, suppressed, visible, plot}` |
| `textToMath(text)` | Plain text | LaTeX string |
| `mathToText(mathJson)` | MathJSON value | Plain text string |
| `abort()` | — | Stops current work |
| `restart()` | — | Clears session |
| `userSymbols()` | — | Array of symbol names |

`plot` contains Plotly `{data, layout}`. The UI reports a missing kernel or
converter instead of silently changing math during a mode switch.
