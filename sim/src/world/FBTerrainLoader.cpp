/* FlightBox WebGPU demo — real-terrain loader. Fetches a square field of fb-tiles DEM tiles around a
 * geographic centre, meshes each through the SAME machinery the sim uses (osmmesh -> chunkmesh_ecef,
 * w3_vtx pos3+uv2+norm3, camera-relative ECEF), and merges them into one vertex array for the GPU.
 *
 * This is the tileworker.c pipeline (SYNCHRONOUS emscripten_fetch under ASYNCIFY -> osmmesh_fetch_tile
 * -> w3_chunk_build_ecef) but run inline on the demo's main thread: the native path has no tile worker, and a
 * one-shot blocking load before the render loop is the simplest correct thing. Not for cc.js (there the
 * worker exists for the measured frame-time reason documented in tileworker.c). */
#include "FBTerrainLoader.h"
#include "FBMips.h"
#include "style_ver.h"
#include "FBChunkMesh.h"
#include "geo.h"
#include "osmmesh.h"
#include "FBLog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <curl/curl.h>
#include <unistd.h>
#endif

/* stb_image lives in osmmesh's terrain.cpp (STB_IMAGE_IMPLEMENTATION, non-static, shared across the
 * link — see the note there). Declare, don't re-implement: two implementations would collide. C
 * linkage: stb wraps its API in extern "C" under C++. */
extern "C" {
unsigned char *stbi_load_from_memory(const unsigned char *buffer, int len, int *x, int *y,
                                     int *channels_in_file, int desired_channels);
void stbi_image_free(void *retval_from_stbi_load);
}

static char fb_base[160] = "";

#ifdef __EMSCRIPTEN__
/* Synchronous byte fetch. NOTE: emscripten_fetch's SYNCHRONOUS mode is a Web-Worker-only facility
 * (returns NULL on a page's main thread — that is why tileworker.c runs in a worker). The browser
 * demo has no worker, so the correct main-thread primitive is a blocking XHR; binary comes back via
 * the x-user-defined charset trick (each response char is one byte). Returns the HTTP status, or -1
 * on a JS exception; on 200 hands over a malloc'd buffer. */
EM_JS(int, fb_xhr_get, (const char *url, uint8_t **out_ptr, uint32_t *out_len), {
  var u = UTF8ToString(url);
  try {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', u, false);
    xhr.overrideMimeType('text/plain; charset=x-user-defined');
    xhr.send(null);
    if (xhr.status !== 200) return xhr.status | 0;
    var s = xhr.responseText, n = s.length;
    var p = _malloc(n);
    for (var i = 0; i < n; i++) HEAPU8[p + i] = s.charCodeAt(i) & 0xff;
    HEAPU32[out_ptr >> 2] = p;
    HEAPU32[out_len >> 2] = n;
    return 200;
  } catch (e) { return -1; }
})

/* 202/404/5xx = queued or transient, retry after a short sleep (ASYNCIFY); 204 = a real hole; only
 * 200 hands over bytes. */
static int fb_get(const char *url, uint8_t **out, size_t *len) {
  for (int attempt = 0; attempt < 60; attempt++) {
    uint8_t *buf = 0;
    uint32_t n = 0;
    int st = fb_xhr_get(url, &buf, &n);
    if (st == 200 && buf && n > 0) {
      *out = buf;
      *len = (size_t)n;
      return 1;
    }
    if (buf) free(buf);
    if (st == 204) return 0;
    emscripten_sleep(50);
  }
  return 0;
}
#else
/* Native counterpart: libcurl's easy interface, blocking (no ASYNCIFY here — gpu_native is a plain
 * CLI, its own thread of control, no browser event loop to yield to). Same retry contract as the
 * WASM path: 202/404/5xx queued-or-transient (retry after a short sleep), 204 a real hole, only 200
 * hands over bytes. */
struct fb_buf { uint8_t *data; size_t len, cap; };

static size_t fb_curl_write(void *ptr, size_t sz, size_t nmemb, void *userdata) {
  struct fb_buf *b = (struct fb_buf *)userdata;
  size_t n = sz * nmemb;
  if (b->len + n > b->cap) {
    size_t ncap = b->cap ? b->cap * 2 : 4096;
    while (ncap < b->len + n) ncap *= 2;
    uint8_t *grown = (uint8_t *)realloc(b->data, ncap);
    if (!grown) return 0;   /* short write -> curl aborts the transfer */
    b->data = grown;
    b->cap = ncap;
  }
  memcpy(b->data + b->len, ptr, n);
  b->len += n;
  return n;
}

