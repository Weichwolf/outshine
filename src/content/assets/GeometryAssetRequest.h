#ifndef OUTSHINE_CONTENT_ASSETS_GEOMETRYASSETREQUEST_H
#define OUTSHINE_CONTENT_ASSETS_GEOMETRYASSETREQUEST_H
#include "AssetPipeline.h"
#include "GeometryAsset.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine {
class GeometryAssetRequest {
  struct State;

public:
  struct Budget {
    size_t ProducerBytes = 0;
    size_t ConsumerBytes = 0;
    size_t EncodedBytesMost = 0;
    size_t ReceiptBytesMost = 0;
  };

  // Miss-only worker callback; publish the requested geometry and metadata before returning.
  using Publisher = std::function<std::expected<void, std::string>(const std::stop_token &)>;

  struct Prepared {
    std::shared_ptr<const void> Owner;
    size_t Bytes = 0;
  };

  // May consume the mesh; product leases retain the returned owner and its declared bytes.
  using Preparation = std::function<std::expected<Prepared, std::string>(GeometryAsset::Product &)>;

  // ConsumerBytes reserves caller-bounded decode/preparation work, not just encoded bytes.
  [[nodiscard]] static std::expected<GeometryAssetRequest, AssetPublicationError>
  Start(AssetPipeline &pipeline,
        Tasks &workers,
        std::shared_ptr<AssetCache> cache,
        std::string key,
        std::string parent,
        Budget budget,
        Publisher publish = {},
        Preparation prepare = {});
  GeometryAssetRequest(GeometryAssetRequest &&other) noexcept;
  GeometryAssetRequest &operator=(GeometryAssetRequest &&other) noexcept;
  ~GeometryAssetRequest();
  GeometryAssetRequest(const GeometryAssetRequest &) = delete;
  GeometryAssetRequest &operator=(const GeometryAssetRequest &) = delete;
  [[nodiscard]] std::expected<std::shared_ptr<const GeometryAsset::Product>, std::string> Poll();
  void Cancel() noexcept;

private:
  GeometryAssetRequest(Tasks &workers, std::shared_ptr<State> state) noexcept;
  Tasks *Workers_;
  std::shared_ptr<State> State_;
};
}
#endif
