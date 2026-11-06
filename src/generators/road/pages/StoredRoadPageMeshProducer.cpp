#include "StoredRoadPageMeshProducer.h"
#include "RoadPageMeshProducer.h"
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine::Generators {
StoredRoadPageMeshProducer::StoredRoadPageMeshProducer(std::shared_ptr<PreparedRoadPages> pages,
                                                       RoadPageAddress address,
                                                       AssetCache::ReadPlan read,
                                                       size_t decodedBytes)
    : Pages_(std::move(pages)),
      Address_(std::move(address)),
      Key_(RoadPageKey(Address_)),
      Read_(read),
      DecodedBytes_(decodedBytes),
      InputBytes_(sizeof(*this) + Address_.PlanKey.capacity() + Address_.MaterialKey.capacity() +
                  Address_.Parts.capacity() * sizeof(RoadPagePart) + Key_.capacity() +
                  read.BufferBytesMost + decodedBytes) {
  if (const auto *appearance = std::get_if<RoadPageAppearanceV2>(&Address_.Appearance)) {
    InputBytes_ += appearance->Key.capacity();
  }
}

std::expected<StoredRoadPageMeshProducer, std::string> StoredRoadPageMeshProducer::Bind(
    std::shared_ptr<PreparedRoadPages> pages, RoadPageAddress address, AssetCache::ReadPlan read) {
  const auto decoded = RoadPageRecipeDecodeCapacity(read.PackageBytesMost);
  if (!pages || RoadPageKey(address).empty() || !decoded || read.PackageBytesMost == 0 ||
      read.BufferBytesMost < read.PackageBytesMost ||
      read.BufferBytesMost > (std::numeric_limits<size_t>::max() - *decoded) / 2) {
    return std::unexpected(
        "stored road page needs a complete address and reserved input capacities");
  }
  return StoredRoadPageMeshProducer(std::move(pages), std::move(address), read, *decoded);
}

std::expected<RoadPageGeometry, std::string>
StoredRoadPageMeshProducer::Produce(const RoadPageRequest &request, std::stop_token stop) const {
  if (RoadPageKey(request.Address) != Key_) {
    return std::unexpected("stored road page producer does not own the requested product");
  }
  if (stop.stop_requested()) { return std::unexpected("stored road page generation canceled"); }
  auto recipe = Pages_->LoadRecipe(Address_, Read_, DecodedBytes_);
  if (!recipe) { return std::unexpected(PreparedRoadPages::DescribeFailure(recipe.error())); }
  if (!*recipe) { return std::unexpected("stored road page recipe is absent"); }
  if (stop.stop_requested()) { return std::unexpected("stored road page generation canceled"); }
  return RoadPageMeshProducer(Key_, std::move(**recipe)).Produce(request, stop);
}
}
