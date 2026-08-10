/* The tile-streaming C ABI: fb-tiles bytes -> osmmesh -> camera-relative ECEF meshes + albedo mip
 * meshes, polled per pass by World. */
#include "TerrainLoader.h"
#include "Mips.h"
#include "style_ver.h"
#include "ChunkMesh.h"
#include "ClusterDag.h"
#include "geo.h"
#include "osmmesh.h"
#include "terrain.h"
#include "tilemath.h"   /* fb-tiles' OWN tile maths: /elev and this oracle must not drift apart */
#include "Log.h"
#include "StackProbe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <curl/curl.h>
#include <unistd.h>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#endif

/* A C ABI cannot live in a namespace, but the types it borrows do — see the C-ISLAND note in
 * tools/verify_layers.py. */
using namespace outshine;

/* stb_image's implementation lives in terrain.cpp — declared here, never re-implemented: two
 * implementations in one link would collide. */

#ifdef __EMSCRIPTEN__
/* emscripten_fetch's SYNCHRONOUS mode is Web-Worker-only (NULL on a page's main thread), so the
 * main-thread primitive is a blocking XHR; binary arrives via the x-user-defined charset trick. */
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

/* 202/404/5xx = queued or transient (retry), 204 = a real hole, only 200 hands over bytes. */
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
/* Blocking libcurl: gpu_native is a CLI with its own thread of control, no event loop to yield to.
 * Same retry contract as the WASM path above. */
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

/* The streaming layer. fb_stream_* is COMMON to both platforms; only the three byte primitives
 * fbs_init/fbs_size/fbs_copy differ — WASM a non-blocking JS async cache, native blocking
 * libcurl. */
#ifdef __EMSCRIPTEN__
/* WASM: every blocking step runs in a worker pool; the render thread only posts requests and polls
 * finished results (whole vertex arrays, zero-copy across postMessage). The ASYNCIFY
 * "one build in flight" rule holds PER worker instance, so N parallel builds are safe. */
EM_JS(void, fbw_init, (const char *base, double lat, double lon), {
  var N = Math.max(1, Math.min(((navigator.hardwareConcurrency || 4) - 2), 6));
  var T = { workers: [], readyCount: 0, q: [], done: new Map(),
            req: new Set(), camLat: lat, camLon: lon };
  Module.__fbw = T;
  var baseStr = UTF8ToString(base);
  T.key = function (z, x, y, what) { return z + '/' + x + '/' + y + '/' + what; };
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
      if (req.what === 4) { W.w.postMessage({ cmd: 'dag', id: req.id, soup: req.soup, nverts: req.nverts, seam: req.seam }, [req.soup]); continue; }
      W.w.postMessage({ cmd: 'build', z: req.z, x: req.x, y: req.y, grid: req.grid });
    }
  };
  for (var i = 0; i < N; i++) {
    var W = { w: new Worker('fbtw-worker.js'), ready: false, busy: false };
    (function (W) {
      W.w.onmessage = function (e) {
        var d = e.data;
        if (d.cmd === 'opened') { W.ready = true; T.readyCount++; if (T.readyCount === 1) console.log('[tilepool] ' + N + ' workers'); T.pump(); return; }
        if (d.cmd === 'built') { W.busy = false; T.builtCount = (T.builtCount | 0) + 1; T.done.set(T.key(d.z, d.x, d.y, 1), d); T.pump(); return; }
        if (d.cmd === 'dagged') { W.busy = false; T.done.set('dag/' + d.id, d); T.pump(); }
      };
      W.w.postMessage({ cmd: 'open', base: baseStr, lat: lat, lon: lon });
    })(W);
    T.workers.push(W);
  }
})

EM_JS(void, fbw_campos, (double lat, double lon), {
  var T = Module.__fbw; if (T) { T.camLat = lat; T.camLon = lon; }
})

/* 1 + the DAG (verts and clusters, both malloc'd, caller frees) when ready, else queue a mesh
 * request and return 0. */