static int fb_get(const char *url, uint8_t **out, size_t *len) {
  for (int attempt = 0; attempt < 60; attempt++) {
    CURL *c = curl_easy_init();
    if (!c) return 0;
    struct fb_buf buf = {0, 0, 0};
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, fb_curl_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(c);
    long status = 0;
    if (res == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(c);
    if (res == CURLE_OK && status == 200 && buf.len > 0) {
      *out = buf.data;
      *len = buf.len;
      return 1;
    }
    free(buf.data);
    if (res == CURLE_OK && status == 204) return 0;
    usleep(50 * 1000);
  }
  return 0;
}
#endif

static int fb_provider(void *user, osmmesh_tile_kind kind, uint32_t z, uint32_t x, uint32_t y,
                       uint8_t **out, size_t *len) {
  (void)user;
  static const char *names[] = {"vector", "terrain", "imagery"};
  if ((int)kind < 0 || (int)kind >= 3) return 0;
  char url[256];
  snprintf(url, sizeof url, "%s/t/%s/%u/%u/%u", fb_base, names[kind], z, x, y);
  return fb_get(url, out, len);
}

/* Missing-albedo layer: a magenta/charcoal checker — an unmistakable "no bake here" marker. */
static void fb_albedo_fallback(int ts, uint8_t *dst) {
  for (int y = 0; y < ts; y++)
    for (int x = 0; x < ts; x++) {
      int chk = ((x >> 6) ^ (y >> 6)) & 1;
      uint8_t *p = dst + ((size_t)y * ts + x) * 4;
      p[0] = chk ? 200 : 40;
      p[1] = chk ? 20 : 40;
      p[2] = chk ? 200 : 40;
      p[3] = 255;
    }
}

/* Fetch the baked OSM albedo (fb-tiles /bake/osm) and decode it into a ts*ts RGBA8 layer. Returns 1
 * on a real image, 0 on any miss/decode failure (caller draws the fallback). */
static int fb_load_albedo(int z, uint32_t x, uint32_t y, int ts, uint8_t *dst) {
  char url[256];
  snprintf(url, sizeof url, "%s/bake/osm/%d/%u/%u?tex=%d&v=" FB_OSM_STYLE_VER_S, fb_base, z, x, y, ts);
  uint8_t *enc = 0;
  size_t n = 0;
  if (!fb_get(url, &enc, &n)) return 0;
  int w = 0, h = 0, comp = 0;
  uint8_t *px = stbi_load_from_memory(enc, (int)n, &w, &h, &comp, 4);
  free(enc);
  if (!px) return 0;
  if (w != ts || h != ts) { stbi_image_free(px); return 0; }
  memcpy(dst, px, (size_t)ts * ts * 4);
  stbi_image_free(px);
  return 1;
}

int fb_terrain_load(const char *base, double lat, double lon, int z, int grid, fb_terrain *out) {
  if (!out) return 0;
  memset(out, 0, sizeof *out);
  snprintf(fb_base, sizeof fb_base, "%s", base ? base : "");

  uint32_t cx = 0, cy = 0;
  if (osmmesh_geo_to_tile(lon, lat, (uint8_t)z, &cx, &cy) != 0) {
    FlightBox::FBLog::Error("world", "geo_to_tile_failed", {{"lat", lat}, {"lon", lon}, {"z", z}});
    return 0;
  }

  osmmesh_config cfg = {};
  cfg.origin_lat = lat;
  cfg.origin_lon = lon;
  cfg.tile_provider = fb_provider;
  cfg.provider_terrain_max_zoom = 15;
  cfg.enable_terrain = 1;
  osmmesh_ctx *ctx = 0;
  if (osmmesh_create(&cfg, &ctx) != OSMMESH_OK || !ctx) {
    FlightBox::FBLog::Error("world", "osmmesh_create_failed");
    return 0;
  }

  const int TS = 512;   /* 512-bake this stage (ramp comes later) */
  const size_t layerBytes = (size_t)TS * TS * 4;
  out->albedo = (uint8_t *)malloc(layerBytes * FB_TERRAIN_MAX_TILES);
  if (!out->albedo) { osmmesh_destroy(ctx); return 0; }
  out->albedo_ts = TS;

  float *merged = 0;
  uint32_t total = 0;
  double ground = 0.0;
  int have_ground = 0;
  /* 4x4 field: the centre tile sits at (cx,cy); span -1..+2 so (lat,lon) lands near the middle. */
  for (int dy = -1; dy <= 2; dy++)
    for (int dx = -1; dx <= 2; dx++) {
      uint32_t tx = cx + (uint32_t)dx, ty = cy + (uint32_t)dy;
      osmmesh_tile t = {};
      if (osmmesh_fetch_tile(ctx, (uint8_t)z, tx, ty, &t) != OSMMESH_OK || !t.terrain) {
        FlightBox::FBLog::Debug("world", "tile_no_terrain", {{"z", z}, {"x", (int)tx}, {"y", (int)ty}});
        osmmesh_free_tile(&t);
        continue;
      }
      if (tx == cx && ty == cy) {
        float best = 1e30f;
        for (uint32_t i = 0; i < t.terrain->n_vertices; i++)
          if (t.terrain->positions[i * 3 + 2] < best) best = t.terrain->positions[i * 3 + 2];
        ground = (double)best;
        have_ground = 1;
      }
      w3_chunk chunk = {};
      double origin[3];
      int ok = w3_chunk_build_ecef(t.terrain, z, tx, ty, grid, &chunk, origin);
      osmmesh_free_tile(&t);
      if (!ok || chunk.nverts <= 0) {
        FlightBox::FBLog::Warn("world", "mesh_build_failed", {{"z", z}, {"x", (int)tx}, {"y", (int)ty}});
        w3_chunk_free(&chunk);
        continue;
      }
      int idx = out->ntiles;
      float *grow = (float *)realloc(merged, (size_t)(total + (uint32_t)chunk.nverts) * 8 * sizeof(float));
      if (!grow) { w3_chunk_free(&chunk); free(merged); free(out->albedo); out->albedo = 0; osmmesh_destroy(ctx); return 0; }
      merged = grow;
      memcpy(merged + (size_t)total * 8, chunk.verts, (size_t)chunk.nverts * 8 * sizeof(float));
      out->voff[idx] = total;
      out->vcnt[idx] = (uint32_t)chunk.nverts;
      out->origin[idx][0] = origin[0];
      out->origin[idx][1] = origin[1];
      out->origin[idx][2] = origin[2];
      total += (uint32_t)chunk.nverts;
      out->ntiles = idx + 1;
      FlightBox::FBLog::Debug("world", "tile_meshed", {{"z", z}, {"x", (int)tx}, {"y", (int)ty},
                                                       {"nverts", chunk.nverts}, {"errM", (double)chunk.err}});
      w3_chunk_free(&chunk);

      uint8_t *lay = out->albedo + (size_t)idx * layerBytes;
      if (fb_load_albedo(z, tx, ty, TS, lay))
        FlightBox::FBLog::Debug("world", "tile_albedo", {{"z", z}, {"x", (int)tx}, {"y", (int)ty},
                                                         {"layer", idx}, {"ts", TS}, {"src", "/bake/osm"}});
      else {
        fb_albedo_fallback(TS, lay);
        FlightBox::FBLog::Debug("world", "tile_albedo", {{"z", z}, {"x", (int)tx}, {"y", (int)ty},
                                                         {"layer", idx}, {"src", "fallback_checker"}});
      }
    }

  osmmesh_destroy(ctx);
  if (out->ntiles == 0 || total == 0) {
    free(merged);
    free(out->albedo);
    out->albedo = 0;
    return 0;
  }

  osmmesh_geo cg = {lon, lat, have_ground ? ground : 0.0};
  osmmesh_ecef ce = osmmesh_geo_to_ecef(cg);
  out->center[0] = ce.x;
  out->center[1] = ce.y;
  out->center[2] = ce.z;
  out->verts = merged;
  out->nverts = total;
  FlightBox::FBLog::Info("world", "terrain_loaded", {{"tiles", out->ntiles}, {"verts", (int)total},
                                                     {"groundM", have_ground ? ground : 0.0},
                                                     {"ecefX", ce.x}, {"ecefY", ce.y}, {"ecefZ", ce.z}});
  return 1;
}

void fb_terrain_free(fb_terrain *t) {
  if (!t) return;
  free(t->verts);
  free(t->albedo);
  t->verts = 0;
  t->albedo = 0;
  t->nverts = 0;
  t->ntiles = 0;
}

/* ============================================================================================
 * Streaming layer (Stage 4/7). Same fb-tiles endpoints; FBWorld polls per frame. The BYTE ACCESS is
 * platform-specific:
 *   WASM   — a JS-side async cache (EM_JS), NON-BLOCKING (mirrors tilesrc_js.h: 200 terminal, 204
 *            hole, anything else "ask again"), so nothing stalls the browser render loop.
 *   native — a blocking libcurl fetch behind a small in-memory cache (gpu_native is a CLI with its
 *            own thread of control; blocking a PNG-dump frame is fine).
 * The provider + fb_stream_* below are COMMON — only fbs_init/fbs_size/fbs_copy differ.
 * ========================================================================================== */
#ifdef __EMSCRIPTEN__
/* ---- WASM: EVERY blocking step (DEM/albedo fetch, stbi decode, osmmesh mesh, sRGB mips) runs in a
 * Web Worker (fbtw-worker.js + fbtileworker.wasm). The render thread only posts requests — camera-
 * priority, BASE tiles before the lazy OVERLAY — and polls finished results: whole vertex arrays +
 * whole mip pyramids, transferred zero-copy across postMessage. ---------------------------------- */

/* Worker POOL: N independent fbtileworker instances (each its own WASM module -> own osmmesh ctx +
 * DEM cache; the ASYNCIFY "one build in flight" rule holds PER instance, so N parallel builds are safe
 * — minor cross-instance cache redundancy accepted). One shared camera-priority queue (T.q) + dedup set
 * (T.req: a tile is queued/in-flight at most once, so no tile is built twice) + result map (T.done). pump
 * hands the best (base-prio, nearest) pending request to EVERY free worker. Poll fns + two-phase flip
 * (FBWorld) unchanged. N = min(hardwareConcurrency-2, 6), >=1. */
EM_JS(void, fbw_init, (const char *base, double lat, double lon), {
  var N = Math.max(1, Math.min(((navigator.hardwareConcurrency || 4) - 2), 6));
  var T = { workers: [], readyCount: 0, q: [], done: new Map(),
            req: new Set(), baseMode: 1, camLat: lat, camLon: lon };
  Module.__fbw = T;
  var baseStr = UTF8ToString(base);
  T.key = function (z, x, y, what, mode) { return z + '/' + x + '/' + y + '/' + what + '/' + mode; };
  T.dist = function (z, x, y) {                     /* tile-space distance^2 to the camera tile at z */
    var n = Math.pow(2, z);
    var cx = (T.camLon + 180) / 360 * n;
    var lr = T.camLat * Math.PI / 180;
    var cy = (1 - Math.log(Math.tan(lr) + 1 / Math.cos(lr)) / Math.PI) / 2 * n;
    var dx = x + 0.5 - cx, dy = y + 0.5 - cy;
    return dx * dx + dy * dy;
  };
  T.pump = function () {                            /* fill EVERY free worker; base (prio 0) nearest-first */
    for (var wi = 0; wi < T.workers.length && T.q.length > 0; wi++) {
      var W = T.workers[wi];
      if (!W.ready || W.busy) continue;
      var bi = 0, best = 1e30;
      for (var i = 0; i < T.q.length; i++) {
        var r = T.q[i], s = r.prio * 1e18 + r.dist;
        if (s < best) { best = s; bi = i; }
      }
      var req = T.q.splice(bi, 1)[0];
      W.busy = true;
      W.w.postMessage({ cmd: 'build', z: req.z, x: req.x, y: req.y, grid: req.grid, mode: req.mode, ts: req.ts, what: req.what });
    }
  };
  for (var i = 0; i < N; i++) {
    var W = { w: new Worker('fbtw-worker.js'), ready: false, busy: false };
    (function (W) {
      W.w.onmessage = function (e) {
        var d = e.data;
        if (d.cmd === 'opened') { W.ready = true; T.readyCount++; if (T.readyCount === 1) console.log('[tilepool] ' + N + ' workers'); T.pump(); return; }
        if (d.cmd === 'built') { W.busy = false; T.builtCount = (T.builtCount | 0) + 1; T.done.set(T.key(d.z, d.x, d.y, d.what, d.mode), d); T.pump(); }
      };
      W.w.postMessage({ cmd: 'open', base: baseStr, lat: lat, lon: lon });
    })(W);
    T.workers.push(W);
  }
})

EM_JS(void, fbw_set_base, (int mode), { if (Module.__fbw) Module.__fbw.baseMode = mode; })
EM_JS(void, fbw_campos, (double lat, double lon), {
  var T = Module.__fbw; if (T) { T.camLat = lat; T.camLon = lon; }
})

/* Poll the meshed tile. 1 + verts (malloc'd, caller frees) / nverts / origin / err when ready, else
 * queue a mesh request (priority 0) and return 0. */
EM_JS(int, fbw_mesh_poll, (int z, int x, int y, int grid, uint8_t **vptr, int *nv, double *origin, float *errp), {
  var T = Module.__fbw; if (!T) return 0;
  var k = T.key(z, x, y, 1, 0);
  var d = T.done.get(k);
  if (d) {
    T.done.delete(k);
    if (!(d.res & 1)) return 0;   /* mesh failed (missing DEM): keep `req` so it never re-queues -> no
                                     worker spin; the tile stays a gap (skirts cover it) like osmmesh */
    T.req.delete(k);
    var vb = new Uint8Array(d.verts);
    var p = _malloc(vb.length); HEAPU8.set(vb, p);
    HEAPU32[vptr >> 2] = p; HEAP32[nv >> 2] = d.nverts; HEAPF32[errp >> 2] = d.err;
    HEAPF64[origin >> 3] = d.origin[0]; HEAPF64[(origin >> 3) + 1] = d.origin[1]; HEAPF64[(origin >> 3) + 2] = d.origin[2];
    return 1;
  }
  if (!T.req.has(k)) { T.req.add(k); T.q.push({ z: z, x: x, y: y, grid: grid, mode: 0, ts: 0, what: 1, prio: 0, dist: T.dist(z, x, y) }); T.pump(); }
  return 0;
})

/* Poll the albedo mip pyramid for `mode` into dst. Bytes written (>0) when ready, 0 pending, -1 hole
 * (204). Base-mode requests are priority 0 (with the mesh); the overlay is priority 1. */
EM_JS(int, fbw_pyr_poll, (int z, int x, int y, int mode, int ts, uint8_t *dst), {
  var T = Module.__fbw; if (!T) return 0;
  var k = T.key(z, x, y, 2, mode);
  var d = T.done.get(k);
  if (d) {
    T.done.delete(k); T.req.delete(k);
    if (!(d.res & 2)) return -1;
    HEAPU8.set(new Uint8Array(d.mips), dst);
    return d.mipbytes;
  }
  if (!T.req.has(k)) {
    var prio = (mode === T.baseMode) ? 0 : 1;
    T.req.add(k); T.q.push({ z: z, x: x, y: y, grid: 0, mode: mode, ts: ts, what: 2, prio: prio, dist: T.dist(z, x, y) }); T.pump();
  }
  return 0;
})

int fb_stream_open(const char *base, double lat, double lon, int z) {
  (void)z;
  fbw_init(base, lat, lon);
  return 1;
}
void fb_stream_set_base(int mode) { fbw_set_base(mode); }
void fb_stream_campos(double lat, double lon) { fbw_campos(lat, lon); }

int fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                    double origin[3], float *err) {
  float e = 0.f;
  int ok = fbw_mesh_poll(z, (int)x, (int)y, grid, (uint8_t **)verts, nverts, origin, &e);
  if (ok && err) *err = e;
  return ok;
}

