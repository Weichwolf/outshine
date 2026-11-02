#ifndef OUTSHINE_ENGINE_STREAMING_LITTERPAGESTREAM_H
#define OUTSHINE_ENGINE_STREAMING_LITTERPAGESTREAM_H

#include "AssetPipeline.h"
#include "R8PageAtlas.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace outshine {
struct LitterPageStreamLimits {
  size_t DemandMost = 16384;
  size_t PendingMost = 64;
};

struct LitterPageStreamCounts {
  size_t Wanted = 0, Pending = 0, BookkeepingBytes = 0;
  uint64_t Started = 0, Published = 0, CacheHits = 0, Deferred = 0;
};

class LitterPageStream final {
public:
  // The pipeline, worker pool and atlas outlive the stream. Advance runs between frame submissions.
  [[nodiscard]] static Render::Native::Result<std::unique_ptr<LitterPageStream>>
  Create(AssetPipeline &pipeline,
         std::shared_ptr<AssetCache> cache,
         Render::R8PageAtlas &atlas,
         LitterPageStreamLimits limits = {});

  // Feedback includes used resident pages; only the latest bounded demand is retained.
  [[nodiscard]] Render::Native::Result<void>
  Observe(std::span<const Render::FieldPageAddress> pages);
  [[nodiscard]] Render::Native::Result<void> Advance();
  [[nodiscard]] LitterPageStreamCounts Counts() const noexcept;

private:
  struct Pending {
    Render::FieldPageAddress Address;
    uint64_t Seen;
    AssetPipeline::RequestTicket Ticket;
  };

  LitterPageStream(AssetPipeline &pipeline,
                   std::shared_ptr<AssetCache> cache,
                   Render::R8PageAtlas &atlas,
                   AssetPipeline::Consumer consumer,
                   LitterPageStreamLimits limits);
  [[nodiscard]] Render::Native::Result<void> Publish();
  [[nodiscard]] Render::Native::Result<void> PrepareEdits();
  [[nodiscard]] Render::Native::Result<void> Request();

  AssetPipeline *Pipeline_;
  std::shared_ptr<AssetCache> Cache_;
  Render::R8PageAtlas *Atlas_;
  AssetPipeline::Consumer Consumer_;
  LitterPageStreamLimits Limits_;
  uint64_t Epoch_ = 0;
  std::vector<Render::FieldPageAddress> Wanted_;
  std::vector<Pending> Pending_;
  std::vector<uint64_t> Seen_;
  std::vector<uint32_t> Victims_;
  std::vector<Render::R8PageEdit> Edits_;
  std::vector<size_t> Completed_;
  LitterPageStreamCounts Counts_;
};
}
#endif
