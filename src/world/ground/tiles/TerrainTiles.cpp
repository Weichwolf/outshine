#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "math/Units.h"
#include "math/Vec4.h"
#include "TerrainTiles.h"
#include "ChunkSurface.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>

namespace outshine::Ground {

constexpr int kZoomMost = 24;

constexpr double kWaveShoulder = 0.25;
constexpr double kQuarterOfFour = 0.25;

namespace {

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
    : Source_(source), Frame_(frame), Config_(std::move(config)) {
  if (Config_.Stride == 0) { Config_.Stride = 1; }
  SharesDecoded_ = Config_.Shared != nullptr;
  Decoded_ =
      SharesDecoded_ ? Config_.Shared : std::make_shared<DecodedCache>(Config_.DemCacheBytes);
}

bool DecodedCache::Take(Data::TileId of, TerrainField *out) {
  const std::scoped_lock lock(Lock_);
  for (Entry &one : Held_) {
    if (one.Of == of) {
      one.Seq = ++Seq_;
      *out = one.Field;
      return true;
    }
  }
  return false;
}

void DecodedCache::Store(Data::TileId of, const TerrainField &field) {
  if (!field.Meshable() || field.Bytes() > Budget_) { return; }
  const std::scoped_lock lock(Lock_);
  for (const Entry &one : Held_) {
    if (one.Of == of) { return; }
  }
  size_t held = field.Bytes();
  for (const Entry &one : Held_) { held += one.Field.Bytes(); }
  while (held > Budget_ && !Held_.empty()) {
    size_t oldest = 0;
    for (size_t at = 1; at < Held_.size(); ++at) {
      if (Held_[at].Seq < Held_[oldest].Seq) { oldest = at; }
    }
    held -= Held_[oldest].Field.Bytes();
    Held_[oldest] = std::move(Held_.back());
    Held_.pop_back();
  }
  Held_.push_back({.Seq = ++Seq_, .Of = of, .Field = field});
}

size_t DecodedCache::Bytes() const {
  const std::scoped_lock lock(Lock_);
  size_t bytes = 0;
  for (const Entry &one : Held_) { bytes += one.Field.Bytes(); }
  return bytes;
}

double TerrainTiles::ShapedAslM(LongitudeLatitude at) const noexcept {
  const double perLon = kMPerDegLon * std::cos(Shape_.FocusLatDeg * kPi / kDegPerHalfTurn);
  const double eastM = (at.LongitudeDeg - Shape_.FocusLonDeg) * perLon;
  const double northM = (at.LatitudeDeg - Shape_.FocusLatDeg) * kMPerDegLat;
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

TerrainGrid TerrainTiles::RawGrid(Data::TileId of) {
  if (IsShaped()) {
    constexpr uint32_t kShapedSide = 257;
    TerrainField field(kShapedSide, kShapedSide);
    const TerrainField::Writable postings = field.Field();
    for (uint32_t row = 0; row < kShapedSide; ++row) {
      for (uint32_t col = 0; col < kShapedSide; ++col) {
        const double fx = static_cast<double>(col) / static_cast<double>(kShapedSide - 1u);
        const double fy = static_cast<double>(row) / static_cast<double>(kShapedSide - 1u);
        const Geo stands = TileFracToGeo(
            {.X = static_cast<double>(of.X) + fx, .Y = static_cast<double>(of.Y) + fy}, of.Zoom);
        postings[row, col] = static_cast<float>(
            ShapedAslM({.LongitudeDeg = stands.LongitudeDeg, .LatitudeDeg = stands.LatitudeDeg}));
      }
    }
    return TerrainGrid::Holding(std::move(field));
  }
  {
    TerrainField cached;
    if (Decoded_->Take(of, &cached)) { return TerrainGrid::Holding(std::move(cached)); }
  }

  TerrainBytes answer = Source_.Take(of);
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

  const int steps = of.Zoom - source.Zoom;
  if (steps < 0 || steps >= kZoomMost) { return TerrainGrid::Refused(); }
  const uint32_t subDiv = 1u << static_cast<uint32_t>(steps);
  const uint32_t subX = of.X & (subDiv - 1);
  const uint32_t subY = of.Y & (subDiv - 1);

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
  Decoded_->Store(of, *field);
  return grid;
}

TerrainGrid::State
TerrainTiles::StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny, Side side) {
  const TerrainGrid neighbour = RawGrid({.Zoom = z, .X = nx, .Y = ny});
  const TerrainField *n = neighbour.TryField();
  if ((n == nullptr) || !n->Meshable()) { return neighbour.Where(); }

  if (side == Side::West || side == Side::East) {
    const uint32_t selfCol = (side == Side::West) ? 0 : self.Cols() - 1;
    const double neighbourFrac = (side == Side::West) ? 1.0 : 0.0;
    for (uint32_t r = 0; r < self.Rows(); r++) {
      const double along = PostingFrac(r, self.Rows());
      self.SetM(r,
                selfCol,
                0.5f * (self.AtM(r, selfCol) + n->PostingM({.Col = neighbourFrac, .Row = along})));
    }
  } else {
    const uint32_t selfRow = (side == Side::North) ? 0 : self.Rows() - 1;
    const double neighbourFrac = (side == Side::North) ? 1.0 : 0.0;
    for (uint32_t c = 0; c < self.Cols(); c++) {
      const double along = PostingFrac(c, self.Cols());
      self.SetM(selfRow,
                c,
                0.5f * (self.AtM(selfRow, c) + n->PostingM({.Col = along, .Row = neighbourFrac})));
    }
  }
  return TerrainGrid::State::Decoded;
}

TerrainGrid::State
TerrainTiles::StitchCorner(TerrainField &self, float selfRawM, Data::TileId of, Corner corner) {
  const int z = of.Zoom;
  const uint32_t x = of.X;
  const uint32_t y = of.Y;
  const bool west = corner == Corner::NorthWest || corner == Corner::SouthWest;
  const bool north = corner == Corner::NorthWest || corner == Corner::NorthEast;
  const uint32_t n = 1u << static_cast<uint32_t>(z);
  if ((west && x == 0) || (!west && x + 1 >= n)) { return TerrainGrid::State::Decoded; }
  if ((north && y == 0) || (!north && y + 1 >= n)) { return TerrainGrid::State::Decoded; }

  const uint32_t acrossX = west ? x - 1 : x + 1;
  const uint32_t acrossY = north ? y - 1 : y + 1;
  const TerrainGrid sideways = RawGrid({.Zoom = z, .X = acrossX, .Y = y});
  const TerrainGrid updown = RawGrid({.Zoom = z, .X = x, .Y = acrossY});
  const TerrainGrid diagonal = RawGrid({.Zoom = z, .X = acrossX, .Y = acrossY});
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

std::shared_ptr<const TerrainField> TerrainTiles::HeldStitched(Data::TileId of) const {
  for (const StitchedEntry &held : Stitched_) {
    if (held.Of == of) { return held.Field; }
  }
  return nullptr;
}

std::shared_ptr<const TerrainField> TerrainTiles::StitchedField(int z, uint32_t x, uint32_t y) {
  const Data::TileId of{.Zoom = z, .X = x, .Y = y};
  for (StitchedEntry &held : Stitched_) {
    if (held.Of == of) {
      held.Seq = ++Seq_;
      return held.Field;
    }
  }
  TerrainGrid grid = StitchedGrid(z, x, y);
  TerrainField *field = grid.TryFieldMutable();
  if (field == nullptr) { return nullptr; }
  auto shared = std::make_shared<const TerrainField>(std::move(*field));
  HoldsStitched(of, shared);
  return shared;
}

void TerrainTiles::HoldsStitched(Data::TileId of,
                                 const std::shared_ptr<const TerrainField> &shared) {
  if (!shared || Config_.StitchedFieldBytes == 0 || shared->Bytes() > Config_.StitchedFieldBytes) {
    return;
  }
  size_t held = shared->Bytes();
  for (const StitchedEntry &one : Stitched_) { held += one.Field->Bytes(); }
  while (held > Config_.StitchedFieldBytes && !Stitched_.empty()) {
    size_t oldest = 0;
    for (size_t at = 1; at < Stitched_.size(); ++at) {
      if (Stitched_[at].Seq < Stitched_[oldest].Seq) { oldest = at; }
    }
    held -= Stitched_[oldest].Field->Bytes();
    Stitched_[oldest] = std::move(Stitched_.back());
    Stitched_.pop_back();
  }
  Stitched_.push_back({.Seq = ++Seq_, .Of = of, .Field = shared});
}

TerrainGrid TerrainTiles::StitchedGrid(int z, uint32_t x, uint32_t y) {
  TerrainGrid grid = RawGrid({.Zoom = z, .X = x, .Y = y});
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
    worst = Worse(worst,
                  StitchCorner(*field,
                               rawCorners[(west ? 0u : 1u) + (north ? 0u : 2u)],
                               {.Zoom = z, .X = x, .Y = y},
                               corner));
  }
  if (worst == TerrainGrid::State::Refused) { return TerrainGrid::Refused(); }
  if (worst == TerrainGrid::State::Deferred) { return TerrainGrid::Deferred(); }
  return grid;
}

void FillNodeHeights(const TerrainField &field,
                     uint32_t rowPostings,
                     uint32_t colPostings,
                     int nodes,
                     std::vector<float> *out) {
  out->resize(static_cast<size_t>(nodes) * static_cast<size_t>(nodes));
  for (int j = 0; j < nodes; j++) {
    const double fr = PostingFrac(ChunkNodePosting(j, rowPostings, nodes), rowPostings);
    for (int i = 0; i < nodes; i++) {
      const double fc = PostingFrac(ChunkNodePosting(i, colPostings, nodes), colPostings);
      (*out)[static_cast<size_t>(j) * static_cast<size_t>(nodes) + static_cast<size_t>(i)] =
          field.PostingM({.Col = fc, .Row = fr});
    }
  }
}

TerrainGrid::State TerrainTiles::NodesOf(
    Data::TileId of, int grid, std::vector<float> *out, uint32_t *postings, int *side) {
  out->clear();
  *postings = 0;
  *side = 0;
  const TerrainGrid stitched = StitchedGrid(of.Zoom, of.X, of.Y);
  const TerrainField *field = stitched.TryField();
  if (field == nullptr) { return stitched.Where(); }
  const uint32_t rows = PostingsPerEdge(field->Rows(), Config_.Stride);
  const uint32_t cols = PostingsPerEdge(field->Cols(), Config_.Stride);
  const int nodes = ChunkNodes({.Postings = rows, .Grid = grid});
  if (nodes < 2 || nodes != ChunkNodes({.Postings = cols, .Grid = grid})) {
    return TerrainGrid::State::Refused;
  }
  FillNodeHeights(*field, rows, cols, nodes, out);
  *postings = cols;
  *side = nodes;
  return TerrainGrid::State::Decoded;
}

size_t TerrainTiles::HeapBytes() const {
  size_t bytes = sizeof(*this);
  if (!SharesDecoded_) { bytes += Decoded_->Bytes(); }
  for (const StitchedEntry &held : Stitched_) {
    if (held.Field) { bytes += held.Field->Bytes(); }
  }
  return bytes;
}

} // namespace outshine::Ground
