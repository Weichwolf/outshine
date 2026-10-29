#ifndef OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUEWORK_H
#define OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUEWORK_H
#include "RoadPageQueue.h"
#include <atomic>
#include <chrono>

namespace outshine {
struct RoadPageQueue::Work {
  enum class LoadStatus { Running, Deferred, Complete };
  std::atomic<LoadStatus> LoadState{LoadStatus::Running};
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
  enum class BindingStatus { Running, Deferred, CancelRequested, Complete };
  std::atomic<BindingStatus> BindingState{BindingStatus::Running};
  std::chrono::steady_clock::time_point RetryAt;
  std::chrono::milliseconds Delay{1};
  std::optional<Generators::PreparedRoadPages::PageWrite> Binding;
  std::optional<std::expected<AssetRecord, std::string>> Bound;
  void FinishBinding();
};
}
#endif
