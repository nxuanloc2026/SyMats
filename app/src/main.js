// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
import { EditorView, minimalSetup } from 'codemirror';
import { keymap } from '@codemirror/view';
import { MathfieldElement } from 'mathlive';
import { ComputeEngine } from '@cortex-js/compute-engine';
import katex from 'katex';
import 'katex/dist/katex.min.css';
import '@fontsource/stix-two-math/400.css';
import './style.css';

const ce = new ComputeEngine();
MathfieldElement.computeEngine = ce;
const notebook = document.querySelector('#app');
notebook.innerHTML = `<header class="topbar">
  <input class="notebook-name" aria-label="Notebook name" value="Untitled" />
  <nav aria-label="Notebook controls">
    <button id="run-all" title="Run all cells" aria-label="Run all">▶</button>
    <button id="abort" title="Abort" aria-label="Abort">■</button>
    <button id="restart" title="Restart kernel" aria-label="Restart kernel">⟳</button>
    <button id="workspace-toggle" title="Workspace" aria-label="Workspace">Workspace</button>
    <button id="more" title="More" aria-label="More">⋯</button>
  </nav>
</header>
<div class="layout"><main id="cells" aria-label="Notebook cells"></main>
<aside id="workspace" hidden><h2>Workspace</h2><ul></ul></aside></div>
<div id="menu" hidden><button id="add-text">Add text cell</button><button id="add-input">Add input cell</button></div>`;

const cellsRoot = document.querySelector('#cells');
const workspace = document.querySelector('#workspace');
const cells = [];
let running = false;
let abortController;

function host() { return window.symatsHost; }
function showError(cell, message) {
  const row = document.createElement('div');
  row.className = 'output error';
  row.textContent = message;
  cell.outputs.append(row);
}
function setBusy(cell, busy) {
  cell.element.classList.toggle('busy', busy);
  cell.element.setAttribute('aria-busy', String(busy));
}
function focusCell(cell) {
  if (cell.mode === 'math') cell.math.focus();
  else cell.editor.focus();
}
function currentText(cell) { return cell.editor.state.doc.toString(); }

async function toggleMath(cell) {
  cell.outputs.replaceChildren();
  if (cell.mode === 'text') {
    const text = currentText(cell);
    if (text.trim()) {
      if (!host()?.textToMath) {
        showError(cell, 'Math conversion needs the Symats kernel.');
        return;
      }
      try { cell.math.value = await host().textToMath(text); }
      catch (error) { showError(cell, String(error)); return; }
    }
    cell.mode = 'math';
  } else {
    if (cell.math.value.trim()) {
      if (!host()?.mathToText) {
        showError(cell, 'Math conversion needs the Symats kernel.');
        return;
      }
      try {
        const json = ce.parse(cell.math.value, { form: 'raw' }).json;
        const text = await host().mathToText(json);
        cell.editor.dispatch({ changes: { from: 0, to: cell.editor.state.doc.length, insert: text } });
      } catch (error) { showError(cell, String(error)); return; }
    }
    cell.mode = 'text';
  }
  cell.element.classList.toggle('math-mode', cell.mode === 'math');
  cell.toggle.textContent = cell.mode === 'math' ? 'Text' : 'Math';
  focusCell(cell);
}

async function renderResult(cell, result) {
  if (result.error) { showError(cell, result.error); return; }
  if (result.suppressed || result.visible === false) return;
  const row = document.createElement('div');
  row.className = 'output';
  const gutter = document.createElement('span');
  gutter.className = 'gutter';
  gutter.textContent = result.line ? `Out[${result.line}]=` : '';
  const body = document.createElement('div');
  body.className = 'output-body';
  row.append(gutter, body);
  cell.outputs.append(row);
  if (result.plot) {
    const Plotly = (await import('plotly.js-dist-min')).default;
    await Plotly.newPlot(body, result.plot.data, result.plot.layout, { responsive: true });
  } else if (result.latex) {
    katex.render(result.latex, body, { throwOnError: false, trust: false, displayMode: true });
  } else body.textContent = result.text ?? '';
  if (result.status && result.status !== 'verified' && result.status !== 'exact') {
    const status = document.createElement('small');
    status.className = 'status';
    status.textContent = result.status;
    body.append(status);
  }
}

async function updateWorkspace() {
  if (workspace.hidden || !host()?.userSymbols) return;
  const names = await host().userSymbols();
  const list = workspace.querySelector('ul');
  list.replaceChildren(...names.map((name) => {
    const item = document.createElement('li');
    item.textContent = name;
    return item;
  }));
}

