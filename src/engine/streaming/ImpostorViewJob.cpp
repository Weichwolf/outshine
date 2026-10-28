#include "ImpostorViewJob.h"
#include "Geometry.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
std::expected<AssetRecord, std::string> ImpostorViewJob::operator()(std::stop_token stop) const {
  if (stop.stop_requested()) { return std::unexpected("impostor view request cancelled"); }
  if (!Cache || Provenance.empty() || ProductKey.empty() || BytesMost == 0) {
    return std::unexpected("invalid impostor view job");
  }
  const auto opened = Content::ImpostorViewAssets::Open(*Cache, Provenance);
  if (!opened || !*opened) { return std::unexpected("impostor view metadata unavailable"); }
  const auto &views = **opened;
  if (views.Key(Selected) != ProductKey) {
    return std::unexpected("impostor view metadata changed before production");
  }
  std::string failure;
  const auto cards = views.Resolve(
      Selected,
      BytesMost,
      [&](Selection selection, size_t budget) -> std::expected<Geometry, AssetCacheError> {
        if (stop.stop_requested() || !Generate) {
          failure = stop.stop_requested() ? "impostor view request cancelled"
                                          : "impostor view miss has no producer";
          return std::unexpected(AssetCacheError::InvalidInput);
        }
        auto geometry = Generate(selection, budget, stop);
        if (!geometry) {
          failure = std::move(geometry.error());
          return std::unexpected(AssetCacheError::InvalidInput);
        }
        if (stop.stop_requested()) {
          failure = "impostor view request cancelled";
          return std::unexpected(AssetCacheError::InvalidInput);
        }
        return std::move(*geometry);
      });
  if (!cards) {
    return std::unexpected(failure.empty() ? "impostor view acquisition failed" : failure);
  }
  if (stop.stop_requested()) { return std::unexpected("impostor view request cancelled"); }
  auto record = Cache->Find(ProductKey);
  if (!record || !*record) { return std::unexpected("impostor view receipt unavailable"); }
  return std::move(**record);
}
}
