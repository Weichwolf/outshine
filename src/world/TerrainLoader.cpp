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
#include "SourceSet.h"
#include "StarBands.h"
#include "TerrainTiles.h"
#include "TileGeodesy.h"

using namespace outshine;
using namespace outshine::World;
using outshine::World::TilePool;

namespace {

constexpr int kMaxTileThreads = 6;

constexpr size_t kByteBudget = 64u * 1024u * 1024u;

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

  if (const char *e = getenv("FB_TILEWORKERS")) {
    const int w = atoi(e);
    if (w > 0 && w <= 32) n = w;
  }
  return n;
}

constexpr int kGroundSlots = 12;

constexpr int kGroundStitchGrids = 5;
constexpr double kGroundGridBytes = 256.0 * 256.0 * 4.0;

struct GroundTile {
  long X = 0, Y = 0;
  int Nodes = 0;
  uint32_t Postings = 0;
  std::vector<float> H;
  bool Resident = false;
  bool Hole = false;
  uint64_t Used = 0;
};

FbGroundSurface gSurface{14, 128};
GroundTile gGround[kGroundSlots];
uint64_t gGroundClock = 0;
long gGroundBuilds = 0;
long gGroundDecodes = 0;
double gGroundStitchMs = 0.0;
bool gGroundPending = false;

class OracleTerrain : public TerrainSource {
 public:
  TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
    if (!gPool) return TerrainBytes::Wire();
    gGroundDecodes++;
    const Data::Request request(Data::DataKind::Elevation, Data::Address::Tile(z, x, y));
    TilePool::Landing landing;
    switch (gPool->BytesBlocking(request, &landing)) {
      case TilePool::Reply::Ready: {

        int az = 0;
        uint32_t ax = 0, ay = 0;
        if (!landing.At.TryTile(&az, &ax, &ay)) return TerrainBytes::Wire();
        return TerrainBytes::From(az, ax, ay, std::move(landing.Bytes));
      }
      case TilePool::Reply::Absent: return TerrainBytes::Nothing();

      case TilePool::Reply::Undeclared: return TerrainBytes::Nothing();
      case TilePool::Reply::Refused: return TerrainBytes::Wire();
      case TilePool::Reply::Pending: break;
    }
    gGroundPending = true;
    return TerrainBytes::Waiting();
  }
};

OracleTerrain gGroundSource;
std::unique_ptr<TerrainTiles> gGroundTiles;

void FillNodeHeights(const TerrainField &field, uint32_t rowPostings, uint32_t colPostings,
                     GroundTile *out) {
  out->H.resize((size_t)out->Nodes * (size_t)out->Nodes);
  for (int j = 0; j < out->Nodes; j++) {
    const double fr = PostingFrac(World::ChunkNodePosting(j, rowPostings, out->Nodes), rowPostings);
    for (int i = 0; i < out->Nodes; i++) {
      const double fc = PostingFrac(World::ChunkNodePosting(i, colPostings, out->Nodes), colPostings);
      out->H[(size_t)j * (size_t)out->Nodes + (size_t)i] = field.InterpolatedM(fc, fr);
    }
  }
}

const GroundTile *GroundTileAt(long x, long y) {
  GroundTile *victim = &gGround[0];
  for (GroundTile &t : gGround) {
    if (t.Resident && t.X == x && t.Y == y) {
      t.Used = ++gGroundClock;
      return t.Hole ? nullptr : &t;
    }
    if (t.Used < victim->Used) victim = &t;
  }
  if (!gGroundTiles) return nullptr;
  gGroundPending = false;
  const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  const TerrainGrid grid = gGroundTiles->StitchedGrid(gSurface.Z, (uint32_t)x, (uint32_t)y);
  gGroundStitchMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  const TerrainField *field = grid.TryField();
  if (grid.Where() == TerrainGrid::State::Undecodable) {

    Log::Error("world", "ground_grid_failed", {{"z", gSurface.Z}, {"x", (int)x}, {"y", (int)y}});
  }
  const uint32_t stride = gGroundTiles->Stride();
  const uint32_t rowPostings = field ? PostingsPerEdge(field->Rows(), stride) : 0;
  const uint32_t colPostings = field ? PostingsPerEdge(field->Cols(), stride) : 0;
  const int gr = field ? World::ChunkNodes(rowPostings, gSurface.Grid) : 0;
  const int gc = field ? World::ChunkNodes(colPostings, gSurface.Grid) : 0;
  const bool square = gr >= 2 && gr == gc && rowPostings == colPostings;
  if (gGroundPending) return nullptr;
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
  FillNodeHeights(*field, rowPostings, colPostings, victim);
  return victim;
}

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

  TerrainTiles::Config config;
  config.DemCacheTiles = kGroundStitchGrids;
  gGroundTiles = std::make_unique<TerrainTiles>(gGroundSource, EnuFrame::At(0.0, 0.0), config);
}