int fb_stream_pyramid(int z, uint32_t x, uint32_t y, int mode, int ts, uint8_t *dst) {
  return fbw_pyr_poll(z, (int)x, (int)y, mode, ts, dst);
}

/* Ground under the aircraft, ASYNC: kicks one /elev fetch when idle, returns the last resolved ASL
 * (-1e9 until the first lands). Mirrors jsbridge.h fb_ground_request/get, merged into one poll so the
 * render thread never holds a connection open (tiles/elev.c: cold = 503, warm = the DEM read). */
EM_JS(double, fbw_ground_poll, (double lat, double lon), {
  var G = Module.__fbG || (Module.__fbG = { val: -1e9, busy: false });
  if (!G.busy) {
    var base = window.FB_TILES_URL;
    if (base) {
      G.busy = true;
      fetch(base + '/elev?lat=' + lat + '&lon=' + lon)
        .then(function (r) { return r.ok ? r.text() : null; })
        .then(function (t) { if (t !== null) { var v = parseFloat(t); if (isFinite(v)) G.val = v; } G.busy = false; })
        .catch(function () { G.busy = false; });
    }
  }
  return G.val;
})
double fb_stream_ground(double lat, double lon) {
  while (lon > 180.0) lon -= 360.0;   /* normalize lon before the /elev query (dateline) */
  while (lon < -180.0) lon += 360.0;
  double v = fbw_ground_poll(lat, lon);
  /* Clamp REAL samples to sea level: open-ocean bathymetry is negative (ETOPO seafloor), but the water
   * surface — and the aircraft's ground reference / JSBSim floor — is 0, not the seabed. Leave the
   * -1e9 "not-yet" sentinel untouched so callers still know a sample hasn't landed. */
  return v < -1e8 ? v : (v < 0.0 ? 0.0 : v);
}

