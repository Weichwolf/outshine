/* The off-thread tile worker (WASM, its own module): the whole blocking pipeline leaves the render
 * thread and finished vertex arrays come back zero-copy across postMessage.
 *
 * A WORKER AND NOT A PTHREAD: the byte cache is a main-thread JS Map, so a pthread would proxy every
 * fetch back into the very thread being emptied.
 * ONE build in flight at a time (the JS shim gates it): fbtw_build SUSPENDS on the synchronous fetch
 * under ASYNCIFY, and a re-entrant second build corrupts the shared result state.
 * doc/world/terrain.md, Abschnitt 6. */
#include <emscripten.h>
#include <emscripten/fetch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "osmmesh.h"
#include "geo.h"
#include "ChunkMesh.h"
#include "ClusterDag.h"
#include "style_ver.h"
#include "Mips.h"
#include "Log.h"
#include "LogSinks.h"

/* extern "C" exports pin this TU to the global namespace; the types it borrows are namespaced. */
using namespace outshine;

/* stb_image's implementation lives in terrain.cpp — declared, never re-implemented. */

/* C linkage so the -sEXPORTED_FUNCTIONS names resolve unmangled; the definitions inherit it. */
extern "C" {
int      fbtw_open(const char *base, double lat, double lon);
int      fbtw_build(int z, int x, int y, int grid);
float   *fbtw_verts(void);
int      fbtw_nverts(void);
uint32_t *fbtw_idx(void);
int      fbtw_nidx(void);
uint8_t *fbtw_clusters(void);
int      fbtw_nclusters(void);
int      fbtw_clusterbytes(void);
int      fbtw_dag(const float *soup, int nverts, int seamAttr);
float    fbtw_err(void);
double  *fbtw_origin(void);
void     fbtw_release(void);
}

static osmmesh_ctx *tw_osm = 0;
static char tw_base[160] = "";

/* [tileperf-worker]: the browser-side half of the cold-boot timing — the venue the DEM cache actually
 * runs in, since native headless never reaches drawn>0. §3.4 */
static double twp_osmfetch = 0, twp_mesh = 0, twp_t0 = 0;
static long twp_nmesh = 0;
static void twp_report(void) {
  double wall = emscripten_get_now() - twp_t0;
  long m = twp_nmesh ? twp_nmesh : 1;
  outshine::Log::Debug("worker", "tileperf",
      {{"wallMs", wall}, {"meshTiles", (int)twp_nmesh},
       {"osmFetchMs", twp_osmfetch}, {"osmFetchMsPerTile", twp_osmfetch / m},
       {"meshMs", twp_mesh}, {"meshMsPerTile", twp_mesh / m}});
}

/* A worker MAY block. 200 = bytes, 204 = a real hole, everything else = retry. */
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
    emscripten_sleep(50);                    /* 202/5xx: queued or transient — the server is fetching */
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
  static outshine::Clients::StdoutLogSink twLogSink;
  outshine::Log::SetSink(&twLogSink);
  outshine::Log::SetLevel(outshine::LogLevel::Debug);   /* the worker console used to print unconditionally */
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
  twp_osmfetch = twp_mesh = 0; twp_nmesh = 0;
  return 1;
}

/* One tile's result, read back by JS after fbtw_build. Globals: one build in flight. The MESH THAT
 * LEAVES HERE IS THE DAG: the simplifier is 3.68 ms of a measured 10.7 ms per tile natively and far
 * more in the browser, and the render thread may not pay it. */
static std::vector<float> tw_dagverts;
static std::vector<uint32_t> tw_dagidx;
static std::vector<Render::DagCluster> tw_clusters;
static float tw_err = 0.0f;
static double tw_origin[3] = {0, 0, 0};

EMSCRIPTEN_KEEPALIVE float  *fbtw_verts(void)    { return tw_dagverts.data(); }
EMSCRIPTEN_KEEPALIVE int     fbtw_nverts(void)   { return (int)(tw_dagverts.size() / 8); }
EMSCRIPTEN_KEEPALIVE uint32_t *fbtw_idx(void)    { return tw_dagidx.data(); }
EMSCRIPTEN_KEEPALIVE int     fbtw_nidx(void)     { return (int)tw_dagidx.size(); }
EMSCRIPTEN_KEEPALIVE uint8_t *fbtw_clusters(void){ return (uint8_t *)tw_clusters.data(); }
EMSCRIPTEN_KEEPALIVE int     fbtw_nclusters(void){ return (int)tw_clusters.size(); }
/* The JS shim copies the cluster array out as raw bytes and must not carry a sizeof of its own. */
EMSCRIPTEN_KEEPALIVE int     fbtw_clusterbytes(void){ return (int)(tw_clusters.size() * sizeof(Render::DagCluster)); }
EMSCRIPTEN_KEEPALIVE float   fbtw_err(void)      { return tw_err; }
EMSCRIPTEN_KEEPALIVE double *fbtw_origin(void)   { return tw_origin; }

