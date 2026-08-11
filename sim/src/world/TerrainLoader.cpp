#include "TerrainLoader.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "ChunkSurface.h"
#include "GroundSample.h"
#include "Heap.h"
#include "Log.h"
#include "geo.h"
#include "osmmesh.h"
#include "terrain.h"
#include "tilemath.h"   /* fb-tiles' OWN tile maths, so a tile index means the same on both sides */

/* A C ABI cannot live in a namespace, but the types it borrows do. */
using namespace outshine;
using outshine::World::TilePool;

/* stb_image's implementation lives in terrain.cpp — declared here, never re-implemented: two
 * implementations in one link would collide. */

namespace {

constexpr int kProviderTerrainMaxZ = 15;   /* fb-tiles' terrarium source stops here */

/* [SET] The pool's ceiling, in threads: one per core beyond the two the client's own threads
 * occupy. Six against a browser's six connections per host, which is why the in-flight cap and the
 * thread count are the same number. */
constexpr int kMaxTileThreads = 6;

/* [SET] What the shared byte cache may hold. A z14 Terrarium PNG measures 50-160 KiB and a vector
 * tile 5-54 KiB (fb-tiles, Weserbergland), so this is several rings of both around the camera —
 * a ceiling reached only by travelling, and the eviction below is what makes travelling survivable. */
constexpr size_t kByteBudget = 64u * 1024u * 1024u;

/* [SET] Decoded source grids per pool thread. A stitch reads five (the tile and its four edge
 * neighbours) and everything beyond that buys sharing between consecutive builds; at 256 KiB per
 * z14 grid this is 4 MiB a thread, 24 MiB at the six-thread ceiling. */
constexpr int kPoolDemCacheTiles = 16;

std::unique_ptr<TilePool> gPool;

double Clamped01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

double Wrapped180(double lonDeg) {
  while (lonDeg > 180.0) lonDeg -= 360.0;
  while (lonDeg < -180.0) lonDeg += 360.0;
  return lonDeg;
}

int PoolThreads() {
  unsigned hw = std::thread::hardware_concurrency();
  int n = hw > 3u ? (int)hw - 2 : 1;
  if (n > kMaxTileThreads) n = kMaxTileThreads;
  /* THE POOL WIDTH IS THE ARRIVAL SCHEDULE. A picture that is a function of the scene has to come out
   * the same at one thread and at six, and the only way to test that is to be able to say which. */
  if (const char *e = getenv("FB_TILEWORKERS")) {
    const int w = atoi(e);
    if (w > 0 && w <= 32) n = w;
  }
  return n;
}

/* THE HEIGHT ORACLE. It answers with the surface the renderer DRAWS — the finest tile of the ladder,
 * its stitched source grid, its posting lattice, its triangle, its split (ChunkSurface.h) — so what
 * is placed on the ground stands on the ground. The finest tile and not the drawn one: a coarser tile
 * is the renderer's approximation of this surface within a declared screen-space error, and a
 * placement that followed the LOD would move as the camera walks.
 *
 * WHY THE TILE AND NOT THE POINT. /elev is one round trip per position, and in the browser that trip
 * lands AFTER the tick that asked: a per-point cache can therefore never answer the question the
 * simulation is asking, and the un-keyed one that stood here answered it with somebody else's
 * position entirely. ONE TILE IS 4.8 km of ground at this zoom, twenty seconds of flight: the
 * transport happens two hundred times more rarely than the question, which is what makes a streamed
 * ground truth usable at all. */
constexpr int kGroundSlots = 12;   /* a cast plus its stores sits in a handful of tiles at once */
/* WHAT ONE STITCH TOUCHES, and nothing beyond it: a build needs five decoded grids at once. Every
 * entry beyond the fifth buys sharing BETWEEN builds, and it buys it with permanent memory — one z14
 * grid is 256*256 float32 = 256 KiB, so the decodesPerBuild the teardown reports is the price of that
 * trade, measured, per scenario. */
constexpr int kGroundStitchGrids = 5;
constexpr double kGroundGridBytes = 256.0 * 256.0 * 4.0;

/* One tile of the drawn surface, reduced to what a query needs: the chunk's node heights. Built once
 * per tile, because the stitch behind them costs five PNG decodes. */
struct GroundTile {
  long X = 0, Y = 0;
  int Nodes = 0;                 /* per edge; the chunk is square because the source grid is */
  uint32_t Postings = 0;
  std::vector<float> H;
  bool Resident = false;
  bool Hole = false;
  uint64_t Used = 0;
};

FbGroundSurface gSurface{14, 128};
GroundTile gGround[kGroundSlots];
uint64_t gGroundClock = 0;
long gGroundBuilds = 0;    /* tiles turned into node heights */
long gGroundDecodes = 0;   /* PNGs the stitch decoded for them -- 5 per build without the cache */
double gGroundStitchMs = 0.0;   /* the wall the builds spent inside osmmesh, decode included */
osmmesh_ctx *gGroundCtx = nullptr;
bool gGroundPending = false;   /* set by the provider: a miss that is a wait, not a hole */

/* THE ORACLE'S BYTES, and the platform split is exactly one statement: a browser's frame thread
 * cannot wait for a fetch, a native one may. Both read the pool's one cache, so the DEM the mesh
 * already pulled is the DEM the oracle reads. */
TilePool::Reply GroundBytes(const char *path, std::vector<uint8_t> *out) {
#ifdef __EMSCRIPTEN__
  return gPool->Bytes(path, out);
#else
  return gPool->BytesBlocking(path, out);
#endif
}

int GroundProvider(void *, osmmesh_tile_kind kind, uint32_t z, uint32_t x, uint32_t y,
                   uint8_t **out, size_t *len) {
  *out = nullptr;
  *len = 0;
  if (kind != OSMMESH_TILE_TERRAIN || !gPool) return 0;
  char path[96];
  std::snprintf(path, sizeof path, "/t/terrain/%u/%u/%u", z, x, y);
  gGroundDecodes++;   /* a provider call IS a DEM LRU miss: osmmesh decodes what it is handed */
  std::vector<uint8_t> bytes;
  const TilePool::Reply reply = GroundBytes(path, &bytes);
  if (reply == TilePool::Reply::Pending) gGroundPending = true;
  if (reply != TilePool::Reply::Ready || bytes.empty()) return 0;
  uint8_t *copy = (uint8_t *)Heap::Take("oracle dem", bytes.size());   /* osmmesh frees it */
  std::memcpy(copy, bytes.data(), bytes.size());
  *out = copy;
  *len = bytes.size();
  return 1;
}

void FillNodeHeights(const osmmesh_terrain_grid *grid, uint32_t rowPostings, uint32_t colPostings,
                     GroundTile *out) {
  out->H.resize((size_t)out->Nodes * (size_t)out->Nodes);
  for (int j = 0; j < out->Nodes; j++) {
    const double fr = osmmesh_terrain_posting_frac(
        World::ChunkNodePosting(j, rowPostings, out->Nodes), rowPostings);
    for (int i = 0; i < out->Nodes; i++) {
      const double fc = osmmesh_terrain_posting_frac(
          World::ChunkNodePosting(i, colPostings, out->Nodes), colPostings);
      out->H[(size_t)j * (size_t)out->Nodes + (size_t)i] =
          osmmesh_terrain_posting_height(grid, fc, fr);
    }
  }
}

/* Null = not usable yet: PENDING (retry next tick) or a real hole, and fb_stream_ground below is what
 * tells those two apart for the caller. */
const GroundTile *GroundTileAt(long x, long y) {
  GroundTile *victim = &gGround[0];
  for (GroundTile &t : gGround) {
    if (t.Resident && t.X == x && t.Y == y) {
      t.Used = ++gGroundClock;
      return t.Hole ? nullptr : &t;
    }
    if (t.Used < victim->Used) victim = &t;
  }
  if (!gGroundCtx) return nullptr;
  gGroundPending = false;
  uint32_t rowPostings = 0, colPostings = 0;
  const osmmesh_terrain_grid *grid = nullptr;
  const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  const int rc = osmmesh_tile_grid(gGroundCtx, (uint8_t)gSurface.Z, (uint32_t)x, (uint32_t)y, &grid,
                                   &rowPostings, &colPostings);
  gGroundStitchMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  /* A REFUSED ALLOCATION IS NOT A HOLE, and on this path the difference is the whole run: a hole is
   * cached, answers Missing() for ever after, and the once-only scatters downstream take that as
   * final. So the two negative codes are separated before anything is written. */
  if (rc == OSMMESH_ERR_OOM) Heap::Exhausted("oracle dem grid");
  if (rc != OSMMESH_OK) {
    /* A source that will not decode is not a wait either — the bytes are cached and the next ask
     * decodes the same failure — so it is a hole, and this line is what says whose. */
    Log::Error("world", "ground_grid_failed",
               {{"z", gSurface.Z}, {"x", (int)x}, {"y", (int)y}, {"rc", rc}});
  }
  const int gr = grid ? World::ChunkNodes(rowPostings, gSurface.Grid) : 0;
  const int gc = grid ? World::ChunkNodes(colPostings, gSurface.Grid) : 0;
  const bool square = gr >= 2 && gr == gc && rowPostings == colPostings;
  if (gGroundPending) return nullptr;   /* NOT cached, or the wait would become permanent */
  gGroundBuilds++;
  victim->X = x;
  victim->Y = y;
  victim->Used = ++gGroundClock;
  victim->Resident = true;
  victim->Hole = !square;
  victim->Nodes = square ? gr : 0;
  victim->Postings = square ? colPostings : 0;
  if (!square) {
    victim->H.clear();
    return nullptr;
  }
  FillNodeHeights(grid, rowPostings, colPostings, victim);
  return victim;
}

/* THE ONE EVALUATION OF THE DRAWN SURFACE INSIDE A TILE, by fraction of the tile: fx east from the
 * west edge, fy south from the north edge. Both readers below go through it, so "a block and a point
 * answer the same number" is a property of the code. */
double TileHeightAslM(const float *nodes, int side, uint32_t postings, double fx, double fy) {
  const double px = Clamped01(fx) * (double)(postings - 1);
  const double py = Clamped01(fy) * (double)(postings - 1);
  const int i = World::ChunkNodeCell(px, postings, side);
  const int j = World::ChunkNodeCell(py, postings, side);
  const uint32_t c0 = World::ChunkNodePosting(i, postings, side);
  const uint32_t c1 = World::ChunkNodePosting(i + 1, postings, side);
  const uint32_t r0 = World::ChunkNodePosting(j, postings, side);
  const uint32_t r1 = World::ChunkNodePosting(j + 1, postings, side);
  const float su = (float)((px - (double)c0) / (double)(c1 - c0));
  const float sv = (float)((py - (double)r0) / (double)(r1 - r0));
  const World::ChunkCell cell{nodes, side, j, i};
  return (double)World::ChunkCellHeight(cell, su, sv);
}

void GroundOpen(FbGroundSurface surface) {
  gSurface = surface;
  osmmesh_config cfg{};
  cfg.tile_provider = GroundProvider;
  cfg.provider_terrain_max_zoom = kProviderTerrainMaxZ;
  cfg.dem_cache_tiles = kGroundStitchGrids;
  cfg.enable_terrain = 1;
  const int rc = osmmesh_create(&cfg, &gGroundCtx);
  if (rc == OSMMESH_ERR_OOM) Heap::Exhausted("oracle context");
  if (rc != OSMMESH_OK) {
    /* Every other code is this call's own arguments being wrong, three lines up: an oracle that
     * opens without a context answers "no ground" EVERYWHERE and does it silently. */
    Log::Error("world", "ground_oracle_unopenable", {{"rc", rc}});
    std::abort();
  }
}

void GroundClose() {
  if (gGroundCtx)
    Log::Debug("world", "ground_oracle",
        {{"tileBuilds", (int)gGroundBuilds}, {"demDecodes", (int)gGroundDecodes},
         {"decodesPerBuild", gGroundBuilds ? (double)gGroundDecodes / (double)gGroundBuilds : 0.0},
         {"stitchMs", gGroundStitchMs},
         {"stitchMsPerBuild", gGroundBuilds ? gGroundStitchMs / (double)gGroundBuilds : 0.0},
         {"gridCache", kGroundStitchGrids},
         {"gridCacheMB", kGroundStitchGrids * kGroundGridBytes / 1048576.0}});
  gGroundBuilds = 0;
  gGroundStitchMs = 0.0;
  gGroundDecodes = 0;
  if (gGroundCtx) osmmesh_destroy(gGroundCtx);
  gGroundCtx = nullptr;
  for (GroundTile &t : gGround) t = GroundTile{};
  gGroundClock = 0;
}

}  // namespace

