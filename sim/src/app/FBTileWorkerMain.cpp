/* FlightBox — off-thread tile worker (WASM, its own module; no WebGPU, no JSBSim). Runs inside a Web
 * Worker (fbtileworker.js) so the whole blocking pipeline — DEM/MVT/albedo fetch, stbi decode, osmmesh
 * meshing AND the sRGB mip pyramid — leaves the render thread. It hands back finished vertex arrays +
 * finished mip pyramids (transferables, zero-copy across postMessage). Ports command_center/tileworker.c
 * (the WHY-a-worker-not-pthread reasoning holds verbatim: the byte cache is a main-thread JS Map, so a
 * pthread would proxy every fetch back into the thread we are emptying — a worker just owns its fetch).
 *
 * ONE build in flight at a time (fbtileworker.js gates it): fbtw_build SUSPENDS on the synchronous
 * fetch under ASYNCIFY; a re-entrant second build corrupts the shared osmmesh/result state. */
#include <emscripten.h>
#include <emscripten/fetch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "osmmesh.h"
#include "geo.h"
#include "FBChunkMesh.h"
#include "style_ver.h"
#include "FBMips.h"
#include "FBLog.h"
#include "FBLogSinks.h"

/* stb_image lives in osmmesh's terrain.cpp (non-static, shared across the link). Declare, don't
 * re-impl. C linkage: stb wraps its API in extern "C" under C++. */
extern "C" {
unsigned char *stbi_load_from_memory(const unsigned char *buffer, int len, int *x, int *y,
                                     int *channels_in_file, int desired_channels);
void stbi_image_free(void *retval_from_stbi_load);
}

/* JS entry points: C linkage so the -sEXPORTED_FUNCTIONS names resolve unmangled. The definitions
 * below inherit this linkage. */
extern "C" {
int      fbtw_open(const char *base, double lat, double lon);
int      fbtw_build(int z, int x, int y, int grid, int mode, int ts, int what);
float   *fbtw_verts(void);
int      fbtw_nverts(void);
float    fbtw_err(void);
double  *fbtw_origin(void);
uint8_t *fbtw_mips(void);
int      fbtw_mipbytes(void);
int      fbtw_ts(void);
void     fbtw_release(void);
}

static osmmesh_ctx *tw_osm = 0;
static char tw_base[160] = "";

/* [tileperf-worker] — browser-side cold-boot timing (the venue the DEM cache actually runs in; native
 * headless can't reach drawn>0, so this is where the cache win is measured live). Per-stage ms + a
 * summary every 32 pyramids. Emitted via FBLog::Debug (tag "worker") to the worker's own stdout sink
 * (fbtw_open); the last line before the field converges is the cold-boot pipeline total. Cheap (a few
 * log lines per boot). */
static double twp_osmfetch = 0, twp_mesh = 0, twp_albfetch = 0, twp_decode = 0, twp_mips = 0, twp_t0 = 0;
static long twp_nmesh = 0, twp_npyr = 0;
static void twp_report(void) {
  double wall = emscripten_get_now() - twp_t0;
  long m = twp_nmesh ? twp_nmesh : 1, p = twp_npyr ? twp_npyr : 1;
  FlightBox::FBLog::Debug("worker", "tileperf",
      {{"wallMs", wall}, {"meshTiles", (int)twp_nmesh}, {"pyrTiles", (int)twp_npyr},
       {"osmFetchMs", twp_osmfetch}, {"osmFetchMsPerTile", twp_osmfetch / m},
       {"meshMs", twp_mesh}, {"meshMsPerTile", twp_mesh / m},
       {"albFetchMs", twp_albfetch}, {"albFetchMsPerTile", twp_albfetch / p},
       {"decodeMs", twp_decode}, {"decodeMsPerTile", twp_decode / p},
       {"mipsMs", twp_mips}, {"mipsMsPerTile", twp_mips / p}});
}

/* Synchronous byte fetch — a worker MAY block. 200 = bytes, 204 = a real hole, 202/404/5xx = queued or
 * transient (retry after a short sleep). Same contract as tileworker.c's tw_get. */
static int tw_get(const char *url, uint8_t **out, size_t *len) {
  for (int attempt = 0; attempt < 60; attempt++) {
    emscripten_fetch_attr_t a;
    emscripten_fetch_attr_init(&a);
    strcpy(a.requestMethod, "GET");
    a.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS | EMSCRIPTEN_FETCH_REPLACE;
    emscripten_fetch_t *f = emscripten_fetch(&a, url);
    if (!f) return 0;
    unsigned short st = f->status;
    if (st == 200 && f->numBytes > 0) {
      uint8_t *b = (uint8_t *)malloc((size_t)f->numBytes);
      if (!b) { emscripten_fetch_close(f); return 0; }
      memcpy(b, f->data, (size_t)f->numBytes);
      *out = b;
      *len = (size_t)f->numBytes;
      emscripten_fetch_close(f);
      return 1;
    }
    emscripten_fetch_close(f);
    if (st == 204 || st == 404) return 0;   /* nothing here (fb-tiles 404s genuinely-missing tiles) */
    emscripten_sleep(50);                    /* 202/5xx: queued or transient — wait for the bake */
  }
  return 0;
}

static int tw_provider(void *user, osmmesh_tile_kind kind, uint32_t z, uint32_t x, uint32_t y,
                       uint8_t **out, size_t *len) {
  (void)user;
  static const char *names[] = {"vector", "terrain", "imagery"};
  if ((int)kind < 0 || (int)kind >= 3) return 0;
  char url[256];
  snprintf(url, sizeof url, "%s/t/%s/%u/%u/%u", tw_base, names[kind], z, x, y);
  return tw_get(url, out, len);
}