EM_JS(int, fbw_mesh_poll, (int z, int x, int y, int grid, uint8_t **vptr, int *nv, uint8_t **iptr, int *ni, uint8_t **cptr, int *nc, double *origin, float *errp), {
  var T = Module.__fbw; if (!T) return 0;
  var k = T.key(z, x, y, 1);
  var d = T.done.get(k);
  if (d) {
    T.done.delete(k);
    if (!(d.res & 1)) return 0;   /* mesh failed (missing DEM): keep `req` so it never re-queues -> no
                                     worker spin; the tile stays a gap (skirts cover it) like osmmesh */
    T.req.delete(k);
    var vb = new Uint8Array(d.verts);
    var p = _malloc(vb.length); HEAPU8.set(vb, p);
    var ib = new Uint8Array(d.idx);
    var r = _malloc(ib.length); HEAPU8.set(ib, r);
    var cb = new Uint8Array(d.clusters);
    var q = _malloc(cb.length); HEAPU8.set(cb, q);
    HEAPU32[vptr >> 2] = p; HEAP32[nv >> 2] = d.nverts;
    HEAPU32[iptr >> 2] = r; HEAP32[ni >> 2] = d.nidx;
    HEAPU32[cptr >> 2] = q; HEAP32[nc >> 2] = d.nclusters;
    HEAPF32[errp >> 2] = d.err;
    HEAPF64[origin >> 3] = d.origin[0]; HEAPF64[(origin >> 3) + 1] = d.origin[1]; HEAPF64[(origin >> 3) + 2] = d.origin[2];
    return 1;
  }
  if (!T.req.has(k)) { T.req.add(k); T.q.push({ z: z, x: x, y: y, grid: grid, what: 1, prio: 0, dist: T.dist(z, x, y) }); T.pump(); }
  return 0;
})

int fb_stream_open(const char *base, double lat, double lon, int z) {
  (void)z;
  fbw_init(base, lat, lon);
  return 1;
}
void fb_stream_campos(double lat, double lon) { fbw_campos(lat, lon); }

int fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                    uint32_t **idx, int *nidx,
                    outshine::Render::DagCluster **clusters, int *nclusters,
                    double origin[3], float *err) {
  float e = 0.f;
  int ok = fbw_mesh_poll(z, (int)x, (int)y, grid, (uint8_t **)verts, nverts,
                         (uint8_t **)idx, nidx,
                         (uint8_t **)clusters, nclusters, origin, &e);
  if (ok && err) *err = e;
  return ok;
}

/* Priority -1: the soup is already decoded and the frame it belongs to is waiting on it. */
EM_JS(int, fbw_dag_poll, (int id, const float *soup, int nverts, int seamAttr,
                          uint8_t **vptr, int *nv, uint8_t **iptr, int *ni, uint8_t **cptr, int *nc), {
  var T = Module.__fbw; if (!T) return 0;
  var k = 'dag/' + id;
  var d = T.done.get(k);
  if (d) {
    T.done.delete(k); T.req.delete(k);
    if (!d.res) return 0;
    var vb = new Uint8Array(d.verts);
    var p = _malloc(vb.length); HEAPU8.set(vb, p);
    var ib = new Uint8Array(d.idx);
    var r = _malloc(ib.length); HEAPU8.set(ib, r);
    var cb = new Uint8Array(d.clusters);
    var q = _malloc(cb.length); HEAPU8.set(cb, q);
    HEAPU32[vptr >> 2] = p; HEAP32[nv >> 2] = d.nverts;
    HEAPU32[iptr >> 2] = r; HEAP32[ni >> 2] = d.nidx;
    HEAPU32[cptr >> 2] = q; HEAP32[nc >> 2] = d.nclusters;
    return 1;
  }
  if (!T.req.has(k)) {
    T.req.add(k);
    var soupCopy = HEAPU8.slice(soup, soup + nverts * 8 * 4).buffer;
    T.q.push({ what: 4, id: id, soup: soupCopy, nverts: nverts, seam: seamAttr, prio: -1, dist: 0 });
    T.pump();
  }
  return 0;
})

int fb_stream_dag(int id, const float *soup, int nverts, int seamAttr, float **verts, int *nverts_out,
                  uint32_t **idx, int *nidx,
                  outshine::Render::DagCluster **clusters, int *nclusters) {
  return fbw_dag_poll(id, soup, nverts, seamAttr, (uint8_t **)verts, nverts_out,
                      (uint8_t **)idx, nidx, (uint8_t **)clusters, nclusters);
}

/* Non-blocking DEM-tile cache. TerrainField decodes immediately, so a single static buffer is safe.
 * 0 = pending; do NOT cache that as a hole. */
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

/* Lowest priority and in-flight-capped, so it never floods the connection or stalls the render loop. */
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

/* The tile-provider cache already fetches /t/vector for the terrain path, so the vector tile rides
 * the SAME async cache the mesh does instead of opening a second one. */