void GroundClose() {
  if (gGroundTiles)
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
  gGroundTiles.reset();
  for (GroundTile &t : gGround) t = GroundTile{};
  gGroundClock = 0;
}

}

int fb_stream_open(Data::SourceSet &sources, Data::Transport &transport, double lat, double lon,
                   FbGroundSurface surface) {
  TilePool::Config config;
  config.OriginLatDeg = lat;
  config.OriginLonDeg = lon;
  config.Threads = PoolThreads();
  config.ByteBudget = kByteBudget;
  config.DemCacheTiles = kPoolDemCacheTiles;
  gPool = std::make_unique<TilePool>(config, sources, transport);
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
  Geo place;
  place.LatDeg = lat;
  place.LonDeg = lon;
  const TileFrac f = ToTileFracClamped(place, gSurface.Z);
  long hx = (long)f.X, hy = (long)f.Y;
  if (!WrapTile(gSurface.Z, &hx, &hy)) return GroundSample::Missing();
  gGroundPending = false;
  const GroundTile *t = GroundTileAt(hx, hy);
  if (!t) return gGroundPending ? GroundSample::Waiting() : GroundSample::Missing();
  return GroundSample::At(
      TileHeightAslM(t->H.data(), t->Nodes, t->Postings, f.X - (double)hx, f.Y - (double)hy));
}

FbGroundBlock fb_stream_ground_block(int z, long x, long y) {
  FbGroundBlock block;
  if (z != gSurface.Z) return block;
  long hx = x, hy = y;
  if (!WrapTile(z, &hx, &hy)) return block;
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
  Geo from;
  from.LatDeg = latDeg;
  from.LonDeg = Wrapped180(lonFromDeg);
  Geo to;
  to.LatDeg = latDeg;
  to.LonDeg = Wrapped180(lonFromDeg + lonStepDeg);
  const TileFrac fromFrac = ToTileFracClamped(from, Zoom_);
  const double tx0 = fromFrac.X, ty = fromFrac.Y;
  double tx1 = ToTileFracClamped(to, Zoom_).X;

  const double width = (double)((long)1 << Zoom_);
  if ((tx1 - tx0) * lonStepDeg < 0.0) tx1 += lonStepDeg > 0.0 ? width : -width;
  const double fy = ty - (double)Y_;
  const double fx0 = tx0 - (double)X_, fxStep = tx1 - tx0;
  for (int i = 0; i < count; i++)
    out[i] = TileHeightAslM(Nodes_, Side_, Postings_, fx0 + (double)i * fxStep, fy);
}

FbStarBands fb_fetch_stars(uint8_t *dst, int cap) {
  if (!gPool) return {FbStarBands::State::Complete, 0};
  int off = 0;
  TilePool::Landing band;
  for (uint32_t b = 0; b < Data::StarBands::kBands; b++) {
    const Data::Request request(Data::DataKind::StarCatalogue, Data::Address::Whole(b));
    const TilePool::Reply reply = gPool->Bytes(request, &band);

    if (reply == TilePool::Reply::Pending) return {FbStarBands::State::Pending, 0};
    if (reply != TilePool::Reply::Ready || band.Bytes.empty()) break;
    if (off + (int)band.Bytes.size() > cap) break;
    memcpy(dst + off, band.Bytes.data(), band.Bytes.size());
    off += (int)band.Bytes.size();
  }
  return {FbStarBands::State::Complete, off};
}