async function runCell(cell, advance = false) {
  if (running) return;
  cell.outputs.replaceChildren();
  if (!host()?.runCell) { showError(cell, 'Symats kernel is not connected.'); return; }
  running = true;
  abortController = new AbortController();
  setBusy(cell, true);
  try {
    const input = cell.mode === 'math'
      ? { mode: 'math', mathJson: ce.parse(cell.math.value, { form: 'raw' }).json }
      : { mode: 'text', text: currentText(cell) };
    const results = await host().runCell(input, { signal: abortController.signal });
    for (const result of results) await renderResult(cell, result);
    if (results[0]?.line) cell.gutter.textContent = `In[${results[0].line}]:=`;
    await updateWorkspace();
    if (advance) {
      let next = cells[cells.indexOf(cell) + 1];
      if (!next) next = addCell();
      focusCell(next);
    }
  } catch (error) {
    if (error.name !== 'AbortError') showError(cell, String(error));
  } finally {
    running = false;
    abortController = undefined;
    setBusy(cell, false);
  }
}

function addCell(kind = 'input', after = cells.length - 1) {
  const element = document.createElement('section');
  element.className = `cell ${kind}`;
  element.innerHTML = `<span class="gutter input-gutter">In[ ]:=</span>
    <div class="cell-content"><div class="editor"></div><div class="math-editor"></div>
      <div class="outputs"></div></div>
    <div class="cell-controls"><button class="toggle" title="Toggle math/text (Ctrl+M)">Math</button>
      <button class="move-up" title="Move up">↑</button><button class="move-down" title="Move down">↓</button>
      <button class="delete" title="Delete cell">×</button></div>`;
  const cell = { element, kind, mode: 'text', outputs: element.querySelector('.outputs'),
    gutter: element.querySelector('.input-gutter'), toggle: element.querySelector('.toggle') };
  const index = Math.max(0, after + 1);
  cells.splice(index, 0, cell);
  cellsRoot.insertBefore(element, cellsRoot.children[index] ?? null);
  cell.editor = new EditorView({
    doc: '', parent: element.querySelector('.editor'),
    extensions: [keymap.of([
      { key: 'Shift-Enter', run: () => { void runCell(cell, true); return true; } },
      { key: 'Ctrl-m', run: () => { void toggleMath(cell); return true; } },
    ]), minimalSetup, EditorView.lineWrapping],
  });
  cell.math = new MathfieldElement();
  cell.math.setAttribute('aria-label', 'Math input');
  cell.math.addEventListener('keydown', (event) => {
    if (event.shiftKey && event.key === 'Enter') { event.preventDefault(); void runCell(cell, true); }
    if (event.ctrlKey && event.key.toLowerCase() === 'm') { event.preventDefault(); void toggleMath(cell); }
  });
  element.querySelector('.math-editor').append(cell.math);
  cell.toggle.onclick = () => void toggleMath(cell);
  element.querySelector('.delete').onclick = () => {
    if (cells.length === 1) return;
    cells.splice(cells.indexOf(cell), 1); cell.editor.destroy(); element.remove();
  };
  for (const [direction, delta] of [['up', -1], ['down', 1]]) {
    element.querySelector(`.move-${direction}`).onclick = () => {
      const old = cells.indexOf(cell), target = old + delta;
      if (target < 0 || target >= cells.length) return;
      cells.splice(old, 1); cells.splice(target, 0, cell);
      cellsRoot.insertBefore(element, cellsRoot.children[target + (delta > 0 ? 1 : 0)] ?? null);
      focusCell(cell);
    };
  }
  return cell;
}

document.querySelector('#run-all').onclick = async () => {
  for (const cell of cells) if (cell.kind === 'input') await runCell(cell);
};
document.querySelector('#abort').onclick = () => {
  abortController?.abort(); host()?.abort?.();
};
document.querySelector('#restart').onclick = async () => {
  if (host()?.restart) await host().restart();
  for (const cell of cells) { cell.outputs.replaceChildren(); cell.gutter.textContent = 'In[ ]:='; }
  await updateWorkspace();
};
document.querySelector('#workspace-toggle').onclick = () => {
  workspace.hidden = !workspace.hidden; void updateWorkspace();
};
document.querySelector('#more').onclick = () => {
  const menu = document.querySelector('#menu'); menu.hidden = !menu.hidden;
};
document.querySelector('#add-input').onclick = () => focusCell(addCell());
document.querySelector('#add-text').onclick = () => focusCell(addCell('text'));
focusCell(addCell());
