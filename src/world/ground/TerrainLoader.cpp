#include "Units.h"
#include "TerrainLoader.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <ratio>
#include <thread>
#include <utility>
#include <vector>

#include "ChunkSurface.h"
#include "GroundSample.h"
#include "Heap.h"

#include "Log.h"
#include "SourceSet.h"
#include "TerrainTiles.h"
#include "TileGeodesy.h"

using namespace outshine;
using namespace outshine::Ground;
using outshine::Ground::TilePool;

namespace {

constexpr double kPollsPerSecond = 1000.0;

constexpr double kBytesPerMB = 1048576.0;

constexpr int kMaxTileThreads = 6;

constexpr size_t kByteBudget = 64u * 1024u * 1024u;

constexpr int kPoolDemCacheTiles = 16;

constexpr int kGroundSlots = 96;
constexpr uint32_t kCoarseDrop = 3;
constexpr int kCoarseSlots = 4;

constexpr int kGroundStitchGrids = 5;
constexpr double kGroundGridBytes = 256.0 * 256.0 * 4.0;

double Clamped01(double v) {
  return std::clamp(v, 0.0, 1.0);
}

double Wrapped180(double lonDeg) {
  while (lonDeg > kDegPerHalfTurn) { lonDeg -= kDegPerTurn; }
  while (lonDeg < -kDegPerHalfTurn) { lonDeg += kDegPerTurn; }
  return lonDeg;
}

int DerivedThreads(int workers) {
  if (workers > 0 && workers <= 32) { return workers; }
  const unsigned hw = std::thread::hardware_concurrency();
  int n = hw > 3u ? static_cast<int>(hw) - 2 : 1;
  n = std::min(n, kMaxTileThreads);
  return n;
}

} // namespace

namespace outshine::Ground {

void FillNodeHeights(const TerrainField &field,
                     uint32_t rowPostings,
                     uint32_t colPostings,
                     int nodes,
                     std::vector<float> *out) {
  out->resize(static_cast<size_t>(nodes) * static_cast<size_t>(nodes));
  for (int j = 0; j < nodes; j++) {
    const double fr = PostingFrac(Ground::ChunkNodePosting(j, rowPostings, nodes), rowPostings);
    for (int i = 0; i < nodes; i++) {
      const double fc = PostingFrac(Ground::ChunkNodePosting(i, colPostings, nodes), colPostings);
      (*out)[static_cast<size_t>(j) * static_cast<size_t>(nodes) + static_cast<size_t>(i)] =
          field.PostingM(fc, fr);
    }
  }
}

double TileHeightAslM(const float *nodes, int side, uint32_t postings, double fx, double fy) {
  const double px = Clamped01(fx) * static_cast<double>(postings - 1);
  const double py = Clamped01(fy) * static_cast<double>(postings - 1);
  const int i = Ground::ChunkNodeCell(px, postings, side);
  const int j = Ground::ChunkNodeCell(py, postings, side);
  const uint32_t c0 = Ground::ChunkNodePosting(i, postings, side);
  const uint32_t c1 = Ground::ChunkNodePosting(i + 1, postings, side);
  const uint32_t r0 = Ground::ChunkNodePosting(j, postings, side);
  const uint32_t r1 = Ground::ChunkNodePosting(j + 1, postings, side);
  const auto su = static_cast<float>((px - static_cast<double>(c0)) / static_cast<double>(c1 - c0));
  const auto sv = static_cast<float>((py - static_cast<double>(r0)) / static_cast<double>(r1 - r0));
  const Ground::ChunkCell cell{.Nodes = nodes, .Stride = side, .Row = j, .Col = i};
  return static_cast<double>(Ground::ChunkCellHeight(cell, su, sv));
}

struct Tile {
  long X = 0, Y = 0;
  int Nodes = 0;
  uint32_t Postings = 0;
  std::vector<float> H;
  bool Resident = false;
  bool Hole = false;
  uint64_t Used = 0;
};

struct GroundStream::Held {
  class Oracle : public TerrainSource {
  public:
    explicit Oracle(Held &held) : Held_(held) {}

    TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
      Held_.Decodes++;
      const Data::Request request(Data::DataKind::Elevation, Data::Address::Tile(z, x, y));
      TilePool::Landing landing;
      switch (Held_.Pool.BytesBlocking(request, &landing)) {
        case TilePool::Reply::Ready: {
          int az = 0;
          uint32_t ax = 0;
          uint32_t ay = 0;
          if (!landing.At.TryTile(&az, &ax, &ay)) { return TerrainBytes::Wire(); }
          return TerrainBytes::From(az, ax, ay, std::move(landing.Bytes));
        }
        case TilePool::Reply::Absent: return TerrainBytes::Nothing();
        case TilePool::Reply::Undeclared: return TerrainBytes::Nothing();
        case TilePool::Reply::Refused: return TerrainBytes::Wire();
        case TilePool::Reply::Pending: break;
      }
      Held_.Pending = true;
      return TerrainBytes::Waiting();
    }