int fb_stream_open(const char *base, double lat, double lon, FbGroundSurface surface) {
  TilePool::Config config;
  config.TilesBase = base ? base : "";
  config.OriginLatDeg = lat;
  config.OriginLonDeg = lon;
  config.Threads = PoolThreads();
  config.ByteBudget = kByteBudget;
  config.DemCacheTiles = kPoolDemCacheTiles;
  gPool = std::make_unique<TilePool>(config);
  GroundOpen(surface);
  return 1;
}

void fb_stream_close(void) {
  GroundClose();
  gPool.reset();
}

TilePool *fb_tile_pool(void) { return gPool.get(); }

double fb_stream_ground_post_m(double latDeg) {
  return 40075016.686 * std::cos(latDeg * 3.14159265358979 / 180.0) /
         (double)((long)1 << gSurface.Z) / (double)gSurface.Grid;
}

GroundSample fb_stream_ground(double lat, double lon) {
  lon = Wrapped180(lon);
  double tx = 0.0, ty = 0.0;
  fb_geo_to_tile(lat, lon, gSurface.Z, &tx, &ty);
  long hx = (long)tx, hy = (long)ty;
  if (!fb_tile_wrap(gSurface.Z, &hx, &hy)) return GroundSample::Missing();
  gGroundPending = false;
  const GroundTile *t = GroundTileAt(hx, hy);
  if (!t) return gGroundPending ? GroundSample::Waiting() : GroundSample::Missing();
  return GroundSample::At(
      TileHeightAslM(t->H.data(), t->Nodes, t->Postings, tx - (double)hx, ty - (double)hy));
}

