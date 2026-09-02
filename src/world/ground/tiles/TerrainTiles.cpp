#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "Units.h"
#include "math/Vec4.h"
#include "TerrainTiles.h"

#include <algorithm>
#include <limits>

namespace outshine::Ground {

constexpr int kZoomMost = 24;

constexpr double kWaveShoulder = 0.25;
constexpr double kQuarterOfFour = 0.25;

namespace {

constexpr int kDemCacheCeiling = 128;

[[nodiscard]] int Severity(TerrainGrid::State s) {
  switch (s) {
    case TerrainGrid::State::Refused: return 3;
    case TerrainGrid::State::Deferred: return 2;
    case TerrainGrid::State::Undecodable: return 1;
    case TerrainGrid::State::NotHere:
    case TerrainGrid::State::Decoded: return 0;
  }
  return 0;
}

[[nodiscard]] TerrainGrid::State Worse(TerrainGrid::State a, TerrainGrid::State b) {
  return Severity(a) >= Severity(b) ? a : b;
}

} // namespace

TerrainTiles::TerrainTiles(TerrainSource &source, EnuFrame frame, Config config)
    : Source_(source), Frame_(frame), Config_(config) {
  if (Config_.Stride == 0) { Config_.Stride = 1; }
  const int slots = (config.DemCacheTiles > 0 && config.DemCacheTiles < kDemCacheCeiling)
                        ? config.DemCacheTiles
                        : kDemCacheCeiling;
  Cache_.resize(static_cast<size_t>(slots));
}

const TerrainField *TerrainTiles::CacheLookup(int z, uint32_t x, uint32_t y) {
  for (CacheEntry &e : Cache_) {
    if (!e.Used || e.Z != z || e.X != x || e.Y != y) { continue; }
    e.Seq = ++Seq_;
    return &e.Field;
  }
  return nullptr;
}

void TerrainTiles::CacheStore(int z, uint32_t x, uint32_t y, const TerrainField &field) {
  if (Cache_.empty() || !field.Meshable()) { return; }
  CacheEntry *victim = Cache_.data();
  uint64_t oldest = std::numeric_limits<uint64_t>::max();
  for (CacheEntry &e : Cache_) {
    if (!e.Used) {
      victim = &e;
      break;
    }
    if (e.Seq < oldest) {
      oldest = e.Seq;
      victim = &e;
    }
  }
  victim->Used = true;
  victim->Z = z;
  victim->X = x;
  victim->Y = y;
  victim->Field = field;
  victim->Seq = ++Seq_;
}

double TerrainTiles::ShapedAslM(double latDeg, double lonDeg) const noexcept {
  const double perLon = kMPerDegLon * std::cos(Shape_.FocusLatDeg * kPi / kDegPerHalfTurn);
  const double eastM = (lonDeg - Shape_.FocusLonDeg) * perLon;
  const double northM = (latDeg - Shape_.FocusLatDeg) * kMPerDegLat;
  const double facing = Shape_.BearingDeg * kPi / kDegPerHalfTurn;
  const double along = eastM * std::sin(facing) + northM * std::cos(facing);
  const double across = eastM * std::cos(facing) - northM * std::sin(facing);
  const double wave = Shape_.WavelengthM > 1.0 ? Shape_.WavelengthM : 1.0;
  const double turn = 2.0 * kPi;
  double up = Shape_.Gradient * along;
  if (Shape_.Kind == "sineRidge") {
    up += Shape_.AmplitudeM * std::cos(turn * across / wave);
  } else if (Shape_.Kind == "sineValley") {
    up -= Shape_.AmplitudeM * std::cos(turn * across / wave);
  } else if (Shape_.Kind == "sineGrid") {
    up += Shape_.AmplitudeM * std::sin(turn * along / wave) * std::sin(turn * across / wave);
  } else if (Shape_.Kind == "escarpment" || Shape_.Kind == "noiseEscarpment") {
    up += Shape_.AmplitudeM * std::tanh(across / (kWaveShoulder * wave));
  }
  if (Shape_.Kind == "noise" || Shape_.Kind == "noiseEscarpment") {
    double octave = 1.0;
    double weight = 0.5;
    for (int step = 0; step < 5; ++step) {
      up += Shape_.AmplitudeM * weight * std::sin(turn * along * octave / wave) *
            std::cos(turn * across * octave / wave);
      octave *= 2.0;
      weight *= 0.5;
    }
  }
  return up;
}

TerrainGrid TerrainTiles::RawGrid(int z, uint32_t x, uint32_t y) {
  if (IsShaped()) {
    constexpr uint32_t kShapedSide = 257;
    TerrainField field(kShapedSide, kShapedSide);
    const TerrainField::Writable postings = field.Field();
    for (uint32_t row = 0; row < kShapedSide; ++row) {
      for (uint32_t col = 0; col < kShapedSide; ++col) {
        const double fx = static_cast<double>(col) / static_cast<double>(kShapedSide - 1u);
        const double fy = static_cast<double>(row) / static_cast<double>(kShapedSide - 1u);
        const Geo at = TileFracToGeo(z, static_cast<long>(x), static_cast<long>(y), fx, fy);
        postings[row, col] = static_cast<float>(ShapedAslM(at.LatitudeDeg, at.LongitudeDeg));
      }
    }
    return TerrainGrid::Holding(std::move(field));
  }
  if (const TerrainField *cached = CacheLookup(z, x, y)) {
    return TerrainGrid::Holding(TerrainField(*cached));
  }

  TerrainBytes answer = Source_.Take(Data::TileId{.Zoom = z, .X = x, .Y = y});
  auto delivered = answer.Take();
  if (!delivered) {
    switch (answer.Where()) {
      case TerrainBytes::State::Deferred: return TerrainGrid::Deferred();
      case TerrainBytes::State::NoTile: return TerrainGrid::NotHere();
      case TerrainBytes::State::Refused:
      case TerrainBytes::State::Delivered: return TerrainGrid::Refused();
    }
  }
  const Data::TileId source = delivered->first;
  std::vector<uint8_t> png = std::move(delivered->second);

  const int steps = z - source.Zoom;
  if (steps < 0 || steps >= kZoomMost) { return TerrainGrid::Refused(); }
  const uint32_t subDiv = 1u << static_cast<uint32_t>(steps);
  const uint32_t subX = x & (subDiv - 1);
  const uint32_t subY = y & (subDiv - 1);

  TerrainGrid grid = TerrainGrid::FromTerrariumPng(png.data(), png.size());
  const TerrainField *field = grid.TryFieldMutable();
  if (field == nullptr) { return grid; }

  if (subDiv > 1) {
    const uint32_t cropCols = field->Cols() / subDiv;
    const uint32_t cropRows = field->Rows() / subDiv;
    if (cropCols < 2 || cropRows < 2) { return TerrainGrid::NotHere(); }

    TerrainField cropped(cropRows, cropCols);
    for (uint32_t r = 0; r < cropRows; r++) {
      for (uint32_t c = 0; c < cropCols; c++) {
        cropped.SetM(r, c, field->AtM(subY * cropRows + r, subX * cropCols + c));
      }
    }
    grid = TerrainGrid::Holding(std::move(cropped));
    field = grid.TryFieldMutable();
  }

  if (!field->Meshable()) { return TerrainGrid::NotHere(); }
  CacheStore(z, x, y, *field);
  return grid;
}

TerrainGrid::State
TerrainTiles::StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny, Side side) {
  const TerrainGrid neighbour = RawGrid(z, nx, ny);
  const TerrainField *n = neighbour.TryField();
  if ((n == nullptr) || !n->Meshable()) { return neighbour.Where(); }

  if (side == Side::West || side == Side::East) {
    const uint32_t selfCol = (side == Side::West) ? 0 : self.Cols() - 1;
    const double neighbourFrac = (side == Side::West) ? 1.0 : 0.0;
    for (uint32_t r = 0; r < self.Rows(); r++) {
      const double along = PostingFrac(r, self.Rows());
      self.SetM(r, selfCol, 0.5f * (self.AtM(r, selfCol) + n->PostingM(neighbourFrac, along)));
    }
  } else {
    const uint32_t selfRow = (side == Side::North) ? 0 : self.Rows() - 1;
    const double neighbourFrac = (side == Side::North) ? 1.0 : 0.0;
    for (uint32_t c = 0; c < self.Cols(); c++) {
      const double along = PostingFrac(c, self.Cols());
      self.SetM(selfRow, c, 0.5f * (self.AtM(selfRow, c) + n->PostingM(along, neighbourFrac)));
    }
  }
  return TerrainGrid::State::Decoded;
}

