/* FlightBox tile-worker entry (runs inside the Web Worker fb_terrain.c spawns). Loads the worker WASM
 * module (fbtileworker.js) and services 'open'/'build' messages: fetch + decode + mesh run here, off
 * the render thread, and finished vertex arrays go back as transferables. ONE
 * build in flight (the main side gates it): fbtw_build SUSPENDS on the synchronous fetch under ASYNCIFY,
 * so it is called async (ccall async) and a second overlapping call would corrupt shared state. */
var ready = false;
var queue = [];
var Module = { onRuntimeInitialized: function () { ready = true; while (queue.length) handle(queue.shift()); } };
importScripts('fbtileworker.js');

self.onmessage = function (e) { if (!ready) { queue.push(e.data); return; } handle(e.data); };

/* This module's linear memory, read out of core/ModuleMemory.h's own words: the client's ledger runs
   in another address space and can measure nothing in here. Every reply carries it, so the ledger is
   never older than this worker's last piece of work. NOT named `memory`: importScripts shares this
   global scope and the emscripten glue assigns that name its WebAssembly.Memory. */
function moduleMemory() {
  var p = Module._fbtw_memory(), n = Module._fbtw_memorywords();
  return Array.prototype.slice.call(Module.HEAPF64.subarray(p >> 3, (p >> 3) + n));
}

function handle(d) {
  if (d.cmd === 'open') {
    Module.ccall('fbtw_open', 'number', ['string', 'number', 'number'], [d.base, d.lat, d.lon]);
    self.postMessage({ cmd: 'opened', mem: moduleMemory() });
    return;
  }
  if (d.cmd === 'dag') {
    /* No fetch, so no ASYNCIFY: the soup goes in, the ladder comes straight back out. */
    var sb = new Uint8Array(d.soup), sp = Module._fbtw_take_soup(sb.length);
    Module.HEAPU8.set(sb, sp);
    var ok = Module._fbtw_dag(sp, d.nverts, d.seam);
    Module._free(sp);
    var out = { cmd: 'dagged', id: d.id, res: ok, mem: moduleMemory() };
    var tr = [];
    if (ok) {
      var vp = Module._fbtw_verts(), nv = Module._fbtw_nverts();
      var vb = Module.HEAPU8.slice(vp, vp + nv * 8 * 4).buffer;
      var ip = Module._fbtw_idx(), ni = Module._fbtw_nidx();
      var ib = Module.HEAPU8.slice(ip, ip + ni * 4).buffer;
      var cp = Module._fbtw_clusters(), cbytes = Module._fbtw_clusterbytes();
      var cb = Module.HEAPU8.slice(cp, cp + cbytes).buffer;
      out.verts = vb; out.nverts = nv; out.idx = ib; out.nidx = ni;
      out.clusters = cb; out.nclusters = Module._fbtw_nclusters();
      tr.push(vb); tr.push(ib); tr.push(cb);
    }
    Module._fbtw_release();
    self.postMessage(out, tr);
    return;
  }
  if (d.cmd === 'build') {
    Module.ccall('fbtw_build', 'number',
      ['number', 'number', 'number', 'number'],
      [d.z, d.x, d.y, d.grid], { async: true }).then(function (res) {
        var msg = { cmd: 'built', z: d.z, x: d.x, y: d.y, res: res, mem: moduleMemory() };
        var transfer = [];
        if (res & 1) {   /* mesh: the DAG's own verts + clusters, copied out (the heap is reused) */
          var nv = Module._fbtw_nverts(), vp = Module._fbtw_verts();
          var vbuf = Module.HEAPU8.slice(vp, vp + nv * 8 * 4).buffer;
          var ni = Module._fbtw_nidx(), ip = Module._fbtw_idx();
          var ibuf = Module.HEAPU8.slice(ip, ip + ni * 4).buffer;
          /* raw bytes, and their COUNT comes from the module too: a sizeof written down here would
             be a second declaration of the struct's layout */
          var cp = Module._fbtw_clusters(), cbytes = Module._fbtw_clusterbytes();
          var cbuf = Module.HEAPU8.slice(cp, cp + cbytes).buffer;
          msg.nclusters = Module._fbtw_nclusters();
          var op = Module._fbtw_origin() >> 3;
          msg.verts = vbuf; msg.nverts = nv; msg.err = Module._fbtw_err();
          msg.idx = ibuf; msg.nidx = ni;
          msg.clusters = cbuf;
          msg.origin = [Module.HEAPF64[op], Module.HEAPF64[op + 1], Module.HEAPF64[op + 2]];
          transfer.push(vbuf); transfer.push(ibuf); transfer.push(cbuf);
        }
        Module._fbtw_release();
        self.postMessage(msg, transfer);
      });
  }
}
