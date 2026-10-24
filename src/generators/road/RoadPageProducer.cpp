#include "RoadPageProducer.h"
#include "ProfiledRoadMesher.h"
#include <expected>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators {
RoadPageMeshProducer::RoadPageMeshProducer(std::string productKey, RoadPageRecipe recipe)
    : ProductKey_(std::move(productKey)), Recipe_(std::move(recipe)) {}

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
