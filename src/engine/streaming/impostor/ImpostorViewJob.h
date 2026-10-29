#ifndef OUTSHINE_ENGINE_STREAMING_IMPOSTORVIEWJOB_H
#define OUTSHINE_ENGINE_STREAMING_IMPOSTORVIEWJOB_H

#include "AssetCache.h"
#include "ImpostorViewAssets.h"
#include "Geometry.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
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

  using Resolver =
      std::function<std::expected<std::optional<AssetRecord>, std::string>(std::stop_token)>;
  // Each resolver owns its metadata and prepared product. Submit with Cache as
  // AssetStoreScope identity through the existing RequestResumable admission.
  [[nodiscard]] Resolver Prepare() const;
};
}
#endif
