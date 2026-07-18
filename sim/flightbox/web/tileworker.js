/* FlightBox tile worker — the glue that runs tw.wasm inside a Web Worker.
 *
 * The main thread (cc.wasm) posts {cmd:'build', z,x,y,...}; this fetches, decodes and MESHES the
 * tile off the main thread, then posts back ONE finished vertex array, transferred (zero-copy).
 * That is the whole point: w3_chunk_build produces w3_vtx[] with no GL and no main-thread state
 * (it was split pure so `err` could be unit-tested), so it runs here and only the glBufferData
 * upload stays on the main thread.
 *
 * Plain Worker + Transferable, NOT pthreads/SharedArrayBuffer: the tile bytes live in a JS Map on
 * the main thread, EM_JS runs on the calling thread, so a pthread provider would have to proxy
 * ~1800 byte requests back into the main thread it is meant to empty. So the bytes cross a
 * boundary either way; this worker owns its own (synchronous) fetching instead.
 */
'use strict';
importScripts('tw.js');

let mod = null;
const pending = [];                       /* messages that arrived before the wasm finished loading */

TileWorker().then((m) => {
  mod = m;
  postMessage({ type: 'ready' });
  for (const d of pending) handle(d);
  pending.length = 0;
});

onmessage = (e) => { if (!mod) { pending.push(e.data); return; } handle(e.data); };

/* tw_build SUSPENDS: its synchronous fetch and its retry sleep are ASYNCIFY unwind points, so the
 * C call does not run to completion inline -- ccall must be told {async:true} and the result
 * awaited. Without that, ccall returns BEFORE the C function finishes, tw_out is still empty, and
 * the reads below come back all-zero while `ok` looks truthy. (Measured exactly that: ok:1 with
 * nverts:0/err:0/yoff:0.) So handle() is async and every entry point that can reach the fetch is
 * awaited. */
async function handle(d) {
  if (d.cmd === 'open') {
    const ok = mod.ccall('tw_open', 'number', ['string', 'number', 'number'],
                         [d.base, d.lat, d.lon]);
    postMessage({ type: 'opened', ok });
    return;
  }
  if (d.cmd === 'build') {
    const ok = await mod.ccall('tw_build', 'number',
                         ['number', 'number', 'number', 'number', 'number'],
                         [d.z, d.x, d.y, d.grid, d.want_yoff], { async: true });
    if (!ok) { postMessage({ type: 'built', ok: 0, z: d.z, x: d.x, y: d.y, seq: d.seq }); return; }

    const nverts = mod.ccall('tw_nverts', 'number', [], []);
    const err    = mod.ccall('tw_err',    'number', [], []);
    const yoff   = mod.ccall('tw_yoff_get', 'number', [], []);
    const ptr    = mod.ccall('tw_verts',  'number', [], []);   /* float* into the worker heap */

    /* w3_vtx = 8 floats (pos3 + uv2 + norm3). Copy out of the worker heap into a standalone
     * buffer so it can be TRANSFERRED -- the heap itself is not transferable, and a view into it
     * would dangle the moment tw_release frees it. */
    const floats = nverts * 8;
    const out = new Float32Array(floats);
    out.set(mod.HEAPF32.subarray(ptr >> 2, (ptr >> 2) + floats));

    /* Per-tile ECEF origin (3 doubles) -- the floating-point anchor the main thread subtracts the
     * camera ECEF from. Read via HEAPF64; origin is a static, so tw_release (which only frees the
     * vertex buffer) does not disturb it. */
    const optr = mod.ccall('tw_origin_get', 'number', [], []);
    const oi = optr >> 3;
    const origin = [mod.HEAPF64[oi], mod.HEAPF64[oi + 1], mod.HEAPF64[oi + 2]];
    mod.ccall('tw_release', null, [], []);

    postMessage({ type: 'built', ok: 1, z: d.z, x: d.x, y: d.y, seq: d.seq,
                  err, nverts, yoff, origin, verts: out.buffer }, [out.buffer]);
  }
}