/* WASM: async main-thread DEM-tile cache (mirrors fbw_lights_poll). Non-blocking — kicks a fetch on a
 * miss and returns 0 (pending) until the bytes land, then copies them into a WASM buffer. FBTerrainField
 * decodes immediately, so a single static buffer is safe. Contract: 1 = bytes ready (buf/len set),
 * 0 = pending (retry — do NOT cache as a hole), -1 = real hole/error. */
EM_JS(int, fbw_dem_poll, (int z, int x, int y, uint8_t *dst, int cap), {
  var D = Module.__fbD || (Module.__fbD = { done: new Map(), req: new Set(), inflight: 0 });
  var k = z + '/' + x + '/' + y;
  if (D.done.has(k)) {
    var b = D.done.get(k);
    if (b === null || b.length > cap) return -1;
    HEAPU8.set(b, dst);
    return b.length;
  }
  if (!D.req.has(k) && D.inflight < 4) {
    var base = window.FB_TILES_URL;
    if (!base) return 0;
    D.req.add(k); D.inflight++;
    fetch(base + '/t/terrain/' + z + '/' + x + '/' + y)
      .then(function (r) { return r.ok ? r.arrayBuffer() : null; })
      .then(function (ab) { D.done.set(k, ab ? new Uint8Array(ab) : null); D.inflight--; })
      .catch(function () { D.done.set(k, null); D.inflight--; });
  }
  return 0;
})
int fb_stream_dem(int z, int x, int y, const uint8_t **bytes, int *len) {
  static uint8_t buf[262144];   /* a z12/13 Terrarium PNG is ~50-160 KB */
  int n = fbw_dem_poll(z, x, y, buf, (int)sizeof buf);
  if (n <= 0) { *bytes = 0; *len = 0; return n; }   /* 0 pending, -1 hole */
  *bytes = buf; *len = n;
  return 1;
}