  private:
    Held &Held_;
  };

  explicit Held(TilePool &pool) : Pool(pool), Source(*this) {
    TerrainTiles::Config config;
    config.DemCacheTiles = kGroundStitchGrids;
    Stitched = std::make_unique<TerrainTiles>(Source, EnuFrame::At(0.0, 0.0), config);
  }

  TilePool &Pool;
  Oracle Source;
  std::unique_ptr<TerrainTiles> Stitched;
  std::array<Tile, kGroundSlots> Ground{};
  std::array<Tile, kCoarseSlots> Coarse{};
  uint64_t Clock = 0;
  long Builds = 0;
  long Decodes = 0;
  double StitchMs = 0.0;
  bool Pending = false;
};

GroundStream::GroundStream(TilePool &tiles, GroundSurface surface)
    : Tiles_(tiles), Surface_(surface), Held_(std::make_unique<Held>(tiles)) {}

GroundStream::~GroundStream() {
  if (Held_ && Held_->Builds > 0) {
    Log::Debug("world",
               "ground_oracle",
               {{"tileBuilds", static_cast<int>(Held_->Builds)},
                {"demDecodes", static_cast<int>(Held_->Decodes)},
                {"decodesPerBuild",
                 (Held_->Builds != 0)
                     ? static_cast<double>(Held_->Decodes) / static_cast<double>(Held_->Builds)
                     : 0.0},
                {"stitchMs", Held_->StitchMs},
                {"stitchMsPerBuild",
                 (Held_->Builds != 0) ? Held_->StitchMs / static_cast<double>(Held_->Builds) : 0.0},
                {"gridCache", kGroundStitchGrids},
                {"gridCacheMB", kGroundStitchGrids * kGroundGridBytes / kBytesPerMB}});
  }
}

const Tile *GroundStream::CoarseResident(long x, long y) const {
  Held &held = *Held_;
  for (Tile &t : held.Coarse) {
    if (t.Resident && t.X == x && t.Y == y) {
      t.Used = ++held.Clock;
      return t.Hole ? nullptr : &t;
    }
  }
  return nullptr;
}

void GroundStream::KeepCoarse(long x, long y) const {
  Held &held = *Held_;
  Tile *victim = &held.Coarse[0];
  for (Tile &t : held.Coarse) {
    if (t.Resident && t.X == x && t.Y == y) { return; }
    if (t.Used < victim->Used) { victim = &t; }
  }
  const int zoom = Surface_.Z - static_cast<int>(kCoarseDrop);
  if (zoom < 1) { return; }
  held.Pending = false;
  const TerrainGrid grid =
      held.Stitched->StitchedGrid(zoom, static_cast<uint32_t>(x), static_cast<uint32_t>(y));
  const TerrainField *field = grid.TryField();
  if (held.Pending) { return; }
  const uint32_t stride = held.Stitched->Stride();
  const uint32_t rowPostings = field != nullptr ? PostingsPerEdge(field->Rows(), stride) : 0;
  const uint32_t colPostings = field != nullptr ? PostingsPerEdge(field->Cols(), stride) : 0;
  const int gr = field != nullptr ? Ground::ChunkNodes(rowPostings, Surface_.Grid) : 0;
  const int gc = field != nullptr ? Ground::ChunkNodes(colPostings, Surface_.Grid) : 0;
  const bool square = gr >= 2 && gr == gc && rowPostings == colPostings;
  victim->X = x;
  victim->Y = y;
  victim->Used = ++held.Clock;
  victim->Resident = true;
  victim->Hole = !square;
  victim->Nodes = square ? gr : 0;
  victim->Postings = square ? colPostings : 0;
  if (!square) {
    victim->H.clear();
    return;
  }
  FillNodeHeights(*field, rowPostings, colPostings, victim->Nodes, &victim->H);
}

const Tile *GroundStream::TileResident(long x, long y) const {
  Held &held = *Held_;
  for (Tile &t : held.Ground) {
    if (t.Resident && t.X == x && t.Y == y) {
      t.Used = ++held.Clock;
      return t.Hole ? nullptr : &t;
    }
  }
  return nullptr;
}

