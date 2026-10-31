#ifndef OUTSHINE_ENGINE_STREAMING_IMPOSTORVIEWJOB_H
#define OUTSHINE_ENGINE_STREAMING_IMPOSTORVIEWJOB_H

#include "AssetProduct.h"
#include "ImpostorViewAssets.h"
#include "Geometry.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <variant>

namespace outshine {
struct ImpostorViewJob {
  using Selection = Content::ImpostorViewAssets::Selection;
  // Factories return CPU products; thread-affine capture resources die inside the call.
  using Factory =
      std::function<std::expected<Geometry, std::string>(Selection, size_t, std::stop_token)>;
  using ViewIndex = Content::ImpostorViewAssets::ViewIndex;
  using FieldFactory =
      std::function<std::expected<ImpostorAtlas, std::string>(ViewIndex, size_t, std::stop_token)>;

  struct SurfaceView {
    using Value = Geometry;
    Selection Selected;
    Factory Generate;
  };

  struct MaterialView {
    using Value = ImpostorAtlas;
    ViewIndex Selected;
    FieldFactory Generate;
  };

  std::shared_ptr<AssetCache> Cache;
  std::string Provenance, ProductKey;
  size_t BytesMost = 0;
  std::variant<SurfaceView, MaterialView> Product;

  // Submit with Cache as the consumer's AssetStoreScope identity.
  [[nodiscard]] std::expected<AssetProduct, std::string> operator()(std::stop_token stop) const;
};
}
#endif
