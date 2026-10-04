// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
let wasm;

function invoke(name, argTypes = [], args = []) {
  return wasm.ccall(name, 'string', argTypes, args);
}

self.onmessage = async ({ data }) => {
  const { id, type, payload } = data;
  try {
    if (type === 'init') {
      const url = payload.moduleUrl;
      const factory = (await import(/* @vite-ignore */ url)).default;
      wasm = await factory({ locateFile: (file) => new URL(file, url).href });
      self.postMessage({ id, value: true });
      return;
    }
    if (!wasm) throw new Error('Symats WebAssembly module is not ready');
    let value;
    if (type === 'runCell') {
      value = payload.statements.map((statement) => {
        const math = statement.mode === 'math';
        const json = math ? JSON.stringify(statement.mathJson) : statement.text;
        const method = math ? 'symats_run_mathjson' : 'symats_run_text';
        return JSON.parse(invoke(method, ['string', 'number'],
          [json, statement.suppressed ? 1 : 0]));
      });
    } else if (type === 'textToMathJSON') {
      value = JSON.parse(invoke('symats_text_to_mathjson', ['string'], [payload.text]));
    } else if (type === 'mathJSONToText') {
      value = JSON.parse(invoke('symats_mathjson_to_text', ['string'],
        [JSON.stringify(payload.mathJson)]));
    } else if (type === 'symbols') {
      value = JSON.parse(invoke('symats_user_symbols'));
    } else if (type === 'restart') {
      wasm.ccall('symats_restart', null, [], []);
      value = true;
    } else throw new Error(`Unknown bridge message: ${type}`);
    self.postMessage({ id, value });
  } catch (error) {
    self.postMessage({ id, error: String(error) });
  }
};
