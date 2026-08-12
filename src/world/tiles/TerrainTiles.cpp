#include "TerrainTiles.h"

#include <algorithm>
#include <limits>

namespace outshine::World {

namespace {

/* The raw fetch is hit ~15x per output tile (self + 4 stitch neighbours, each re-stitching), so
 * caching the DECODED grid short-circuits the dominant cold-boot cost: the PNG decode. */
constexpr int kDemCacheCeiling = 128;

}  // namespace

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
    e.Seq = ++Seq_;   /* touch */
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

  /* Past the source's max zoom: step up to the parent and crop. A tile server has no header stating
   * its max zoom, so the host declares it. */
  int sourceZ = z;
  uint32_t sourceX = x, sourceY = y;
  uint32_t subX = 0, subY = 0, subDiv = 1;
  if (Config_.SourceMaxZoom > 0 && z > Config_.SourceMaxZoom) {
    const int dz = z - Config_.SourceMaxZoom;
    if (dz > 16) return TerrainGrid::NotHere();   /* absurd gap */
    sourceZ = Config_.SourceMaxZoom;
    subDiv = 1u << dz;
    sourceX = x >> dz;
    sourceY = y >> dz;
    subX = x & (subDiv - 1);
    subY = y & (subDiv - 1);
  }

  const std::vector<uint8_t> png = Source_.TakeTerrainPng(sourceZ, sourceX, sourceY);
  if (png.empty()) return TerrainGrid::NotHere();

  TerrainGrid grid = TerrainGrid::FromTerrariumPng(png.data(), png.size());
  TerrainField *field = grid.TryFieldMutable();
  if (!field) return grid;

  if (subDiv > 1) {
    const uint32_t cropCols = field->Cols() / subDiv;
    const uint32_t cropRows = field->Rows() / subDiv;
    if (cropCols < 2 || cropRows < 2) return TerrainGrid::NotHere();
    /* Exactly the sub-tile's own texels, no overlap column: a texel is an AREA, so the sub-tile is
     * covered by cropCols of them and the value ON its border comes from the stitch, the same way an
     * uncropped tile gets it. An extra column would put a sample half a texel outside. */
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

void TerrainTiles::StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny, Side side) {
  TerrainGrid neighbour = RawGrid(z, nx, ny);
  const TerrainField *n = neighbour.TryField();
  if (!n || !n->Meshable()) return;

  /* Dimension mismatches (parent-edge cropping shaves a row) are tolerated — only the overlapping
   * range is averaged. */
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
}

TerrainGrid TerrainTiles::StitchedGrid(int z, uint32_t x, uint32_t y) {
  TerrainGrid grid = RawGrid(z, x, y);
  TerrainField *field = grid.TryFieldMutable();
  if (!field) return grid;

  /* The map's own width at this rung. Past it there is no tile to average with. */
  const uint32_t n = 1u << z;
  if (x > 0) StitchEdge(*field, z, x - 1, y, Side::West);
  if (x + 1 < n) StitchEdge(*field, z, x + 1, y, Side::East);
  if (y > 0) StitchEdge(*field, z, x, y - 1, Side::North);
  if (y + 1 < n) StitchEdge(*field, z, x, y + 1, Side::South);
  return grid;
}

TerrainMesh TerrainTiles::MeshOf(int z, uint32_t x, uint32_t y) {
  const TerrainGrid grid = StitchedGrid(z, x, y);
  const TerrainField *field = grid.TryField();
  if (!field)
    return TerrainMesh::Nothing(grid.Where() == TerrainGrid::State::Undecodable
                                    ? TerrainMesh::State::SourceUndecodable
                                    : TerrainMesh::State::NoTile);
  return TerrainMesh::Over(*field, TileEnuMap::Over(Frame_, z, x, y, 4096), Config_.Stride);
}

size_t TerrainTiles::HeapBytes() const {
  size_t bytes = sizeof(*this);
  for (const CacheEntry &e : Cache_)
    if (e.Used) bytes += e.Field.Bytes();
  return bytes;
}

}  // namespace outshine::World
