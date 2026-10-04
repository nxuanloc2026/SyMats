// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors

// Browser host for a WebAssembly worker. Call createSymatsHost(moduleUrl, ce)
// and assign its result to window.symatsHost in the notebook entry point.
export async function createSymatsHost(moduleUrl, computeEngine) {
  let worker;
  let ready;
  let nextId = 0;
  const pending = new Map();

  function start() {
    worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
    worker.onmessage = ({ data }) => {
      const request = pending.get(data.id);
      if (!request) return;
      pending.delete(data.id);
      if (data.error) request.reject(new Error(data.error));
      else request.resolve(data.value);
    };
    worker.onerror = (event) => {
      for (const request of pending.values()) request.reject(new Error(event.message));
      pending.clear();
    };
    ready = send('init', { moduleUrl });
  }
  function send(type, payload = {}) {
    const id = ++nextId;
    return new Promise((resolve, reject) => {
      pending.set(id, { resolve, reject });
      worker.postMessage({ id, type, payload });
    });
  }
  start();
  await ready;

  return {
    async runCell(input, { signal } = {}) {
      await ready;
      if (signal?.aborted) throw new DOMException('Aborted', 'AbortError');
      const statements = input.statements ?? [input];
      const results = await send('runCell', { statements });
      return results.map((result) => {
        if (result.mathJson && computeEngine)
          result.latex = computeEngine.box(result.mathJson).latex;
        return result;
      });
    },
    async textToMath(text) {
      await ready;
      const json = await send('textToMathJSON', { text });
      if (json?.error) throw new Error(json.error);
      return computeEngine.box(json).latex;
    },
    async mathToText(mathJson) {
      await ready;
      const value = await send('mathJSONToText', { mathJson });
      if (value?.error) throw new Error(value.error);
      return value;
    },
    async userSymbols() { await ready; return send('symbols'); },
    async restart() { await ready; return send('restart'); },
    abort() {
      worker.terminate();
      for (const request of pending.values())
        request.reject(new DOMException('Aborted', 'AbortError'));
      pending.clear();
      start();
    },
  };
}
