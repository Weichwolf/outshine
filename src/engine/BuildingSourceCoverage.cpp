#include "BuildingSourceCoverage.h"
#include "GeometryPageResidency.h"
#include "BuildingField.h"
#include "StructureCellGrid.h"
#include "TileGeodesy.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

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
    if (piece.Cell > Ground::kStructureCellsPerTile) { return; }
    const uint64_t covered = piece.OccupiedCells == 0 && piece.Cell != 0
                                 ? uint64_t{1} << (piece.Cell - 1)
                                 : piece.OccupiedCells;
    masks[piece.Tile] |= covered;
  });
  const auto inputs = footprints.AcceptedInputs();
  const auto tiles = footprints.AcceptedTiles();
  std::vector<Ground::GeoBounds> regions;
  regions.reserve(masks.size());
  for (size_t index = 0; index < inputs.size(); ++index) {
    const auto &input = inputs[index];
    if (!input.Qualified || !input.Vector) { continue; }
    const auto resident = masks.find(tiles[index]);
    const uint64_t covered = resident != masks.end() ? resident->second : 0;
    if (covered == input.OccupiedCells) {
      regions.push_back(Ground::TileBounds(input.Vector->Tile));
    }
  }
  Revision_ = revision;
  if (Regions_ != regions) {
    Regions_ = std::move(regions);
    ++Version_;
  }
  return true;
}
}
