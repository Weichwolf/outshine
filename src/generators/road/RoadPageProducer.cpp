#include "RoadPageProducer.h"
#include "ProfiledRoadMesher.h"
#include "SurfaceBody.h"
#include <algorithm>
#include <cstddef>
#include <vector>
#include <expected>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {
size_t BodyBytes(const SurfaceBody &body) {
  return sizeof(body) + body.ReferencePoints.capacity() * sizeof(body.ReferencePoints[0]) +
         body.ReferenceFaces.capacity() * sizeof(body.ReferenceFaces[0]) +
         body.FaceSources.capacity() * sizeof(body.FaceSources[0]) +
         body.FaceLevels.capacity() * sizeof(body.FaceLevels[0]) +
         body.Levels.capacity() * sizeof(body.Levels[0]) +
         body.Stairs.capacity() * sizeof(body.Stairs[0]) +
         body.Geometry.Points.capacity() * sizeof(body.Geometry.Points[0]) +
         body.Geometry.Faces.capacity() * sizeof(body.Geometry.Faces[0]) +
         body.Geometry.WallParents.capacity() * sizeof(body.Geometry.WallParents[0]) +
         body.Geometry.WallKinds.capacity() * sizeof(body.Geometry.WallKinds[0]);
}

size_t RecipeBytes(const RoadPageRecipe &recipe) {
  size_t bytes = recipe.Intervals.capacity() * sizeof(RoadPageInterval) +
                 recipe.Junctions.capacity() * sizeof(RoadPageJunction);
  std::vector<const SurfaceBody *> bodies;
  for (const auto &interval : recipe.Intervals) {
    bytes += interval.Stations.capacity() * sizeof(RoadStation);
    if (interval.StepBody && std::ranges::find(bodies, interval.StepBody.get()) == bodies.end()) {
      bytes += BodyBytes(*interval.StepBody);
      bodies.push_back(interval.StepBody.get());
    }
  }
  for (const auto &junction : recipe.Junctions) {
    bytes += junction.Gates.capacity() * sizeof(RoadGate);
  }
  return bytes;
}
}

RoadPageMeshProducer::RoadPageMeshProducer(std::string productKey, RoadPageRecipe recipe)
    : ProductKey_(std::move(productKey)),
      Recipe_(std::move(recipe)),
      InputBytes_(sizeof(*this) + ProductKey_.capacity() + RecipeBytes(Recipe_)) {}

std::expected<RoadPageGeometry, std::string>
RoadPageMeshProducer::Produce(const RoadPageRequest &request, std::stop_token stop) const {
  if (ProductKey_.empty() || RoadPageKey(request.Address) != ProductKey_) {
    return std::unexpected("road page producer does not own the requested product");
  }
  const ProfiledRoadMesher mesher;
  auto result = MeshRoadPage(mesher, Recipe_, stop);
  if (!result) { return std::unexpected(std::string(result.error())); }
  return std::move(*result);
}
}
