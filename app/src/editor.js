// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

// A converter implementation must call Symats' MathJSON ⇄ Expr ⇄ text bridge.
// Keeping it injected lets the editor run before bridge/ is available.
export function createEditorController({ math, text, toggle, status, converter }) {
  let mode = 'math';
  let busy = false;

  function render() {
    math.element.hidden = mode !== 'math';
    text.element.hidden = mode !== 'text';
    toggle.textContent = mode === 'math' ? 'Plain text' : 'Math notation';
    toggle.setAttribute(
      'aria-label',
      mode === 'math' ? 'Switch to plain text' : 'Switch to math notation',
    );
    toggle.disabled = busy || !converter;
  }

  async function switchMode() {
    if (busy || !converter) return;
    busy = true;
    status.textContent = '';
    render();
    try {
      if (mode === 'math') {
        const next = await converter.mathJsonToText(await math.readMathJson());
        if (typeof next !== 'string') throw new Error('The converter returned no text.');
        text.input.value = next;
        mode = 'text';
        text.input.focus();
      } else {
        const next = await converter.textToMathJson(text.input.value);
        await math.writeMathJson(next);
        mode = 'math';
        math.focus();
      }
    } catch (error) {
      // Leave the original editor and its contents intact on conversion failure.
      status.textContent = error instanceof Error ? error.message : 'Conversion failed.';
    } finally {
      busy = false;
      render();
    }
  }

  toggle.addEventListener('click', switchMode);
  if (!converter) status.textContent = 'Text mode will be available when the engine bridge is connected.';
  render();
  return { switchMode, get mode() { return mode; } };
}