GroundSample GroundStream::SampleFrom(const Tile &tile, int zoom, double lat, double lon) const {
  Geo place;
  place.LatitudeDeg = lat;
  place.LongitudeDeg = lon;
  const TileFrac f = ToTileFracClamped(place, zoom);
  const double u = f.X - static_cast<double>(static_cast<long>(f.X));
  const double v = f.Y - static_cast<double>(static_cast<long>(f.Y));
  const double step = tile.Postings > 0 ? 1.0 / static_cast<double>(tile.Postings) : 0.0;
  const double here = TileHeightAslM(tile.H.data(), tile.Nodes, tile.Postings, u, v);
  if (!(step > 0.0)) { return GroundSample::At(here); }

  const auto clamped = [](double at) { return std::clamp(at, 0.0, 1.0); };
  const double eastAt = clamped(u + step);
  const double westAt = clamped(u - step);
  const double southAt = clamped(v + step);
  const double northAt = clamped(v - step);
  const double spanM = kMercatorGirthM * std::cos(lat * kDeg2Rad) /
                       static_cast<double>(1ULL << static_cast<uint32_t>(zoom)) /
                       static_cast<double>(Surface_.Grid);
  const double acrossEastM = (eastAt - westAt) * static_cast<double>(tile.Postings) * spanM;
  const double acrossNorthM = (southAt - northAt) * static_cast<double>(tile.Postings) * spanM;
  if (!(acrossEastM > 0.0) || !(acrossNorthM > 0.0)) { return GroundSample::At(here); }

  const double byEast = (TileHeightAslM(tile.H.data(), tile.Nodes, tile.Postings, eastAt, v) -
                         TileHeightAslM(tile.H.data(), tile.Nodes, tile.Postings, westAt, v)) /
                        acrossEastM;
  const double bySouth = (TileHeightAslM(tile.H.data(), tile.Nodes, tile.Postings, u, southAt) -
                          TileHeightAslM(tile.H.data(), tile.Nodes, tile.Postings, u, northAt)) /
                         acrossNorthM;
  Vec3 normal = {{-byEast, 1.0, bySouth}};
  const double length =
      std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
  if (length > 0.0) {
    normal[0] /= length;
    normal[1] /= length;
    normal[2] /= length;
  }
  return GroundSample::At(here, normal);
}

GroundSample GroundStream::Resident(double lat, double lon) const {
  lon = Wrapped180(lon);
  Geo place;
  place.LatitudeDeg = lat;
  place.LongitudeDeg = lon;
  const TileFrac f = ToTileFracClamped(place, Surface_.Z);
  long hx = static_cast<long>(f.X);
  const long hy = static_cast<long>(f.Y);
  if (!WrapTile(Surface_.Z, &hx, &hy)) { return GroundSample::Missing(); }
  if (const Tile *fine = TileResident(hx, hy)) { return SampleFrom(*fine, Surface_.Z, lat, lon); }

  const int zoom = Surface_.Z - static_cast<int>(kCoarseDrop);
  long cx = static_cast<long>(static_cast<uint64_t>(hx) >> kCoarseDrop);
  const long cy = static_cast<long>(static_cast<uint64_t>(hy) >> kCoarseDrop);
  if (zoom >= 1 && WrapTile(zoom, &cx, &cy)) {
    if (const Tile *coarse = CoarseResident(cx, cy)) {
      return SampleFrom(*coarse, zoom, lat, lon).Coarser(kCoarseDrop);
    }
  }
  return GroundSample::Waiting();
}

const Tile *GroundStream::TileAt(long x, long y) const {
  Held &held = *Held_;
  Tile *victim = &held.Ground[0];
  for (Tile &t : held.Ground) {
    if (t.Resident && t.X == x && t.Y == y) {
      t.Used = ++held.Clock;
      return t.Hole ? nullptr : &t;
    }
    if (t.Used < victim->Used) { victim = &t; }
  }
  held.Pending = false;
  const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  {
    const ShapedGround told = Tiles_.Shaped();
    if (!told.Kind.empty()) {
      TerrainTiles::Shaped how;
      how.Kind = told.Kind;
      how.AmplitudeM = told.AmplitudeM;
      how.WavelengthM = told.WavelengthM;
      how.Gradient = told.Gradient;
      how.BearingDeg = told.BearingDeg;
      how.FocusLatDeg = told.FocusLatDeg;
      how.FocusLonDeg = told.FocusLonDeg;
      how.Seed = told.Seed;
      held.Stitched->Shapes(how);
    }
  }
  const TerrainGrid grid =
      held.Stitched->StitchedGrid(Surface_.Z, static_cast<uint32_t>(x), static_cast<uint32_t>(y));
  held.StitchMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  const TerrainField *field = grid.TryField();
  if (grid.Where() == TerrainGrid::State::Undecodable) {
    Log::Error("world",
               "ground_grid_failed",
               {{"z", Surface_.Z}, {"x", static_cast<int>(x)}, {"y", static_cast<int>(y)}});
  }
  const uint32_t stride = held.Stitched->Stride();
  const uint32_t rowPostings = field != nullptr ? PostingsPerEdge(field->Rows(), stride) : 0;
  const uint32_t colPostings = field != nullptr ? PostingsPerEdge(field->Cols(), stride) : 0;
  const int gr = field != nullptr ? Ground::ChunkNodes(rowPostings, Surface_.Grid) : 0;
  const int gc = field != nullptr ? Ground::ChunkNodes(colPostings, Surface_.Grid) : 0;
  const bool square = gr >= 2 && gr == gc && rowPostings == colPostings;
  if (held.Pending) { return nullptr; }
  held.Builds++;
  victim->X = x;
  victim->Y = y;
  victim->Used = ++held.Clock;
  victim->Resident = true;
  victim->Hole = !square;
  victim->Nodes = square ? gr : 0;
  victim->Postings = square ? colPostings : 0;
  if (!square) {
    victim->H.clear();
    return nullptr;
  }
  FillNodeHeights(*field, rowPostings, colPostings, victim->Nodes, &victim->H);
  KeepCoarse(static_cast<long>(static_cast<uint64_t>(x) >> kCoarseDrop),
             static_cast<long>(static_cast<uint64_t>(y) >> kCoarseDrop));
  return victim;
}

