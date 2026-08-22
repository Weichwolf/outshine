#include "TerrainTiles.h"

#include <algorithm>
#include <limits>

namespace outshine::Ground {

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

}

TerrainTiles::TerrainTiles(TerrainSource &source, EnuFrame frame, Config config)
    : Source_(source), Frame_(frame), Config_(config) {
  if (Config_.Stride == 0) Config_.Stride = 1;
  const int slots = (config.DemCacheTiles > 0 && config.DemCacheTiles < kDemCacheCeiling)
                        ? config.DemCacheTiles
                        : kDemCacheCeiling;
  Cache_.resize((size_t)slots);
}

const TerrainField *TerrainTiles::CacheLookup(int z, uint32_t x, uint32_t y) {
  for (CacheEntry &e : Cache_) {
    if (!e.Used || e.Z != z || e.X != x || e.Y != y) continue;
    e.Seq = ++Seq_;
    return &e.Field;
  }
  return nullptr;
}

void TerrainTiles::CacheStore(int z, uint32_t x, uint32_t y, const TerrainField &field) {
  if (Cache_.empty() || !field.Meshable()) return;
  CacheEntry *victim = &Cache_[0];
  uint64_t oldest = std::numeric_limits<uint64_t>::max();
  for (CacheEntry &e : Cache_) {
    if (!e.Used) { victim = &e; break; }
    if (e.Seq < oldest) { oldest = e.Seq; victim = &e; }
  }
  victim->Used = true;
  victim->Z = z;
  victim->X = x;
  victim->Y = y;
  victim->Field = field;
  victim->Seq = ++Seq_;
}

TerrainGrid TerrainTiles::RawGrid(int z, uint32_t x, uint32_t y) {
  if (const TerrainField *cached = CacheLookup(z, x, y)) return TerrainGrid::Holding(TerrainField(*cached));

  TerrainBytes answer = Source_.Take(z, x, y);
  int sourceZ = 0;
  uint32_t sourceX = 0, sourceY = 0;
  std::vector<uint8_t> png;
  if (!answer.TryTake(&sourceZ, &sourceX, &sourceY, &png)) {
    switch (answer.Where()) {
      case TerrainBytes::State::Deferred: return TerrainGrid::Deferred();
      case TerrainBytes::State::NoTile: return TerrainGrid::NotHere();
      case TerrainBytes::State::Refused:
      case TerrainBytes::State::Delivered: return TerrainGrid::Refused();
    }
  }

  const int steps = z - sourceZ;
  if (steps < 0 || steps >= 24) return TerrainGrid::Refused();
  const uint32_t subDiv = 1u << steps;
  const uint32_t subX = x & (subDiv - 1);
  const uint32_t subY = y & (subDiv - 1);

  TerrainGrid grid = TerrainGrid::FromTerrariumPng(png.data(), png.size());
  TerrainField *field = grid.TryFieldMutable();
  if (!field) return grid;

  if (subDiv > 1) {
    const uint32_t cropCols = field->Cols() / subDiv;
    const uint32_t cropRows = field->Rows() / subDiv;
    if (cropCols < 2 || cropRows < 2) return TerrainGrid::NotHere();

    TerrainField cropped(cropRows, cropCols);
    for (uint32_t r = 0; r < cropRows; r++)
      for (uint32_t c = 0; c < cropCols; c++)
        cropped.SetM(r, c, field->AtM(subY * cropRows + r, subX * cropCols + c));
    grid = TerrainGrid::Holding(std::move(cropped));
    field = grid.TryFieldMutable();
  }

  CacheStore(z, x, y, *field);
  return grid;
}

TerrainGrid::State TerrainTiles::StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny,
                                           Side side) {
  TerrainGrid neighbour = RawGrid(z, nx, ny);
  const TerrainField *n = neighbour.TryField();
  if (!n || !n->Meshable()) return neighbour.Where();

  if (side == Side::West || side == Side::East) {
    const uint32_t selfCol = (side == Side::West) ? 0 : self.Cols() - 1;
    const uint32_t neighbourCol = (side == Side::West) ? n->Cols() - 1 : 0;
    const uint32_t rows = std::min(self.Rows(), n->Rows());
    for (uint32_t r = 0; r < rows; r++)
      self.SetM(r, selfCol, 0.5f * (self.AtM(r, selfCol) + n->AtM(r, neighbourCol)));
  } else {
    const uint32_t selfRow = (side == Side::North) ? 0 : self.Rows() - 1;
    const uint32_t neighbourRow = (side == Side::North) ? n->Rows() - 1 : 0;
    const uint32_t cols = std::min(self.Cols(), n->Cols());
    for (uint32_t c = 0; c < cols; c++)
      self.SetM(selfRow, c, 0.5f * (self.AtM(selfRow, c) + n->AtM(neighbourRow, c)));
  }
  return TerrainGrid::State::Decoded;
}

TerrainGrid TerrainTiles::StitchedGrid(int z, uint32_t x, uint32_t y) {
  TerrainGrid grid = RawGrid(z, x, y);
  TerrainField *field = grid.TryFieldMutable();
  if (!field) return grid;

  TerrainGrid::State worst = TerrainGrid::State::Decoded;
  const uint32_t n = 1u << z;
  if (x > 0) worst = Worse(worst, StitchEdge(*field, z, x - 1, y, Side::West));
  if (x + 1 < n) worst = Worse(worst, StitchEdge(*field, z, x + 1, y, Side::East));
  if (y > 0) worst = Worse(worst, StitchEdge(*field, z, x, y - 1, Side::North));
  if (y + 1 < n) worst = Worse(worst, StitchEdge(*field, z, x, y + 1, Side::South));
  if (worst == TerrainGrid::State::Refused) return TerrainGrid::Refused();
  if (worst == TerrainGrid::State::Deferred) return TerrainGrid::Deferred();
  return grid;
}

TerrainMesh TerrainTiles::MeshOf(int z, uint32_t x, uint32_t y) {
  const TerrainGrid grid = StitchedGrid(z, x, y);
  const TerrainField *field = grid.TryField();
  if (!field) {
    switch (grid.Where()) {
      case TerrainGrid::State::Undecodable: return TerrainMesh::Nothing(TerrainMesh::State::SourceUndecodable);
      case TerrainGrid::State::Deferred: return TerrainMesh::Nothing(TerrainMesh::State::Deferred);
      case TerrainGrid::State::Refused: return TerrainMesh::Nothing(TerrainMesh::State::SourceRefused);
      case TerrainGrid::State::NotHere:
      case TerrainGrid::State::Decoded: return TerrainMesh::Nothing(TerrainMesh::State::NoTile);
    }
  }
  return TerrainMesh::Over(*field, TileEnuMap::Over(Frame_, z, x, y, 4096), Config_.Stride);
}

size_t TerrainTiles::HeapBytes() const {
  size_t bytes = sizeof(*this);
  for (const CacheEntry &e : Cache_)
    if (e.Used) bytes += e.Field.Bytes();
  return bytes;
}

}
