#include "RoadPageMeshing.h"
#include "RoadBodyMesh.h"
#include <expected>
#include <string_view>

namespace outshine::Generators {
std::expected<void, std::string_view> AppendRoadPageInterval(const RoadMesher &mesher,
                                                             const RoadPageIntervalView &interval,
                                                             RoadPageGeometry &product) {
  product.Parts.push_back({.Kind = RoadPagePartKind::Interval, .Index = interval.Edge});
  const auto before = product.Mesh.Index.size();
  RoadMeshingStats stats;
  if (interval.Body != nullptr) {
    if (!AppendRoadBodyMesh(*interval.Body, interval.Surface.WearsLinear, product.Mesh)) {
      return std::unexpected("road step body exceeds its mesh index range");
    }
    stats.Pieces = 1;
  } else {
    stats = mesher.Sweep(interval.Stations, interval.Surface, product.Mesh);
  }
  product.Swept += stats;
  if (stats.Refused != 0 || product.Mesh.Index.size() == before) {
    product.Diagnostics.push_back(
        {.Part = {.Kind = RoadPagePartKind::Interval, .Index = interval.Edge},
         .Source = interval.Source,
         .Stats = stats,
         .Empty = product.Mesh.Index.size() == before});
  }
  return {};
}

void AppendRoadPageJunction(const RoadMesher &mesher,
                            const RoadPageJunctionView &junction,
                            RoadPageGeometry &product) {
  product.Parts.push_back({.Kind = RoadPagePartKind::Junction, .Index = junction.Index});
  const auto before = product.Mesh.Index.size();
  mesher.Junction(junction.Gates, junction.Plane, junction.WearsLinear, product.Mesh);
  if (product.Mesh.Index.size() == before) {
    product.Diagnostics.push_back(
        {.Part = {.Kind = RoadPagePartKind::Junction, .Index = junction.Index},
         .Source = junction.Node,
         .Stats = {},
         .Empty = true});
  }
}
}
