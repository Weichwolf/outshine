#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_ROADPAGEGEOMETRY_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_ROADPAGEGEOMETRY_H

#include "RoadMesher.h"
#include <cstdint>
#include <vector>

namespace outshine::Generators {
enum class RoadPagePartKind : uint8_t { Interval, Junction };

struct RoadPagePart {
  RoadPagePartKind Kind = RoadPagePartKind::Interval;
  uint32_t Index = 0;
  [[nodiscard]] bool operator==(const RoadPagePart &) const = default;
};

struct RoadPageDiagnostic {
  RoadPagePart Part;
  uint64_t Source = 0;
  RoadMeshingStats Stats;
  bool Empty = false;
};

struct RoadPageGeometry {
  RoadMeshBuffers Mesh;
  RoadMeshingStats Swept;
  std::vector<RoadPageDiagnostic> Diagnostics;
  std::vector<RoadPagePart> Parts;
};
}
#endif