EM_JS(int, fbw_vector_poll, (int z, int x, int y, uint8_t *dst, int cap), {
  var V = Module.__fbV || (Module.__fbV = { done: new Map(), req: new Set(), inflight: 0 });
  var k = z + '/' + x + '/' + y;
  if (V.done.has(k)) {
    var b = V.done.get(k);
    if (b === null || b.length > cap) return -1;
    HEAPU8.set(b, dst);
    return b.length;
  }
  if (!V.req.has(k) && V.inflight < 3) {
    var base = window.FB_TILES_URL;
    if (!base) { return 0; }
    V.req.add(k); V.inflight++;
    fetch(base + '/t/vector/' + z + '/' + x + '/' + y)
      .then(function (r) { return r.ok ? r.arrayBuffer() : null; })
      .then(function (ab) { V.done.set(k, ab ? new Uint8Array(ab) : null); V.inflight--; })
      .catch(function () { V.done.set(k, null); V.inflight--; });
  }
  return 0;
})
int fb_stream_vector(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap) {
  return fbw_vector_poll(z, (int)x, (int)y, dst, cap);
}

void fb_stream_close(void) {}

EM_JS(double, fbw_cache_bytes, (), {
  var total = 0;
  ['__fbD', '__fbL', '__fbV'].forEach(function (name) {
    var C = Module[name];
    if (!C || !C.done) return;
    C.done.forEach(function (b) { if (b) total += b.byteLength; });
  });
  return total;
})
double fb_stream_cache_bytes(void) { return fbw_cache_bytes(); }

#else /* native: a worker-thread pool with the same contract the browser pool has */

static char fb_base[160] = "";

struct fbs_ent { char path[192]; uint8_t *data; int len; };   /* len 0 = hole; data 0 iff len 0 */
static struct fbs_ent fbs_cache[2048];
static int fbs_cache_n = 0, fbs_cache_head = 0;