/* Night-light tile, ASYNC main-thread fetch (lowest priority — the caller only polls a few per frame).
 * In-flight-capped so it never floods the connection or stalls the render loop. Cached per tile key. */
EM_JS(int, fbw_lights_poll, (int z, int x, int y, uint8_t *dst, int cap), {
  var L = Module.__fbL || (Module.__fbL = { done: new Map(), req: new Set(), inflight: 0 });
  var k = z + '/' + x + '/' + y;
  if (L.done.has(k)) {
    var b = L.done.get(k);
    if (b === null || b.length > cap) return -1;   /* fetched: none/error, or too big to fit */
    HEAPU8.set(b, dst);
    return b.length;
  }
  if (!L.req.has(k) && L.inflight < 3) {
    var base = window.FB_TILES_URL;
    if (!base) { return 0; }
    L.req.add(k); L.inflight++;
    fetch(base + '/t/lights/' + z + '/' + x + '/' + y)
      .then(function (r) { return r.ok ? r.arrayBuffer() : null; })
      .then(function (ab) { L.done.set(k, ab ? new Uint8Array(ab) : null); L.inflight--; })
      .catch(function () { L.done.set(k, null); L.inflight--; });
  }
  return 0;
})
int fb_stream_lights(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap) {
  return fbw_lights_poll(z, (int)x, (int)y, dst, cap);
}