EMSCRIPTEN_KEEPALIVE
int fbtw_open(const char *base, double lat, double lon) {
  static FlightBox::FBStdoutLogSink twLogSink;
  FlightBox::FBLog::SetSink(&twLogSink);
  FlightBox::FBLog::SetLevel(FlightBox::FBLogLevel::Debug);   /* the worker console used to print unconditionally */
  snprintf(tw_base, sizeof tw_base, "%s", base ? base : "");
  osmmesh_config cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.origin_lat = lat;
  cfg.origin_lon = lon;
  cfg.tile_provider = tw_provider;
  cfg.provider_terrain_max_zoom = 15;
  cfg.enable_terrain = 1;
  if (osmmesh_create(&cfg, &tw_osm) != OSMMESH_OK) { tw_osm = 0; return 0; }
  twp_t0 = emscripten_get_now();
  twp_osmfetch = twp_mesh = twp_albfetch = twp_decode = twp_mips = 0; twp_nmesh = twp_npyr = 0;
  return 1;
}

/* One tile's result, read back by JS after fbtw_build (globals — one build in flight). */
static w3_chunk tw_chunk;
static double tw_origin[3] = {0, 0, 0};
static uint8_t *tw_mips = 0;
static int tw_mipbytes = 0;
static int tw_ts = 0;

EMSCRIPTEN_KEEPALIVE float  *fbtw_verts(void)    { return (float *)tw_chunk.verts; }
EMSCRIPTEN_KEEPALIVE int     fbtw_nverts(void)   { return tw_chunk.nverts; }
EMSCRIPTEN_KEEPALIVE float   fbtw_err(void)      { return tw_chunk.err; }
EMSCRIPTEN_KEEPALIVE double *fbtw_origin(void)   { return tw_origin; }
EMSCRIPTEN_KEEPALIVE uint8_t *fbtw_mips(void)    { return tw_mips; }
EMSCRIPTEN_KEEPALIVE int     fbtw_mipbytes(void) { return tw_mipbytes; }
EMSCRIPTEN_KEEPALIVE int     fbtw_ts(void)       { return tw_ts; }

EMSCRIPTEN_KEEPALIVE
void fbtw_release(void) {
  w3_chunk_free(&tw_chunk);
  free(tw_mips);
  tw_mips = 0;
  tw_mipbytes = 0;
}

/* Fetch + decode + pyramid the albedo for (z,x,y) in `mode` (0 = /bake/osm, 1 = /bake/photo) at `ts`.
 * Fills tw_mips/tw_mipbytes/tw_ts. Returns 1 on a real image, 0 on a miss (caller falls back). */
static int tw_albedo(int z, int x, int y, int mode, int ts) {
  char url[256];
  /* ?v= busts the browser's immutable cache when the OSM bake style changes (photo is unversioned) */
  snprintf(url, sizeof url, mode ? "%s/bake/photo/%d/%d/%d?tex=%d" : "%s/bake/osm/%d/%d/%d?tex=%d&v=" FB_OSM_STYLE_VER_S,
           tw_base, z, x, y, ts);
  uint8_t *enc = 0;
  size_t n = 0;
  double tf = emscripten_get_now();
  int got = tw_get(url, &enc, &n);
  twp_albfetch += emscripten_get_now() - tf;
  if (!got) return 0;
  int w = 0, h = 0, comp = 0;
  double td = emscripten_get_now();
  uint8_t *px = stbi_load_from_memory(enc, (int)n, &w, &h, &comp, 4);
  twp_decode += emscripten_get_now() - td;
  free(enc);
  if (!px) return 0;
  if (w != ts || h != ts) { stbi_image_free(px); return 0; }
  int bytes = fb_pyramid_bytes(ts);
  uint8_t *mips = (uint8_t *)malloc((size_t)bytes);
  if (!mips) { stbi_image_free(px); return 0; }
  double tp = emscripten_get_now();
  fb_build_pyramid(px, ts, mips);
  twp_mips += emscripten_get_now() - tp;
  if (++twp_npyr % 32 == 0) twp_report();
  stbi_image_free(px);
  tw_mips = mips;
  tw_mipbytes = bytes;
  tw_ts = ts;
  return 1;
}

/* Build one tile's requested parts. `what` is a bitmask: bit0 = mesh, bit1 = albedo pyramid (for
 * `mode`/`ts`). Mesh and albedo are decoupled so the main thread can queue them independently (base
 * mesh + base albedo at high priority, the lazy overlay albedo lower). Returns the same bitmask of
 * what actually succeeded — JS reads whichever it asked for. */
EMSCRIPTEN_KEEPALIVE
int fbtw_build(int z, int x, int y, int grid, int mode, int ts, int what) {
  fbtw_release();
  int result = 0;

  if ((what & 1) && tw_osm) {
    osmmesh_tile t = {};
    double tof = emscripten_get_now();
    int fetched = (osmmesh_fetch_tile(tw_osm, (uint8_t)z, (uint32_t)x, (uint32_t)y, &t) == OSMMESH_OK && t.terrain);
    twp_osmfetch += emscripten_get_now() - tof;
    if (fetched) {
      double tm = emscripten_get_now();
      int meshed = (w3_chunk_build_ecef(t.terrain, z, (uint32_t)x, (uint32_t)y, grid, &tw_chunk, tw_origin) &&
                    tw_chunk.nverts > 0);
      twp_mesh += emscripten_get_now() - tm; twp_nmesh++;
      if (meshed) result |= 1;
    }
    osmmesh_free_tile(&t);
  }

  if ((what & 2) && tw_albedo(z, x, y, mode, ts)) result |= 2;
  return result;
}