/* [tileperf] (FB_TILEPERF=1): per-stage cold-boot timings of the SHARED pipeline the browser worker
 * wraps. Zero cost when off — one cached env check. */
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
static struct { double demfetch, mesh, provfetch; long nmesh, holes, provcalls, provkind[3]; } fbtp_;
static void fbtp_report(void) {
  if (!fbtp()) return;
  double wall = fbtp_ms() - fbtp_t0_;
  long m = fbtp_.nmesh ? fbtp_.nmesh : 1;
  double demInternal = fbtp_.demfetch - fbtp_.provfetch;   /* osmmesh work minus the HTTP it triggered */
  outshine::Log::Debug("world", "tileperf",
      {{"wallMs", wall}, {"meshTiles", (int)fbtp_.nmesh}, {"holes", (int)fbtp_.holes},
       {"demFetchMs", fbtp_.demfetch}, {"demFetchMsPerTile", fbtp_.demfetch / m},
       {"provHttpMs", fbtp_.provfetch}, {"provHttpMsPerTile", fbtp_.provfetch / m}, {"provCalls", (int)fbtp_.provcalls},
       {"provVec", (int)fbtp_.provkind[0]}, {"provTer", (int)fbtp_.provkind[1]}, {"provImg", (int)fbtp_.provkind[2]},
       {"osmInternalMs", demInternal}, {"osmInternalMsPerTile", demInternal / m},
       {"meshMs", fbtp_.mesh}, {"meshMsPerTile", fbtp_.mesh / m},       {"cpuSumMs", fbtp_.demfetch + fbtp_.mesh}});
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

/* ---- the tile worker pool ------------------------------------------------------------------------
 *
 * The SAME SHAPE the browser has and for the same measured reason: one
 * tile is 10.7 ms of work — DEM fetch 2.00, Terrarium decode + stitch 2.79, mesh 0.11, cluster DAG
 * 3.68 (FB_TILEPERF=1 over 128 tiles) — and a frame that pays it has
 * already lost. `fb_stream_build` posts and polls; nothing here blocks the caller.
 *
 * EACH WORKER OWNS ITS OWN osmmesh_ctx. The context carries a DEM LRU and is written by the tile it
 * is building, so sharing one would need a lock around the whole build and the pool would be a
 * queue. The byte cache below IS shared — it only ever hands out COPIES, so a reader cannot be
 * holding a pointer when an eviction frees it. */
static const char *const fbp_kind_name[3] = {"vector", "terrain", "imagery"};

namespace {

struct FbpEnt { std::string Path; std::vector<uint8_t> Data; bool Hole = false; };

std::mutex fbp_cache_mu;
std::vector<FbpEnt> fbp_cache;
size_t fbp_cache_head = 0;
const size_t kFbpCacheCap = 2048;

/* 1 + a fresh copy, 0 = no bytes there. The lock is held for the lookup and for the insert, never
 * across the HTTP: two workers asking for the same path at the same moment fetch it twice, which
 * costs one request and keeps every worker off the others' critical path. */
int FbpFetch(const char *path, uint8_t **out, size_t *len) {
  {
    std::lock_guard<std::mutex> lk(fbp_cache_mu);
    for (const FbpEnt &e : fbp_cache)
      if (e.Path == path) {
        if (e.Hole || e.Data.empty()) return 0;
        uint8_t *b = (uint8_t *)malloc(e.Data.size());
        if (!b) return 0;
        memcpy(b, e.Data.data(), e.Data.size());
        *out = b;
        *len = e.Data.size();
        return 1;
      }
  }
  char url[256];
  snprintf(url, sizeof url, "%s%s", fb_base, path);
  uint8_t *buf = 0;
  size_t n = 0;
  const int ok = fb_get(url, &buf, &n);
  {
    std::lock_guard<std::mutex> lk(fbp_cache_mu);
    FbpEnt *e;
    if (fbp_cache.size() < kFbpCacheCap) { fbp_cache.emplace_back(); e = &fbp_cache.back(); }
    else { e = &fbp_cache[fbp_cache_head]; fbp_cache_head = (fbp_cache_head + 1) % kFbpCacheCap; }
    e->Path = path;
    e->Hole = !ok || n == 0;
    if (e->Hole) e->Data.clear();
    else e->Data.assign(buf, buf + n);
  }
  if (!ok || n == 0) { free(buf); return 0; }
  *out = buf;
  *len = n;
  return 1;
}

struct FbpStats { double DemFetch = 0, Mesh = 0, ProvFetch = 0;
                  long ProvCalls = 0, ProvKind[3] = {0, 0, 0}; };

struct FbpJob {
  int Kind = 0;                   /* 1 = mesh + DAG, 3 = the DAG of a given soup */
  int Z = 0, Grid = 0, SeamAttr = -1;
  uint32_t X = 0, Y = 0;
  uint64_t Key = 0;
  double Dist = 0.0;              /* tile-space distance^2 to the camera, the pump's order */
  std::vector<float> Soup;
};

struct FbpResult {
  bool Ok = false;
  float *Verts = nullptr;
  int NVerts = 0;
  uint32_t *Idx = nullptr;
  int NIdx = 0;
  outshine::Render::DagCluster *Clusters = nullptr;
  int NClusters = 0;
  double Origin[3] = {0, 0, 0};
  float Err = 0.0f;
  int TS = 0;
};

uint64_t FbpKey(int kind, int z, uint32_t x, uint32_t y) {
  return ((uint64_t)(kind & 3) << 62) | ((uint64_t)(z & 31) << 56)
       | ((uint64_t)(x & 0xFFFFFFFu) << 28) | (uint64_t)(y & 0xFFFFFFFu);
}

std::mutex fbp_mu;
std::condition_variable fbp_cv;
std::vector<FbpJob> fbp_queue;
std::map<uint64_t, FbpResult> fbp_done;
std::set<uint64_t> fbp_asked;      /* posted and not yet collected — no job is queued twice */
std::vector<std::thread> fbp_threads;
bool fbp_stop = false;
double fbp_lat = 0.0, fbp_lon = 0.0;
double fbp_cam_lat = 0.0, fbp_cam_lon = 0.0;

double FbpTileDist(int z, uint32_t x, uint32_t y) {
  const double n = std::ldexp(1.0, z);
  const double cx = (fbp_cam_lon + 180.0) / 360.0 * n;
  const double lr = fbp_cam_lat * 3.14159265358979 / 180.0;
  const double cy = (1.0 - std::asinh(std::tan(lr)) / 3.14159265358979) * 0.5 * n;
  const double dx = (double)x + 0.5 - cx, dy = (double)y + 0.5 - cy;
  return dx * dx + dy * dy;
}

int FbpProvider(void *user, osmmesh_tile_kind kind, uint32_t z, uint32_t x, uint32_t y,
                uint8_t **out, size_t *len) {
  if ((int)kind < 0 || (int)kind >= 3) return 0;
  char path[128];
  snprintf(path, sizeof path, "/t/%s/%u/%u/%u", fbp_kind_name[(int)kind], z, x, y);
  FbpStats *st = (FbpStats *)user;
  const double t0 = fbtp() ? fbtp_ms() : 0;
  const int got = FbpFetch(path, out, len);
  if (fbtp() && st) { st->ProvFetch += fbtp_ms() - t0; st->ProvCalls++; st->ProvKind[(int)kind]++; }
  return got;
}

/* Three malloc'd arrays or none: a partial result is a cluster list that describes a buffer the
 * caller does not have. */
bool FbpPublish(FbpResult *r, const std::vector<float> &dv, const std::vector<uint32_t> &di,
                const std::vector<outshine::Render::DagCluster> &dc) {
  r->Verts = (float *)malloc(dv.size() * sizeof(float));
  r->Idx = (uint32_t *)malloc(di.size() * sizeof(uint32_t));
  r->Clusters = (outshine::Render::DagCluster *)malloc(dc.size() * sizeof(outshine::Render::DagCluster));
  if (!r->Verts || !r->Idx || !r->Clusters) {
    free(r->Verts); free(r->Idx); free(r->Clusters);
    r->Verts = 0; r->Idx = 0; r->Clusters = 0;
    return false;
  }
  memcpy(r->Verts, dv.data(), dv.size() * sizeof(float));
  memcpy(r->Idx, di.data(), di.size() * sizeof(uint32_t));
  memcpy(r->Clusters, dc.data(), dc.size() * sizeof(outshine::Render::DagCluster));
  r->NVerts = (int)(dv.size() / 8);
  r->NIdx = (int)di.size();
  r->NClusters = (int)dc.size();
  return true;
}

void FbpRunMesh(osmmesh_ctx *ctx, const FbpJob &j, FbpResult *r, FbpStats *st) {
  osmmesh_tile t = {};
  const double t0 = fbtp() ? fbtp_ms() : 0;
  const int fetched = osmmesh_fetch_tile(ctx, (uint8_t)j.Z, j.X, j.Y, &t);
  if (fbtp()) st->DemFetch += fbtp_ms() - t0;
  if (fetched != OSMMESH_OK || !t.terrain) { osmmesh_free_tile(&t); return; }
  outshine::Render::Chunk chunk = {};
  double o[3];
  const double t1 = fbtp() ? fbtp_ms() : 0;
  const int ok = outshine::Render::ChunkBuildEcef(t.terrain, j.Z, j.X, j.Y, j.Grid, &chunk, o);
  if (fbtp()) st->Mesh += fbtp_ms() - t1;
  osmmesh_free_tile(&t);
  if (!ok || chunk.nverts <= 0) { outshine::Render::ChunkFree(&chunk); return; }

  std::vector<float> dv;
  std::vector<uint32_t> di;
  std::vector<outshine::Render::DagCluster> dc;
  outshine::Render::TileDagBuild((const float *)chunk.verts, chunk.nverts, chunk.gridverts, o, dv, di, dc);
  r->Err = chunk.err;
  outshine::Render::ChunkFree(&chunk);
  if (dv.empty() || di.empty() || dc.empty()) return;
  if (!FbpPublish(r, dv, di, dc)) return;
  for (int a = 0; a < 3; a++) r->Origin[a] = o[a];
  r->Ok = true;
}

/* The one thing this layer knows about a soup: WHICH float carries the seam. What the vertices mean
 * stays with the caller that built them, so the predicate is a table and not a closure. */
template <int A> int FbpSeamAt(const float *v) { return v[A] < 0.0f ? 1 : 0; }
int (*const kFbpSeam[8])(const float *) = {FbpSeamAt<0>, FbpSeamAt<1>, FbpSeamAt<2>, FbpSeamAt<3>,
                                           FbpSeamAt<4>, FbpSeamAt<5>, FbpSeamAt<6>, FbpSeamAt<7>};

void FbpRunDag(const FbpJob &j, FbpResult *r) {
  const int nv = (int)(j.Soup.size() / 8);
  if (nv < 3) return;
  const float *soup = j.Soup.data();
  outshine::Render::ClusterDag dag;
  outshine::Render::ClusterDagOpts opts;
  if (j.SeamAttr >= 0 && j.SeamAttr < 8) opts.ClassOf = kFbpSeam[j.SeamAttr];
  std::vector<float> dv;
  std::vector<uint32_t> di;
  std::vector<outshine::Render::DagCluster> dc;
  if (outshine::Render::ClusterDagBuild(soup, (uint32_t)nv, 8, opts, &dag)) {
    dv = std::move(dag.Verts);
    di = std::move(dag.Idx);
    dc = std::move(dag.Clusters);
  } else {
    /* A soup too small to partition still has to appear as ONE cluster, or the caller's cluster list
     * stops describing its own buffer. */
    dv.assign(soup, soup + j.Soup.size());
    di.resize((size_t)nv);
    for (int i = 0; i < nv; i++) di[(size_t)i] = (uint32_t)i;
    outshine::Render::DagCluster c{};
    c.Count = (uint32_t)nv;
    c.ParentErr = outshine::Render::kDagRootErr;
    outshine::Render::BoundingSphere(soup, (uint32_t)nv, 8, c.SelfCenter, &c.SelfRadius);
    dc.push_back(c);
  }
  if (!FbpPublish(r, dv, di, dc)) return;
  r->Ok = true;
}

void FbpWorker(void) {
  StackProbe::Enter(StackProbe::Purpose::Tile);
  osmmesh_config cfg = {};
  FbpStats st;
  cfg.origin_lat = fbp_lat;
  cfg.origin_lon = fbp_lon;
  cfg.tile_provider = FbpProvider;
  cfg.tile_provider_user = &st;
  cfg.provider_terrain_max_zoom = 15;
  cfg.enable_terrain = 1;
  osmmesh_ctx *ctx = 0;
  if (osmmesh_create(&cfg, &ctx) != OSMMESH_OK) ctx = 0;

  for (;;) {
    FbpJob job;
    {
      std::unique_lock<std::mutex> lk(fbp_mu);
      fbp_cv.wait(lk, [] { return fbp_stop || !fbp_queue.empty(); });
      if (fbp_stop) break;
      /* Nearest first, exactly as the browser pump orders it: the tile the eye is standing on is
       * the one whose absence is visible. */
      size_t best = 0;
      for (size_t i = 1; i < fbp_queue.size(); i++)
        if (fbp_queue[i].Dist < fbp_queue[best].Dist) best = i;
      job = fbp_queue[best];
      fbp_queue.erase(fbp_queue.begin() + (long)best);
    }
    FbpResult res;
    if (job.Kind == 1) { if (ctx) FbpRunMesh(ctx, job, &res, &st); }
    else if (job.Kind == 3) FbpRunDag(job, &res);
    StackProbe::Mark();
    {
      std::lock_guard<std::mutex> lk(fbp_mu);
      fbp_done[job.Key] = std::move(res);
      if (fbtp()) {
        fbtp_.demfetch += st.DemFetch; fbtp_.mesh += st.Mesh; fbtp_.provfetch += st.ProvFetch;
        fbtp_.provcalls += st.ProvCalls;
        for (int k = 0; k < 3; k++) fbtp_.provkind[k] += st.ProvKind[k];
        if (job.Kind == 1) fbtp_.nmesh++;
        st = FbpStats();
      }
    }
  }
  if (ctx) osmmesh_destroy(ctx);
}

/* Post if nobody has, then answer with what is resident. A job that FAILED stays in `fbp_asked`, so
 * a missing DEM is asked for once and the tile stays a gap the skirts cover — the browser's rule. */
bool FbpPoll(const FbpJob &j, FbpResult *out) {
  std::unique_lock<std::mutex> lk(fbp_mu);
  auto it = fbp_done.find(j.Key);
  if (it != fbp_done.end()) {
    *out = std::move(it->second);
    fbp_done.erase(it);
    if (out->Ok) fbp_asked.erase(j.Key);
    return out->Ok;
  }
  if (fbp_asked.insert(j.Key).second) {
    fbp_queue.push_back(j);
    lk.unlock();
    fbp_cv.notify_one();
  }
  return false;
}

}  // namespace

int fb_stream_open(const char *base, double lat, double lon, int z) {
  (void)z;
  fbs_init(base);
  curl_global_init(CURL_GLOBAL_DEFAULT);   /* curl_easy_init's lazy global init is not thread-safe */
  outshine::Render::fb_srgb_lut_();        /* same reason: a lazy table built by two workers at once */
  fbp_lat = lat;
  fbp_lon = lon;
  fbp_cam_lat = lat;
  fbp_cam_lon = lon;
  unsigned hw = std::thread::hardware_concurrency();
  int n = hw > 3u ? (int)hw - 2 : 1;
  if (n > 6) n = 6;
  /* THE POOL WIDTH IS THE ARRIVAL SCHEDULE. A picture that is a function of the scene has to come out
   * the same at one worker and at six, and the only way to test that is to be able to say which. */
  if (const char *e = getenv("FB_TILEWORKERS")) {
    const int w = atoi(e);
    if (w > 0 && w <= 32) n = w;
  }
  for (int i = 0; i < n; i++) fbp_threads.emplace_back(FbpWorker);
  outshine::Log::Info("world", "tilepool", {{"workers", n}, {"hardwareThreads", (int)hw}});
  return 1;
}
void fb_stream_campos(double lat, double lon) {
  std::lock_guard<std::mutex> lk(fbp_mu);
  fbp_cam_lat = lat;
  fbp_cam_lon = lon;
}

int fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                    uint32_t **idx, int *nidx,
                    outshine::Render::DagCluster **clusters, int *nclusters,
                    double origin[3], float *err) {
  FbpJob j;
  j.Kind = 1; j.Z = z; j.X = x; j.Y = y; j.Grid = grid;
  j.Key = FbpKey(1, z, x, y);
  j.Dist = FbpTileDist(z, x, y);
  FbpResult r;
  if (!FbpPoll(j, &r)) return 0;
  *verts = r.Verts;
  *nverts = r.NVerts;
  *idx = r.Idx;
  *nidx = r.NIdx;
  *clusters = r.Clusters;
  *nclusters = r.NClusters;
  origin[0] = r.Origin[0];
  origin[1] = r.Origin[1];
  origin[2] = r.Origin[2];
  if (err) *err = r.Err;
  return 1;
}