void fb_stream_close(void) {}

#else /* native: synchronous fetch + mesh + decode + mips, all on the CLI's own thread (may block) */

struct fbs_ent { char path[192]; uint8_t *data; int len; };   /* len 0 = hole; data 0 iff len 0 */
static struct fbs_ent fbs_cache[2048];
static int fbs_cache_n = 0, fbs_cache_head = 0;
static osmmesh_ctx *fbs_ctx = 0;

/* [tileperf] — per-stage cold-boot instrumentation (FB_TILEPERF=1). Times the SHARED pipeline the
 * browser worker wraps: DEM fetch+decode (osmmesh_fetch_tile), mesh (w3_chunk_build_ecef), albedo
 * fetch (fbs_size), albedo decode (stbi), mip pyramid (fb_build_pyramid). A running summary prints
 * every 32 pyramids + at close; the last line before convergence is the cold-boot total. Zero cost
 * when off (one cached env check). */
#include <time.h>
static int fbtp_on_ = -1;
static inline int fbtp(void) {
  if (fbtp_on_ < 0) { const char *e = getenv("FB_TILEPERF"); fbtp_on_ = (e && atoi(e)) ? 1 : 0; }
  return fbtp_on_;
}
static inline double fbtp_ms(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
}
static double fbtp_t0_ = 0;
static struct { double demfetch, mesh, albfetch, decode, mips, provfetch; long nmesh, npyr, holes, provcalls, provkind[3]; } fbtp_;
static void fbtp_report(void) {
  if (!fbtp()) return;
  double wall = fbtp_ms() - fbtp_t0_;
  long m = fbtp_.nmesh ? fbtp_.nmesh : 1, p = fbtp_.npyr ? fbtp_.npyr : 1;
  double demInternal = fbtp_.demfetch - fbtp_.provfetch;   /* osmmesh work minus the HTTP it triggered */
  FlightBox::FBLog::Debug("world", "tileperf",
      {{"wallMs", wall}, {"meshTiles", (int)fbtp_.nmesh}, {"pyrTiles", (int)fbtp_.npyr}, {"holes", (int)fbtp_.holes},
       {"demFetchMs", fbtp_.demfetch}, {"demFetchMsPerTile", fbtp_.demfetch / m},
       {"provHttpMs", fbtp_.provfetch}, {"provHttpMsPerTile", fbtp_.provfetch / m}, {"provCalls", (int)fbtp_.provcalls},
       {"provVec", (int)fbtp_.provkind[0]}, {"provTer", (int)fbtp_.provkind[1]}, {"provImg", (int)fbtp_.provkind[2]},
       {"osmInternalMs", demInternal}, {"osmInternalMsPerTile", demInternal / m},
       {"meshMs", fbtp_.mesh}, {"meshMsPerTile", fbtp_.mesh / m},
       {"albFetchMs", fbtp_.albfetch}, {"albFetchMsPerTile", fbtp_.albfetch / p},
       {"decodeMs", fbtp_.decode}, {"decodeMsPerTile", fbtp_.decode / p},
       {"mipsMs", fbtp_.mips}, {"mipsMsPerTile", fbtp_.mips / p},
       {"cpuSumMs", fbtp_.demfetch + fbtp_.mesh + fbtp_.albfetch + fbtp_.decode + fbtp_.mips}});
}

static void fbs_init(const char *base) {
  snprintf(fb_base, sizeof fb_base, "%s", base ? base : "");
  for (int i = 0; i < fbs_cache_n; i++) { free(fbs_cache[i].data); fbs_cache[i].data = 0; }
  fbs_cache_n = 0;
  fbs_cache_head = 0;
  if (fbtp()) { fbtp_t0_ = fbtp_ms(); memset(&fbtp_, 0, sizeof fbtp_); }
}

static struct fbs_ent *fbs_find(const char *path) {
  for (int i = 0; i < fbs_cache_n; i++)
    if (strcmp(fbs_cache[i].path, path) == 0) return &fbs_cache[i];
  return 0;
}

static int fbs_size(const char *path) {
  struct fbs_ent *e = fbs_find(path);
  if (e) return e->len;
  char url[256];
  snprintf(url, sizeof url, "%s%s", fb_base, path);
  uint8_t *buf = 0;
  size_t n = 0;
  int ok = fb_get(url, &buf, &n);
  const int cap = (int)(sizeof fbs_cache / sizeof *fbs_cache);
  if (fbs_cache_n < cap) {
    e = &fbs_cache[fbs_cache_n++];
  } else {
    e = &fbs_cache[fbs_cache_head];
    fbs_cache_head = (fbs_cache_head + 1) % cap;
    free(e->data);
  }
  snprintf(e->path, sizeof e->path, "%s", path);
  e->data = ok ? buf : 0;
  e->len = (ok && n > 0) ? (int)n : 0;
  if (!ok) free(buf);
  return e->len;
}