EMSCRIPTEN_KEEPALIVE
void fbtw_release(void) {
  std::vector<float>().swap(tw_dagverts);
  std::vector<uint32_t>().swap(tw_dagidx);
  std::vector<Render::DagCluster>().swap(tw_clusters);
  tw_err = 0.0f;
}

/* THE DAG OF A GIVEN SOUP (pos3+norm3+uv2). Same worker, same exports, no fetch — the caller sends
 * the vertices and reads the ladder back. `seamAttr` >= 0 names the float whose sign marks an
 * attribute seam. 1 = built, 0 = nothing to build. */
template <int A> static int tw_seam_at(const float *v) { return v[A] < 0.0f ? 1 : 0; }
static int (*const tw_seam[8])(const float *) = {tw_seam_at<0>, tw_seam_at<1>, tw_seam_at<2>, tw_seam_at<3>,
                                                 tw_seam_at<4>, tw_seam_at<5>, tw_seam_at<6>, tw_seam_at<7>};

EMSCRIPTEN_KEEPALIVE
int fbtw_dag(const float *soup, int nverts, int seamAttr) {
  fbtw_release();
  if (!soup || nverts < 3) return 0;
  Render::ClusterDag dag;
  Render::ClusterDagOpts opts;
  if (seamAttr >= 0 && seamAttr < 8) opts.ClassOf = tw_seam[seamAttr];
  if (Render::ClusterDagBuild(soup, (uint32_t)nverts, 8, opts, &dag)) {
    tw_dagverts = std::move(dag.Verts);
    tw_dagidx = std::move(dag.Idx);
    tw_clusters = std::move(dag.Clusters);
  } else {
    tw_dagverts.assign(soup, soup + (size_t)nverts * 8);
    tw_dagidx.resize((size_t)nverts);
    for (int i = 0; i < nverts; i++) tw_dagidx[(size_t)i] = (uint32_t)i;
    Render::DagCluster c{};
    c.Count = (uint32_t)nverts;
    c.ParentErr = Render::kDagRootErr;
    Render::BoundingSphere(soup, (uint32_t)nverts, 8, c.SelfCenter, &c.SelfRadius);
    tw_clusters.push_back(c);
  }
  return 1;
}

/* Returns 1 if the mesh + DAG were built. There is no second channel: the albedo pyramid it used to
 * carry was a raster of an authored bake, and the classification needs the class, not a picture. */
EMSCRIPTEN_KEEPALIVE
int fbtw_build(int z, int x, int y, int grid) {
  fbtw_release();
  int result = 0;

  if (tw_osm) {
    osmmesh_tile t = {};
    double tof = emscripten_get_now();
    int fetched = (osmmesh_fetch_tile(tw_osm, (uint8_t)z, (uint32_t)x, (uint32_t)y, &t) == OSMMESH_OK && t.terrain);
    twp_osmfetch += emscripten_get_now() - tof;
    if (fetched) {
      double tm = emscripten_get_now();
      Render::Chunk chunk = {};
      int meshed = (Render::ChunkBuildEcef(t.terrain, z, (uint32_t)x, (uint32_t)y, grid, &chunk, tw_origin) &&
                    chunk.nverts > 0);
      if (meshed) {
        tw_err = chunk.err;
        Render::TileDagBuild((const float *)chunk.verts, chunk.nverts, chunk.gridverts, tw_origin,
                             tw_dagverts, tw_dagidx, tw_clusters);
        meshed = !tw_dagverts.empty() && !tw_dagidx.empty() && !tw_clusters.empty();
      }
      Render::ChunkFree(&chunk);
      twp_mesh += emscripten_get_now() - tm;
      if (++twp_nmesh % 32 == 0) twp_report();
      if (meshed) result |= 1;
    }
    osmmesh_free_tile(&t);
  }

  return result;
}
