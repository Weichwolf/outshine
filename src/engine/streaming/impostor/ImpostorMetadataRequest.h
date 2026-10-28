#ifndef OUTSHINE_ENGINE_STREAMING_IMPOSTOR_IMPOSTORMETADATAREQUEST_H
#define OUTSHINE_ENGINE_STREAMING_IMPOSTOR_IMPOSTORMETADATAREQUEST_H
#include "AssetPipeline.h"
#include "ImpostorViewAssets.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine {
class ImpostorMetadataRequest {
  struct State;

public:
  using Resolver = std::function<std::expected<Content::ImpostorViewAssets, std::string>(
      const std::stop_token &)>;
  // The consumer uses cache as its store-scope identity; workers outlive the request.
  [[nodiscard]] static std::expected<ImpostorMetadataRequest, AssetPublicationError>
  Start(AssetPipeline &pipeline,
        const AssetPipeline::Consumer &consumer,
        Tasks &workers,
        std::shared_ptr<AssetCache> cache,
        std::string provenance,
        Resolver resolve,
        size_t inputBytes);
  ImpostorMetadataRequest(ImpostorMetadataRequest &&other) noexcept;
  ImpostorMetadataRequest &operator=(ImpostorMetadataRequest &&other) noexcept;
  ~ImpostorMetadataRequest();
  ImpostorMetadataRequest(const ImpostorMetadataRequest &) = delete;
  ImpostorMetadataRequest &operator=(const ImpostorMetadataRequest &) = delete;
  [[nodiscard]] std::expected<std::shared_ptr<const Content::ImpostorViewAssets>, std::string>
  Poll();
  void Cancel() noexcept;

private:
  ImpostorMetadataRequest(Tasks &workers, std::shared_ptr<State> state) noexcept;
  Tasks *Workers_;
  std::shared_ptr<State> State_;
};
}
#endif