static void fbs_copy(const char *path, uint8_t *dst) {
  struct fbs_ent *e = fbs_find(path);
  if (e && e->data && e->len > 0) memcpy(dst, e->data, (size_t)e->len);
}

static int fbs_provider(void *user, osmmesh_tile_kind kind, uint32_t z, uint32_t x, uint32_t y,
                        uint8_t **out, size_t *len) {
  (void)user;
  static const char *names[] = {"vector", "terrain", "imagery"};
  if ((int)kind < 0 || (int)kind >= 3) return 0;
  char path[128];
  snprintf(path, sizeof path, "/t/%s/%u/%u/%u", names[kind], z, x, y);
  double tpf = fbtp() ? fbtp_ms() : 0;
  int n = fbs_size(path);
  if (fbtp()) { fbtp_.provfetch += fbtp_ms() - tpf; fbtp_.provcalls++; if ((int)kind >= 0 && (int)kind < 3) fbtp_.provkind[(int)kind]++; }
  if (n <= 0) return 0;
  uint8_t *b = (uint8_t *)malloc((size_t)n);
  if (!b) return 0;
  fbs_copy(path, b);
  *out = b;
  *len = (size_t)n;
  return 1;
}

int fb_stream_open(const char *base, double lat, double lon, int z) {
  (void)z;
  fbs_init(base);
  osmmesh_config cfg = {};
  cfg.origin_lat = lat;
  cfg.origin_lon = lon;
  cfg.tile_provider = fbs_provider;
  cfg.provider_terrain_max_zoom = 15;
  cfg.enable_terrain = 1;
  if (osmmesh_create(&cfg, &fbs_ctx) != OSMMESH_OK) { fbs_ctx = 0; return 0; }
  return 1;
}
void fb_stream_set_base(int mode) { (void)mode; }              /* native has no priority queue */
void fb_stream_campos(double lat, double lon) { (void)lat; (void)lon; }

int fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                    double origin[3], float *err) {
  if (!fbs_ctx) return 0;
  osmmesh_tile t = {};
  double ta = fbtp() ? fbtp_ms() : 0;
  int fetched = osmmesh_fetch_tile(fbs_ctx, (uint8_t)z, x, y, &t);
  if (fbtp()) fbtp_.demfetch += fbtp_ms() - ta;
  if (fetched != OSMMESH_OK || !t.terrain) {
    osmmesh_free_tile(&t);
    return 0;
  }
  w3_chunk chunk = {};
  double o[3];
  double tm = fbtp() ? fbtp_ms() : 0;
  int ok = w3_chunk_build_ecef(t.terrain, z, x, y, grid, &chunk, o);
  if (fbtp()) { fbtp_.mesh += fbtp_ms() - tm; fbtp_.nmesh++; }
  osmmesh_free_tile(&t);
  if (!ok || chunk.nverts <= 0) { w3_chunk_free(&chunk); return 0; }
  *verts = (float *)chunk.verts;
  *nverts = chunk.nverts;
  origin[0] = o[0];
  origin[1] = o[1];
  origin[2] = o[2];
  if (err) *err = chunk.err;
  return 1;
}

/* Albedo mip pyramid for (z,x,y) in `mode` (0 osm, 1 photo) into dst (fb_pyramid_bytes(ts) long).
 * Fetch + decode + build the same sRGB pyramid the worker builds (fb_mips.h — ONE source, so the
 * oracle and the browser render identical pixels). Bytes (>0) resident, 0 pending, -1 hole. */
int fb_stream_pyramid(int z, uint32_t x, uint32_t y, int mode, int ts, uint8_t *dst) {
  char path[160];
  snprintf(path, sizeof path, mode ? "/bake/photo/%d/%u/%u?tex=%d" : "/bake/osm/%d/%u/%u?tex=%d&v=" FB_OSM_STYLE_VER_S,
           z, x, y, ts);
  double tf = fbtp() ? fbtp_ms() : 0;
  int n = fbs_size(path);
  if (fbtp()) fbtp_.albfetch += fbtp_ms() - tf;
  if (n < 0) return 0;
  if (n == 0) { if (fbtp()) fbtp_.holes++; return -1; }
  uint8_t *enc = (uint8_t *)malloc((size_t)n);
  if (!enc) return 0;
  fbs_copy(path, enc);
  int w = 0, h = 0, comp = 0;
  double td = fbtp() ? fbtp_ms() : 0;
  uint8_t *px = stbi_load_from_memory(enc, n, &w, &h, &comp, 4);
  if (fbtp()) fbtp_.decode += fbtp_ms() - td;
  free(enc);
  if (!px) return 0;
  if (w != ts || h != ts) { stbi_image_free(px); return 0; }
  double tp = fbtp() ? fbtp_ms() : 0;
  fb_build_pyramid(px, ts, dst);
  if (fbtp()) { fbtp_.mips += fbtp_ms() - tp; if (++fbtp_.npyr % 32 == 0) fbtp_report(); }
  stbi_image_free(px);
  return fb_pyramid_bytes(ts);
}