TerrainGrid::State TerrainTiles::StitchCorner(
    TerrainField &self, float selfRawM, int z, uint32_t x, uint32_t y, Corner corner) {
  const bool west = corner == Corner::NorthWest || corner == Corner::SouthWest;
  const bool north = corner == Corner::NorthWest || corner == Corner::NorthEast;
  const uint32_t n = 1u << static_cast<uint32_t>(z);
  if ((west && x == 0) || (!west && x + 1 >= n)) { return TerrainGrid::State::Decoded; }
  if ((north && y == 0) || (!north && y + 1 >= n)) { return TerrainGrid::State::Decoded; }

  const uint32_t acrossX = west ? x - 1 : x + 1;
  const uint32_t acrossY = north ? y - 1 : y + 1;
  const TerrainGrid sideways = RawGrid(z, acrossX, y);
  const TerrainGrid updown = RawGrid(z, x, acrossY);
  const TerrainGrid diagonal = RawGrid(z, acrossX, acrossY);
  const TerrainField *a = sideways.TryField();
  const TerrainField *b = updown.TryField();
  const TerrainField *c = diagonal.TryField();
  if ((a == nullptr) || (b == nullptr) || (c == nullptr) || !a->Meshable() || !b->Meshable() ||
      !c->Meshable()) {
    return Worse(Worse(sideways.Where(), updown.Where()), diagonal.Where());
  }

  const auto cornerOf = [](const TerrainField &f, bool atWest, bool atNorth) {
    return static_cast<double>(f.AtM(atNorth ? 0u : f.Rows() - 1u, atWest ? 0u : f.Cols() - 1u));
  };
  const double sum = static_cast<double>(selfRawM) + cornerOf(*a, !west, north) +
                     cornerOf(*b, west, !north) + cornerOf(*c, !west, !north);
  self.SetM(north ? 0u : self.Rows() - 1u,
            west ? 0u : self.Cols() - 1u,
            static_cast<float>(sum * kQuarterOfFour));
  return TerrainGrid::State::Decoded;
}

