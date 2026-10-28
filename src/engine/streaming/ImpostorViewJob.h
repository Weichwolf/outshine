#ifndef OUTSHINE_ENGINE_STREAMING_IMPOSTORVIEWJOB_H
#define OUTSHINE_ENGINE_STREAMING_IMPOSTORVIEWJOB_H

#include "AssetCache.h"
#include "ImpostorViewAssets.h"
#include "Geometry.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine {
struct ImpostorViewJob {
  using Selection = Content::ImpostorViewAssets::Selection;
  // Factories return CPU products; thread-affine capture resources die inside the call.
  using Factory =
      std::function<std::expected<Geometry, std::string>(Selection, size_t, std::stop_token)>;

  std::shared_ptr<AssetCache> Cache;
  std::string Provenance, ProductKey;
  Selection Selected;
  size_t BytesMost = 0;
  Factory Generate;

  // Submit with Cache as the consumer's AssetStoreScope identity.
  [[nodiscard]] std::expected<AssetRecord, std::string> operator()(std::stop_token stop) const;
};
}
#endif