/* Native /elev: synchronous curl (block=1 waits for a cold origin DEM), cached per ~tile-cell (≈33 m)
 * so a moving aircraft only refetches after appreciable travel — a static camera fetches once. */
double fb_stream_ground(double lat, double lon) {
  static double clat = 1e9, clon = 1e9, cval = -1e9;
  while (lon > 180.0) lon -= 360.0;   /* normalize lon before the /elev query (dateline) */
  while (lon < -180.0) lon += 360.0;
  double dlat = lat - clat, dlon = lon - clon;
  if (dlat < 0) dlat = -dlat;
  if (dlon < 0) dlon = -dlon;
  if (dlat < 3.0e-4 && dlon < 4.5e-4 && cval > -1e8) return cval;
  char url[320];
  snprintf(url, sizeof url, "%s/elev?lat=%.6f&lon=%.6f&block=1", fb_base, lat, lon);
  uint8_t *buf = 0;
  size_t n = 0;
  if (fb_get(url, &buf, &n) && buf) {
    char tmp[64];
    size_t k = n < sizeof tmp - 1 ? n : sizeof tmp - 1;
    memcpy(tmp, buf, k);
    tmp[k] = 0;
    free(buf);
    double v = atof(tmp);
    /* Clamp REAL samples to sea level: open-ocean bathymetry is negative (ETOPO seabed), but the water
     * surface — the ground reference + JSBSim floor — is 0. Sentinel -1e9 stays untouched. */
    if (v > -1e8) { if (v < 0.0) v = 0.0; clat = lat; clon = lon; cval = v; return v; }
  } else if (buf) {
    free(buf);
  }
  return cval;
}

/* Native: raw Terrarium DEM tile bytes behind the byte cache (fbs_size caches by path). The pointer is
 * into the cache — valid until evicted; FBTerrainField decodes immediately and never holds it. */
int fb_stream_dem(int z, int x, int y, const uint8_t **bytes, int *len) {
  char path[96];
  snprintf(path, sizeof path, "/t/terrain/%d/%d/%d", z, x, y);
  int n = fbs_size(path);   /* synchronous: never pending -> bytes or a real hole */
  struct fbs_ent *e = n > 0 ? fbs_find(path) : 0;
  if (!e || !e->data) { *bytes = 0; *len = 0; return -1; }
  *bytes = e->data; *len = e->len;
  return 1;
}

/* Native night-light tile: synchronous fetch behind the byte cache (fbs_size caches by path). Returns
 * bytes (>=4 incl. the count header even when empty), 0 on miss/too-big. */
int fb_stream_lights(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap) {
  char path[96];
  snprintf(path, sizeof path, "/t/lights/%d/%u/%u", z, x, y);
  int n = fbs_size(path);
  if (n <= 0 || n > cap) return 0;
  fbs_copy(path, dst);
  return n;
}

void fb_stream_close(void) {
  fbtp_report();
  if (fbs_ctx) { osmmesh_destroy(fbs_ctx); fbs_ctx = 0; }
}

#endif /* __EMSCRIPTEN__ */

int fb_load_image_file(const char *path, uint8_t **rgba, int *w, int *h) {
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) { fclose(f); return 0; }
  uint8_t *enc = (uint8_t *)malloc((size_t)n);
  if (!enc) { fclose(f); return 0; }
  size_t rd = fread(enc, 1, (size_t)n, f);
  fclose(f);
  if ((long)rd != n) { free(enc); return 0; }
  int comp = 0;
  uint8_t *px = stbi_load_from_memory(enc, (int)n, w, h, &comp, 4);
  free(enc);
  if (!px) return 0;
  size_t bytes = (size_t)(*w) * (*h) * 4;
  uint8_t *out = (uint8_t *)malloc(bytes);   /* hand back a plain malloc buffer (caller free()s) */
  if (!out) { stbi_image_free(px); return 0; }
  memcpy(out, px, bytes);
  stbi_image_free(px);
  *rgba = out;
  return 1;
}

int fb_fetch_stars(const char *base, uint8_t *dst, int cap) {
  int off = 0;
  for (int band = 0; band < 4; band++) {   /* fb-tiles serves 4 HYG mag bands (0..3); no 5th to 404 on */
    char url[256];
    snprintf(url, sizeof url, "%s/t/stars/%d/0/0", base, band);
    uint8_t *buf = 0;
    size_t n = 0;
    if (!fb_get(url, &buf, &n)) break;
    if (off + (int)n > cap) { free(buf); break; }
    memcpy(dst + off, buf, n);
    off += (int)n;
    free(buf);
  }
  return off;
}

int fb_fetch_text(const char *url, char *dst, int cap) {
  uint8_t *buf = 0;
  size_t n = 0;
  if (!fb_get(url, &buf, &n)) return 0;
  int wrote = (int)(n < (size_t)(cap - 1) ? n : (size_t)(cap - 1));
  memcpy(dst, buf, (size_t)wrote);
  dst[wrote] = '\0';
  free(buf);
  return wrote;
}
