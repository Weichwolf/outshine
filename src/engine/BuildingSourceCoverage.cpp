#include "BuildingSourceCoverage.h"
#include "GeometryPageResidency.h"
#include "BuildingField.h"
#include "StructureCell.h"
#include "TileGeodesy.h"
#include <cstdint>
#include <map>

namespace outshine {
bool BuildingSourceCoverage::Update(const Render::GeometryPageResidency &pieces,
                                    const Generators::Osm::BuildingField &footprints) {
  const Revision revision{.Geometry = pieces.Digest(), .Footprints = footprints.Revision()};
  if (Revision_ == revision) { return false; }
  std::map<uint32_t, uint64_t> masks;
  pieces.ForEachDigest([&](const Render::GeometryPageResidency::DigestRecord &piece) {
    const auto *input = footprints.InputOfTile(piece.Tile);
    if (!input || !input->Qualified || !input->Vector || input->SourceKey != piece.SourceKey ||
        piece.FallbackHeights) {
      return;
    }
    if (piece.Cell == 0) {
      masks[piece.Tile] = input->OccupiedCells;
    } else if (piece.Cell <= Ground::kStructureCellsPerTile) {
      masks[piece.Tile] |= uint64_t{1} << (piece.Cell - 1);
    }
  });
  Regions_.clear();
  Regions_.reserve(masks.size());
  for (const auto &[tile, mask] : masks) {
    const auto &input = *footprints.InputOfTile(tile);
    if (input.Vector && mask == input.OccupiedCells) {
      Regions_.push_back(Ground::TileBounds(input.Vector->Tile));
    }
  }
  Revision_ = revision;
  return true;
}
}
