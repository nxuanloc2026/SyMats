// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
import assert from 'node:assert/strict';
import test from 'node:test';
import { createEditorController } from '../src/editor.js';

function fixture(converter) {
  const math = {
    element: { hidden: false },
    readMathJson: () => ['Add', 'x', 1],
    writeMathJson(value) { this.lastWritten = value; },
    focus() {},
  };
  const text = { element: { hidden: true }, input: { value: '', focus() {} } };
  const toggle = {
    disabled: false,
    addEventListener(_event, callback) { this.click = callback; },
    setAttribute() {},
  };
  const status = { textContent: '' };
  return { math, text, toggle, status, converter };
}

test('switches through the converter in both directions', async () => {
  const calls = [];
  const ui = fixture({
    async mathJsonToText(json) { calls.push(json); return 'x + 1'; },
    async textToMathJson(source) { calls.push(source); return ['Add', 'x', 2]; },
  });
  const editor = createEditorController(ui);
  await editor.switchMode();
  assert.equal(editor.mode, 'text');
  assert.equal(ui.text.input.value, 'x + 1');
  assert.equal(ui.math.element.hidden, true);
  ui.text.input.value = 'x + 2';
  await editor.switchMode();
  assert.equal(editor.mode, 'math');
  assert.deepEqual(ui.math.lastWritten, ['Add', 'x', 2]);
  assert.deepEqual(calls, [['Add', 'x', 1], 'x + 2']);
});

test('a conversion error keeps the original mode and source', async () => {
  const ui = fixture({
    async mathJsonToText() { throw new Error('Unsupported expression'); },
  });
  const editor = createEditorController(ui);
  await editor.switchMode();
  assert.equal(editor.mode, 'math');
  assert.equal(ui.math.element.hidden, false);
  assert.equal(ui.status.textContent, 'Unsupported expression');
});

test('invalid MathJSON leaves edited text visible for correction', async () => {
  const ui = fixture({
    async mathJsonToText() { return 'x'; },
    async textToMathJson() { return null; },
  });
  ui.math.writeMathJson = async () => { throw new Error('Unsupported MathJSON'); };
  const editor = createEditorController(ui);
  await editor.switchMode();
  ui.text.input.value = 'x + ?';
  await editor.switchMode();
  assert.equal(editor.mode, 'text');
  assert.equal(ui.text.input.value, 'x + ?');
  assert.equal(ui.status.textContent, 'Unsupported MathJSON');
});

test('mode toggle waits for an engine bridge', async () => {
  const ui = fixture(null);
  const editor = createEditorController(ui);
  assert.equal(ui.toggle.disabled, true);
  await editor.switchMode();
  assert.equal(editor.mode, 'math');
});
