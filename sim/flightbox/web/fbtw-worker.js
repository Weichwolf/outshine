/* FlightBox tile-worker entry (runs inside the Web Worker fb_terrain.c spawns). Loads the worker WASM
 * module (fbtileworker.js) and services 'open'/'build' messages: fetch + decode + mesh + sRGB mips run
 * here, off the render thread, and finished vertex arrays + mip pyramids go back as transferables. ONE
 * build in flight (the main side gates it): fbtw_build SUSPENDS on the synchronous fetch under ASYNCIFY,
 * so it is called async (ccall async) and a second overlapping call would corrupt shared state. */
var ready = false;
var queue = [];
var Module = { onRuntimeInitialized: function () { ready = true; while (queue.length) handle(queue.shift()); } };
importScripts('fbtileworker.js');

self.onmessage = function (e) { if (!ready) { queue.push(e.data); return; } handle(e.data); };

function handle(d) {
  if (d.cmd === 'open') {
    Module.ccall('fbtw_open', 'number', ['string', 'number', 'number'], [d.base, d.lat, d.lon]);
    self.postMessage({ cmd: 'opened' });
    return;
  }
  if (d.cmd === 'build') {
    Module.ccall('fbtw_build', 'number',
      ['number', 'number', 'number', 'number', 'number', 'number', 'number'],
      [d.z, d.x, d.y, d.grid, d.mode, d.ts, d.what], { async: true }).then(function (res) {
        var msg = { cmd: 'built', z: d.z, x: d.x, y: d.y, mode: d.mode, what: d.what, res: res };
        var transfer = [];
        if (res & 1) {   /* mesh: copy verts out (the worker heap is reused for the next build) */
          var nv = Module._fbtw_nverts(), vp = Module._fbtw_verts();
          var vbuf = Module.HEAPU8.slice(vp, vp + nv * 8 * 4).buffer;
          var op = Module._fbtw_origin() >> 3;
          msg.verts = vbuf; msg.nverts = nv; msg.err = Module._fbtw_err();
          msg.origin = [Module.HEAPF64[op], Module.HEAPF64[op + 1], Module.HEAPF64[op + 2]];
          transfer.push(vbuf);
        }
        if (res & 2) {   /* albedo mip pyramid */
          var mp = Module._fbtw_mips(), mb = Module._fbtw_mipbytes();
          var mbuf = Module.HEAPU8.slice(mp, mp + mb).buffer;
          msg.mips = mbuf; msg.mipbytes = mb; msg.ts = Module._fbtw_ts();
          transfer.push(mbuf);
        }
        Module._fbtw_release();
        self.postMessage(msg, transfer);
      });
  }
}
