#include "RoadPageMeshProducer.h"
#include "ProfiledRoadMesher.h"
#include "RoadPageRecipeCodec.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
RoadPageMeshProducer::RoadPageMeshProducer(std::string productKey, RoadPageRecipe recipe)
    : ProductKey_(std::move(productKey)),
      Recipe_(std::move(recipe)),
      InputBytes_(sizeof(*this) + ProductKey_.capacity() + RoadPageRecipeCapacityBytes(Recipe_) -
                  sizeof(Recipe_)) {}

std::expected<RoadPageGeometry, std::string>
RoadPageMeshProducer::Produce(const RoadPageRequest &request, std::stop_token stop) const {
  if (ProductKey_.empty() || RoadPageKey(request.Address) != ProductKey_) {
    return std::unexpected("road page producer does not own the requested product");
  }
  std::optional<RoadDisplayRequest> display;
  const auto &error = request.Address.PositionErrorM;
  if (error) {
    const auto &limits = request.SweepLimits;
    if (!limits) {
      return std::unexpected("road display generation needs an explicit sampling budget");
    }
    for (const auto &interval : Recipe_.Intervals) {
      if (request.Address.DisplayPolicy == RoadDisplayPolicy::UpperOnly &&
          (interval.Body || interval.Surface.Form != RibbonForm::Surface)) {
        return std::unexpected("road display generation cannot replace closed or step bodies");
      }
    }
    display = RoadDisplayRequest{
        .ErrorM = *error, .Limits = *limits, .Policy = request.Address.DisplayPolicy};
  }
  const ProfiledRoadMesher mesher = display ? ProfiledRoadMesher(*display) : ProfiledRoadMesher{};
  auto result = MeshRoadPage(mesher, Recipe_, stop);
  if (!result) { return std::unexpected(std::string(result.error())); }
  if (display && (result->Swept.Refused != 0 || !result->Diagnostics.empty())) {
    return std::unexpected("road display generation cannot publish incomplete source coverage");
  }
  return std::move(*result);
}
}