FbGroundBlock fb_stream_ground_block(int z, long x, long y) {
  FbGroundBlock block;
  if (z != gSurface.Z) return block;   /* Missing: no other zoom of this surface exists */
  long hx = x, hy = y;
  if (!fb_tile_wrap(z, &hx, &hy)) return block;
  gGroundPending = false;
  const GroundTile *t = GroundTileAt(hx, hy);
  if (!t) {
    block.Where_ = gGroundPending ? FbGroundBlock::State::Pending : FbGroundBlock::State::Missing;
    return block;
  }
  block.Nodes_ = t->H.data();
  block.X_ = hx;
  block.Y_ = hy;
  block.Zoom_ = z;
  block.Side_ = t->Nodes;
  block.Postings_ = t->Postings;
  block.Where_ = FbGroundBlock::State::Resolved;
  return block;
}

void FbGroundBlock::AslMRow(double latDeg, double lonFromDeg, double lonStepDeg, int count,
                            double *out) const noexcept {
  double tx0 = 0.0, ty = 0.0, tx1 = 0.0, tyEnd = 0.0;
  fb_geo_to_tile(latDeg, Wrapped180(lonFromDeg), Zoom_, &tx0, &ty);
  fb_geo_to_tile(latDeg, Wrapped180(lonFromDeg + lonStepDeg), Zoom_, &tx1, &tyEnd);
  /* A row starting west of the dateline and stepping over it comes back as a tile index of nearly
   * zero, which would run the whole row backwards across the world. */
  const double width = (double)((long)1 << Zoom_);
  if ((tx1 - tx0) * lonStepDeg < 0.0) tx1 += lonStepDeg > 0.0 ? width : -width;
  const double fy = ty - (double)Y_;
  const double fx0 = tx0 - (double)X_, fxStep = tx1 - tx0;
  for (int i = 0; i < count; i++)
    out[i] = TileHeightAslM(Nodes_, Side_, Postings_, fx0 + (double)i * fxStep, fy);
}

