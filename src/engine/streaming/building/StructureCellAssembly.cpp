#include "StructureCellAssembly.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace outshine {
void StructureCellAssembly::Collect(const Generators::BakedTile &child) {
  assert(child.RequestedCell && *child.RequestedCell > 0 &&
         *child.RequestedCell <= Generators::kStructureCellsPerTile && child.Coordinates);
  const uint64_t bit = uint64_t{1} << (*child.RequestedCell - 1u);
  assert((Cells_ & bit) != 0 && child.OccupiedCells == bit);
  if ((Collected_ & bit) != 0) { return; }
  const size_t at = *child.RequestedCell - 1u;
  if (!Metadata_.Coordinates) {
    Metadata_.Coordinates = child.Coordinates;
  } else {
    Metadata_.Coordinates->Sources.insert_range(Metadata_.Coordinates->Sources.end(),
                                                child.Coordinates->Sources);
  }
  Metadata_.Prints.insert_range(Metadata_.Prints.end(), child.Prints);
  Metadata_.SeatSpreadM.insert_range(Metadata_.SeatSpreadM.end(), child.SeatSpreadM);
  Metadata_.AcrossM.insert_range(Metadata_.AcrossM.end(), child.AcrossM);
  Metadata_.OccupiedCells |= bit;
  Metadata_.CellBounds[at] = child.CellBounds[at];
  Metadata_.CellMaxHeightM[at] = child.CellMaxHeightM[at];
  Metadata_.CellShellErrorM[at] = child.CellShellErrorM[at];
  Metadata_.OsmHeights += child.OsmHeights;
  Metadata_.DefaultHeights += child.DefaultHeights;
  Metadata_.Fronted += child.Fronted;
  Metadata_.UnsupportedMeshes += child.UnsupportedMeshes;
  Metadata_.SkippedRings = std::max(Metadata_.SkippedRings, child.SkippedRings);
  Metadata_.NoGround = std::max(Metadata_.NoGround, child.NoGround);
  Metadata_.FallbackHeights = Metadata_.FallbackHeights || child.FallbackHeights;
  Triangles_ += (child.Built.WallRun.size() + child.Built.RoofRun.size()) / 3u;
  GeometryBytes_ += child.Built.UsedBytes();
  Collected_ |= bit;
}
}
