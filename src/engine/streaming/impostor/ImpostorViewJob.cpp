#include "ImpostorViewJob.h"
#include <concepts>
#include <cstddef>
#include <expected>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
namespace {
using Views = Content::ImpostorViewAssets;

template <class Product> std::string Key(const Views &views, const Product &product) {
  if constexpr (std::same_as<Product, ImpostorViewJob::SurfaceView>) {
    return views.Key(product.Selected);
  } else {
    return views.FieldKey(product.Selected);
  }
}

template <class Product>
std::expected<void, std::string>
Produce(const Views &views, const Product &product, size_t bytesMost, std::stop_token stop) {
  std::string failure;
  const auto factory =
      [&](auto selected, size_t budget) -> std::expected<typename Product::Value, AssetCacheError> {
    if (stop.stop_requested() || !product.Generate) {
      failure = stop.stop_requested() ? "impostor view request cancelled"
                                      : "impostor view miss has no producer";
      return std::unexpected(AssetCacheError::InvalidInput);
    }
    auto generated = product.Generate(selected, budget, stop);
    if (!generated) {
      failure = std::move(generated.error());
      return std::unexpected(AssetCacheError::InvalidInput);
    }
    if (stop.stop_requested()) {
      failure = "impostor view request cancelled";
      return std::unexpected(AssetCacheError::InvalidInput);
    }
    return std::move(*generated);
  };
  const auto acquired = [&] {
    if constexpr (std::same_as<Product, ImpostorViewJob::SurfaceView>) {
      return views.Resolve(product.Selected, bytesMost, factory);
    } else {
      return views.ResolveField(product.Selected, bytesMost, factory);
    }
  }();
  if (!acquired) {
    return std::unexpected(failure.empty() ? "impostor view acquisition failed" : failure);
  }
  return {};
}
}

std::expected<AssetRecord, std::string> ImpostorViewJob::operator()(std::stop_token stop) const {
  if (stop.stop_requested()) { return std::unexpected("impostor view request cancelled"); }
  if (!Cache || Provenance.empty() || ProductKey.empty() || BytesMost == 0) {
    return std::unexpected("invalid impostor view job");
  }
  const auto opened = Views::Open(*Cache, Provenance);
  if (!opened || !*opened) { return std::unexpected("impostor view metadata unavailable"); }
  const auto &views = **opened;
  const auto acquired = std::visit(
      [&](const auto &product) -> std::expected<void, std::string> {
        if (Key(views, product) != ProductKey) {
          return std::unexpected("impostor view metadata changed before production");
        }
        return Produce(views, product, BytesMost, stop);
      },
      Product);
  if (!acquired) { return std::unexpected(acquired.error()); }
  if (stop.stop_requested()) { return std::unexpected("impostor view request cancelled"); }
  auto record = Cache->Find(ProductKey);
  if (!record || !*record) { return std::unexpected("impostor view receipt unavailable"); }
  return std::move(**record);
}
}