int fb_stream_dag(int id, const float *soup, int nverts, int seamAttr, float **verts, int *nverts_out,
                  uint32_t **idx, int *nidx,
                  outshine::Render::DagCluster **clusters, int *nclusters) {
  const uint64_t key = ((uint64_t)3 << 62) | (uint64_t)(uint32_t)id;
  std::unique_lock<std::mutex> lk(fbp_mu);
  auto it = fbp_done.find(key);
  if (it != fbp_done.end()) {
    FbpResult r = std::move(it->second);
    fbp_done.erase(it);
    fbp_asked.erase(key);
    lk.unlock();
    if (!r.Ok) return 0;
    *verts = r.Verts; *nverts_out = r.NVerts;
    *idx = r.Idx; *nidx = r.NIdx;
    *clusters = r.Clusters; *nclusters = r.NClusters;
    return 1;
  }
  if (fbp_asked.insert(key).second) {
    FbpJob j;
    j.Kind = 3;
    j.Key = key;
    j.SeamAttr = seamAttr;
    j.Dist = -1.0;              /* ahead of every tile: the soup is already decoded and waiting */
    j.Soup.assign(soup, soup + (size_t)nverts * 8);
    fbp_queue.push_back(std::move(j));
    lk.unlock();
    fbp_cv.notify_one();
  }
  return 0;
}

