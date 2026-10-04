# Engine bridge (T-022)

`symats_bridge` exposes the C++ Session through a UTF-8 C ABI. The API returns
JSON with a result line, MathJSON, plain text, status, suppression flag, and
error. It also converts text/MathJSON and provides restart and workspace names.
The browser wrapper runs the WebAssembly module in a worker, so evaluation
does not block the notebook UI.

For a WebAssembly build with Emscripten installed:

```sh
emcmake cmake -S . -B out/wasm -DSYMATS_BUILD_TESTS=OFF -DSYMATS_BUILD_CLI=OFF
cmake --build out/wasm --target symats_wasm
```

Serve the resulting `symats_wasm.js` and `symats_wasm.wasm` together. In the
app entry point, create a Compute Engine and call
`createSymatsHost('/symats_wasm.js', ce)` from `bridge/web/host.js`, then assign
the returned host to `window.symatsHost`.

`runCell` accepts `{statements:[{mode:'text',text,suppressed}, ...]}` and returns
one result per statement. The notebook app branch and cell parser branch must
land before they can send complete multi-statement cells. This branch builds
the transport and session API independently. Aborting terminates the worker
and starts a fresh session, so the workspace resets after abort.
