#ifndef OUTSHINE_GENERATORS_ROAD_ROADPAGERECIPE_H
#define OUTSHINE_GENERATORS_ROAD_ROADPAGERECIPE_H

#include "RoadMesher.h"
#include <cstdint>
#include <expected>
#include <stop_token>
#include <string_view>
#include <vector>

namespace outshine::Generators {

enum class RoadPagePartKind : uint8_t { Interval, Junction };

struct RoadPagePart {
  RoadPagePartKind Kind = RoadPagePartKind::Interval;
  uint32_t Index = 0;

  [[nodiscard]] bool operator==(const RoadPagePart &) const = default;
};

struct RoadPageInterval {
  uint32_t Edge = 0;
  uint32_t Lane = 0;
  std::vector<RoadStation> Stations;
  RoadSweep Surface;
};

struct RoadPageJunction {
  uint32_t Index = 0;
  uint64_t Node = 0;
  std::vector<RoadGate> Gates;
  RoadPlane Plane;
  Vec3f WearsLinear;
};

struct RoadPageRecipe {
  std::vector<RoadPageInterval> Intervals;
  std::vector<RoadPageJunction> Junctions;
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

[[nodiscard]] std::expected<RoadPageGeometry, std::string_view> MeshRoadPage(
    const RoadMesher &mesher, const RoadPageRecipe &recipe, const std::stop_token &stop = {});
}
#endif