/* Levels 0..N packed contiguous. >0 = bytes written, 0 = not yet, -1 = a real hole. */
/* The pointer is INTO the byte cache — valid until evicted; TerrainField decodes immediately. */
int fb_stream_dem(int z, int x, int y, const uint8_t **bytes, int *len) {
  char path[96];
  snprintf(path, sizeof path, "/t/terrain/%d/%d/%d", z, x, y);
  int n = fbs_size(path);   /* synchronous: never pending -> bytes or a real hole */
  struct fbs_ent *e = n > 0 ? fbs_find(path) : 0;
  if (!e || !e->data) { *bytes = 0; *len = 0; return -1; }
  *bytes = e->data; *len = e->len;
  return 1;
}

/* Returns >= 4 bytes (the count header even when empty), 0 on miss/too big. */
int fb_stream_lights(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap) {
  char path[96];
  snprintf(path, sizeof path, "/t/lights/%d/%u/%u", z, x, y);
  int n = fbs_size(path);
  if (n <= 0 || n > cap) return 0;
  fbs_copy(path, dst);
  return n;
}

int fb_stream_vector(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap) {
  char path[96];
  snprintf(path, sizeof path, "/t/vector/%d/%u/%u", z, x, y);
  int n = fbs_size(path);
  if (n <= 0) return -1;
  if (n > cap) return -1;
  fbs_copy(path, dst);
  return n;
}

