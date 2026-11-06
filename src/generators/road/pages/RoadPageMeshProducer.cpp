#include "RoadPageMeshProducer.h"
#include "ProfiledRoadMesher.h"
#include "RoadPageRecipeCodec.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <expected>
#include <optional>
#include <limits>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {
size_t EncodedInputMost(size_t native) {
  constexpr size_t framing = sizeof(uint64_t) + 4 * sizeof(uint32_t) + 64;
  if (native > (std::numeric_limits<size_t>::max() - framing) / 2) {
    return std::numeric_limits<size_t>::max();
  }
  return 2 * native + framing;
}

size_t InputWorkBytes(const RoadPageRecipe &recipe) {
  const size_t native = RoadPageRecipeCapacityBytes(recipe);
  const size_t encodedMost = EncodedInputMost(native);
  if (encodedMost > std::numeric_limits<size_t>::max() - native) {
    return std::numeric_limits<size_t>::max();
  }
  const auto publication = AssetCache::PublicationBufferBytes(encodedMost);
  if (!publication || *publication > std::numeric_limits<size_t>::max() - native - encodedMost) {
    return std::numeric_limits<size_t>::max();
  }
  return native + encodedMost + *publication;
}
}

RoadPageMeshProducer::RoadPageMeshProducer(std::string productKey, RoadPageRecipe recipe)
    : ProductKey_(std::move(productKey)),
      Recipe_(std::move(recipe)),
      InputBytes_(InputWorkBytes(Recipe_)) {
  const size_t own = sizeof(*this) - sizeof(Recipe_) + ProductKey_.capacity();
  InputBytes_ = InputBytes_ > std::numeric_limits<size_t>::max() - own
                    ? std::numeric_limits<size_t>::max()
                    : InputBytes_ + own;
}

std::expected<std::optional<AssetPackage>, std::string>
RoadPageMeshProducer::PrepareInput(const RoadPageRequest &request) const {
  if (RoadPageKey(request.Address) != ProductKey_) {
    return std::unexpected("road input producer does not own the requested product");
  }
  if (request.Address.PositionErrorM) { return std::nullopt; }
  const auto key = RoadPageRecipeKey(request.Address);
  auto bytes =
      EncodeRoadPageRecipe(key, Recipe_, EncodedInputMost(RoadPageRecipeCapacityBytes(Recipe_)));
  if (!bytes) { return std::unexpected("could not encode the complete road page recipe"); }
  AssetPackage package{.Records = {{.Key = key,
                                    .Kind = "road-page-recipe",
                                    .Bounds = {},
                                    .Package = {},
                                    .ByteCount = bytes->size(),
                                    .Level = 0,
                                    .Parent = request.Address.PlanKey}},
                       .Bytes = std::move(*bytes)};
  return package;
}

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