/* The one raster left in this file: the moon, which is a MEASURED image of a real body and not
 * authored appearance (CLAUDE.md principle 2). */
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
  uint8_t *enc = (uint8_t *)Heap::Take("moon file", (size_t)n);
  size_t rd = fread(enc, 1, (size_t)n, f);
  fclose(f);
  if ((long)rd != n) { free(enc); return 0; }
  int comp = 0;
  uint8_t *px = stbi_load_from_memory(enc, (int)n, w, h, &comp, 4);
  free(enc);
  if (!px) return 0;
  size_t bytes = (size_t)(*w) * (*h) * 4;
  uint8_t *out = (uint8_t *)Heap::Take("moon rgba", bytes);   /* the caller free()s */
  memcpy(out, px, bytes);
  stbi_image_free(px);
  *rgba = out;
  return 1;
}

FbStarBands fb_fetch_stars(uint8_t *dst, int cap) {
  if (!gPool) return {FbStarBands::State::Complete, 0};
  int off = 0;
  std::vector<uint8_t> band;
  for (int b = 0; b < 4; b++) {   /* fb-tiles serves 4 HYG mag bands (0..3); no 5th to 404 on */
    char path[64];
    std::snprintf(path, sizeof path, "/t/stars/%d/0/0", b);
    const TilePool::Reply reply = gPool->Bytes(path, &band);
    /* The pool caches what has landed, so the bands already copied are re-copied for free on the
     * next turn and no progress has to be carried across the poll. */
    if (reply == TilePool::Reply::Pending) return {FbStarBands::State::Pending, 0};
    if (reply != TilePool::Reply::Ready || band.empty()) break;
    if (off + (int)band.size() > cap) break;
    memcpy(dst + off, band.data(), band.size());
    off += (int)band.size();
  }
  return {FbStarBands::State::Complete, off};
}