void fb_stream_close(void) {
  {
    std::lock_guard<std::mutex> lk(fbp_mu);
    fbp_stop = true;
  }
  fbp_cv.notify_all();
  for (std::thread &t : fbp_threads) t.join();
  fbp_threads.clear();
  fbtp_report();
}

double fb_stream_cache_bytes(void) {
  double total = 0;
  for (int i = 0; i < fbs_cache_n; i++) total += fbs_cache[i].len;
  std::lock_guard<std::mutex> lk(fbp_cache_mu);
  for (const FbpEnt &e : fbp_cache) total += (double)e.Data.capacity();
  return total;
}

#endif /* __EMSCRIPTEN__ */

/* THE HEIGHT ORACLE, and it is deliberately OUTSIDE the platform split: a ground truth that differs
 * between two clients is not a ground truth. It reproduces fb-tiles' /elev exactly — same DEM zoom,
 * same tile, same bilinear (tiles/src/elev.c, fb_elev_at) — out of the tile bytes the client already
 * streams, so on both sides of the wire the answer is a PURE FUNCTION OF POSITION.
 *
 * WHY THE TILE AND NOT THE POINT. /elev is one round trip per position, and in the browser that trip
 * lands AFTER the tick that asked: a per-point cache can therefore never answer the question the
 * simulation is asking, and the un-keyed one that stood here answered it with somebody else's position
 * entirely. [MESS, mods/f22 c01m05] the browser briefed both strike aircraft's targets with the flight
 * LEADER's spawn ground — 777.06 m for 510.93 / 442.26 m — put the CCRP plane 270 m high, released
 * 1.6 s late and dropped 344 m short, while fb-gym flew the same file and destroyed both targets.
 * Keying that point cache fixed the briefing and left the per-tick samples permanently unresolved —
 * the stores then hit their carrier's spawn elevation, 350 m above the ground. ONE TILE IS 4.8 km of
 * ground at this zoom, twenty seconds of flight: the transport now happens two hundred times more
 * rarely than the question, which is what makes a streamed ground truth usable at all.
 * */
