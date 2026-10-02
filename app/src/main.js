// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
import { MathfieldElement } from 'mathlive';
import 'mathlive/fonts.css';
import { createEditorController } from './editor.js';
import './styles.css';

let computeEnginePromise;
function getComputeEngine() {
  computeEnginePromise ??= import('@cortex-js/compute-engine')
    .then(({ ComputeEngine }) => new ComputeEngine());
  return computeEnginePromise;
}
const field = new MathfieldElement();
field.value = 'x^2+1';
field.setAttribute('aria-label', 'Math expression');
document.querySelector('#math-editor').append(field);

const bridge = globalThis.symatsConverter ?? null;
createEditorController({
  math: {
    element: document.querySelector('#math-editor'),
    readMathJson: async () => (await getComputeEngine()).parse(field.value).json,
    writeMathJson: async (json) => { field.value = (await getComputeEngine()).expr(json).latex; },
    focus: () => field.focus(),
  },
  text: {
    element: document.querySelector('#text-editor'),
    input: document.querySelector('#text-input'),
  },
  toggle: document.querySelector('#mode-toggle'),
  status: document.querySelector('#cell-status'),
  converter: bridge,
});