GroundSample GroundStream::At(double lat, double lon) const {
  lon = Wrapped180(lon);
  Geo place;
  place.LatitudeDeg = lat;
  place.LongitudeDeg = lon;
  const TileFrac f = ToTileFracClamped(place, Surface_.Z);
  long hx = static_cast<long>(f.X);
  const long hy = static_cast<long>(f.Y);
  if (!WrapTile(Surface_.Z, &hx, &hy)) { return GroundSample::Missing(); }
  Held_->Pending = false;
  const Tile *t = TileAt(hx, hy);
  if (t == nullptr) { return Held_->Pending ? GroundSample::Waiting() : GroundSample::Missing(); }
  return GroundSample::At(TileHeightAslM(t->H.data(),
                                         t->Nodes,
                                         t->Postings,
                                         f.X - static_cast<double>(hx),
                                         f.Y - static_cast<double>(hy)));
}

double GroundStream::PostM(double latDeg) const {
  return kMercatorGirthM * std::cos(latDeg * kDeg2Rad) /
         static_cast<double>(1ULL << static_cast<uint32_t>(Surface_.Z)) /
         static_cast<double>(Surface_.Grid);
}

GroundBlock GroundStream::BlockAt(int z, long x, long y) const {
  GroundBlock block;
  if (z != Surface_.Z) { return block; }
  long hx = x;
  const long hy = y;
  if (!WrapTile(z, &hx, &hy)) { return block; }
  Held_->Pending = false;
  const Tile *t = TileAt(hx, hy);
  if (t == nullptr) {
    block.Where_ = Held_->Pending ? GroundBlock::State::Pending : GroundBlock::State::Missing;
    return block;
  }
  block.Nodes_ = t->H.data();
  block.X_ = hx;
  block.Y_ = hy;
  block.Zoom_ = z;
  block.Side_ = t->Nodes;
  block.Postings_ = t->Postings;
  block.Where_ = GroundBlock::State::Resolved;
  return block;
}

void GroundBlock::AslMRow(
    double latDeg, double lonFromDeg, double lonStepDeg, int count, double *out) const noexcept {
  Geo from;
  from.LatitudeDeg = latDeg;
  from.LongitudeDeg = Wrapped180(lonFromDeg);
  Geo to;
  to.LatitudeDeg = latDeg;
  to.LongitudeDeg = Wrapped180(lonFromDeg + lonStepDeg);
  const TileFrac fromFrac = ToTileFracClamped(from, Zoom_);
  const double tx0 = fromFrac.X;
  const double ty = fromFrac.Y;
  double tx1 = ToTileFracClamped(to, Zoom_).X;

  const auto width = static_cast<double>(1ULL << static_cast<uint32_t>(Zoom_));
  if ((tx1 - tx0) * lonStepDeg < 0.0) { tx1 += lonStepDeg > 0.0 ? width : -width; }
  const double fy = ty - static_cast<double>(Y_);
  const double fx0 = tx0 - static_cast<double>(X_);
  const double fxStep = tx1 - tx0;
  for (int i = 0; i < count; i++) {
    out[i] = TileHeightAslM(Nodes_, Side_, Postings_, fx0 + static_cast<double>(i) * fxStep, fy);
  }
}

TilePool::Config GroundPoolConfig(double lat, double lon, int workers, double patienceS) {
  TilePool::Config config;
  config.OriginLatDeg = lat;
  config.OriginLonDeg = lon;
  config.Threads = DerivedThreads(workers);
  config.ByteBudget = kByteBudget;
  config.DemCacheTiles = kPoolDemCacheTiles;
  if (patienceS > 0.0) { config.PollAttempts = static_cast<int>(patienceS * kPollsPerSecond); }
  return config;
}

} // namespace outshine::Ground
