#ifndef OUTSHINE_ENGINE_STREAMING_IMPOSTORFIELDLOAD_H
#define OUTSHINE_ENGINE_STREAMING_IMPOSTORFIELDLOAD_H

#include "AssetPipeline.h"
#include "ImpostorAtlas.h"
#include "ImpostorViewAssets.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <string>

namespace outshine {
class ImpostorFieldLoad {
  struct State;

public:
  [[nodiscard]] static std::expected<ImpostorFieldLoad, std::string>
  Start(Tasks &workers,
        std::shared_ptr<AssetCache> cache,
        std::string provenance,
        Content::ImpostorViewAssets::ViewIndex view,
        size_t bytesMost,
        AssetPipeline::RequestTicket ticket);
  ImpostorFieldLoad(ImpostorFieldLoad &&other) noexcept;
  ImpostorFieldLoad &operator=(ImpostorFieldLoad &&other) noexcept;
  ~ImpostorFieldLoad();
  ImpostorFieldLoad(const ImpostorFieldLoad &) = delete;
  ImpostorFieldLoad &operator=(const ImpostorFieldLoad &) = delete;

  // Null means pending; the returned CPU field retains the publication ticket.
  [[nodiscard]] std::expected<std::shared_ptr<const ImpostorAtlas>, std::string> Poll() const;
  void Cancel() noexcept;

private:
  explicit ImpostorFieldLoad(std::shared_ptr<State> state) noexcept;
  std::shared_ptr<State> State_;
};
}
#endif