namespace {

constexpr int kFbDemZ = 13;        /* fb-tiles' FB_DEM_Z — the zoom /elev samples (tiles/src/elev.c) */
constexpr int kFbDemN = 256;       /* Terrarium texels per tile edge, as fb-tiles' FB_DEM_N */
constexpr int kFbDemSlots = 12;    /* a cast plus its stores sits in a handful of tiles at once */

struct FbDemTile {
  int Z = -1;
  long X = 0, Y = 0;
  uint32_t Cols = 0, Rows = 0;
  std::vector<float> H;
  bool Hole = false;
  uint64_t Used = 0;
};

FbDemTile fb_dem[kFbDemSlots];
uint64_t fb_dem_clock = 0;

/* Null = not usable yet: PENDING in the browser (retry next tick) or a real hole. Both leave the
 * caller with its last good value, which is the one behaviour a client may have for missing ground. */
const FbDemTile *fb_dem_tile(int z, long x, long y) {
  FbDemTile *victim = &fb_dem[0];
  for (FbDemTile &t : fb_dem) {
    if (t.Z == z && t.X == x && t.Y == y) { t.Used = ++fb_dem_clock; return t.Hole ? nullptr : &t; }
    if (t.Used < victim->Used) victim = &t;
  }
  const uint8_t *bytes = 0;
  int len = 0;
  const int rc = fb_stream_dem(z, (int)x, (int)y, &bytes, &len);
  if (rc == 0) return nullptr;   /* pending — NOT cached, or the hole would be permanent */
  victim->Z = z; victim->X = x; victim->Y = y;
  victim->Used = ++fb_dem_clock;
  victim->Hole = true;
  victim->Cols = victim->Rows = 0;
  if (rc != 1 || !bytes || len <= 0) return nullptr;
  osmmesh_terrain_grid grid{};
  if (osmmesh_terrain_decode_png(bytes, (size_t)len, &grid) != OSMMESH_TERRAIN_OK || !grid.heights)
    return nullptr;
  victim->Cols = grid.cols;
  victim->Rows = grid.rows;
  victim->H.assign(grid.heights, grid.heights + (size_t)grid.cols * grid.rows);
  osmmesh_terrain_grid_free(&grid);
  victim->Hole = false;
  return victim;
}

int fb_dem_texel(void *user, long x, long y, uint32_t col, uint32_t row, float *out) {
  const int z = *(const int *)user;
  const FbDemTile *t = fb_dem_tile(z, x, y);
  if (!t || t->Cols != kFbDemN || t->Rows != kFbDemN) return 0;
  *out = t->H[(size_t)row * kFbDemN + col];
  return 1;
}

} // namespace

double fb_stream_ground(double lat, double lon) {
  while (lon > 180.0) lon -= 360.0;   /* normalize lon before the tile maths (dateline) */
  while (lon < -180.0) lon += 360.0;
  double tx = 0.0, ty = 0.0;
  fb_geo_to_tile(lat, lon, kFbDemZ, &tx, &ty);
  int z = kFbDemZ;
  double v = 0.0;
  if (!fb_dem_bilinear(kFbDemZ, tx, ty, kFbDemN, fb_dem_texel, &z, &v)) return -1e9;
  return v < 0.0 ? 0.0 : v;   /* the sea-level clamp; the -1e9 "not yet" sentinel never reaches here */
}

/* The one raster left in this file: the moon, which is a MEASURED image of a real body and not
 * authored appearance (CLAUDE.md principle 2). Everything the bake used to decode is gone. */
extern "C" {
unsigned char *stbi_load_from_memory(const unsigned char *buffer, int len, int *x, int *y,
                                     int *channels_in_file, int desired_channels);
void stbi_image_free(void *retval_from_stbi_load);
}

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