TerrainGrid TerrainTiles::StitchedGrid(int z, uint32_t x, uint32_t y) {
  TerrainGrid grid = RawGrid(z, x, y);
  TerrainField *field = grid.TryFieldMutable();
  if (field == nullptr) { return grid; }

  const Vec4f rawCorners = {{field->AtM(0u, 0u),
                             field->AtM(0u, field->Cols() - 1u),
                             field->AtM(field->Rows() - 1u, 0u),
                             field->AtM(field->Rows() - 1u, field->Cols() - 1u)}};

  TerrainGrid::State worst = TerrainGrid::State::Decoded;
  const uint32_t n = 1u << static_cast<uint32_t>(z);
  if (x > 0) { worst = Worse(worst, StitchEdge(*field, z, x - 1, y, Side::West)); }
  if (x + 1 < n) { worst = Worse(worst, StitchEdge(*field, z, x + 1, y, Side::East)); }
  if (y > 0) { worst = Worse(worst, StitchEdge(*field, z, x, y - 1, Side::North)); }
  if (y + 1 < n) { worst = Worse(worst, StitchEdge(*field, z, x, y + 1, Side::South)); }
  for (const Corner corner :
       {Corner::NorthWest, Corner::NorthEast, Corner::SouthWest, Corner::SouthEast}) {
    const bool west = corner == Corner::NorthWest || corner == Corner::SouthWest;
    const bool north = corner == Corner::NorthWest || corner == Corner::NorthEast;
    worst = Worse(
        worst,
        StitchCorner(*field, rawCorners[(west ? 0u : 1u) + (north ? 0u : 2u)], z, x, y, corner));
  }
  if (worst == TerrainGrid::State::Refused) { return TerrainGrid::Refused(); }
  if (worst == TerrainGrid::State::Deferred) { return TerrainGrid::Deferred(); }
  return grid;
}

TerrainMesh TerrainTiles::MeshOf(int z, uint32_t x, uint32_t y) {
  const TerrainGrid grid = StitchedGrid(z, x, y);
  const TerrainField *field = grid.TryField();
  if (field == nullptr) {
    switch (grid.Where()) {
      case TerrainGrid::State::Undecodable:
        return TerrainMesh::Nothing(TerrainMesh::State::SourceUndecodable);
      case TerrainGrid::State::Deferred: return TerrainMesh::Nothing(TerrainMesh::State::Deferred);
      case TerrainGrid::State::Refused:
        return TerrainMesh::Nothing(TerrainMesh::State::SourceRefused);
      case TerrainGrid::State::NotHere:
      case TerrainGrid::State::Decoded: return TerrainMesh::Nothing(TerrainMesh::State::NoTile);
    }
  }
  constexpr uint32_t kTileExtent = 4096;
  return TerrainMesh::Over(*field, TileEnuMap::Over(Frame_, z, x, y, kTileExtent), Config_.Stride);
}

size_t TerrainTiles::HeapBytes() const {
  size_t bytes = sizeof(*this);
  for (const CacheEntry &e : Cache_) {
    if (e.Used) { bytes += e.Field.Bytes(); }
  }
  return bytes;
}

} // namespace outshine::Ground
