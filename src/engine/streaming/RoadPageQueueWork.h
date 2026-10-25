#ifndef OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUEWORK_H
#define OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUEWORK_H
#include "RoadPageQueue.h"
#include <atomic>

namespace outshine {
struct RoadPageQueue::Work {
  Loader Load;
  std::string Parent;
  std::string RequestKey;
  Kind Type = Kind::Payload;
  std::stop_source Stop;
  RoadPageQueue::Result Result = std::unexpected("road page work has not completed");
  std::atomic<bool> Complete{false};
  std::optional<Generators::RoadPageRequest> Request;
  std::optional<AssetPipeline::RequestTicket> Ticket;
  Receipt Published;
  bool BindingQueued = false;
  std::optional<std::expected<AssetRecord, std::string>> Bound;
};
}
#endif
