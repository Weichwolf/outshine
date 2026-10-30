#ifndef OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUE_H
#define OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUE_H
#include "PreparedRoadPages.h"
#include "AssetPipeline.h"
#include "Tasks.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace outshine {
class RoadPageQueue {
public:
  using Receipt = std::shared_ptr<const AssetRecord>;
  using Product = std::variant<Generators::RoadPageLease, Receipt>;
  using Result = std::expected<std::optional<Product>, std::string>;

  struct Completed {
    std::string Key;
    RoadPageQueue::Result Result;
  };

  struct Manifest {
    std::string Key;
  };

  struct Limits {
    size_t PendingMost = 0;
    size_t PageBytesMost = 0;
  };

  // The pool outlives this queue and all unfinished callbacks.
  RoadPageQueue(Tasks &pool,
                std::shared_ptr<Generators::PreparedRoadPages> pages,
                Limits limits,
                std::shared_ptr<AssetPipeline> publications = {});
  ~RoadPageQueue();
  RoadPageQueue(const RoadPageQueue &) = delete;
  RoadPageQueue &operator=(const RoadPageQueue &) = delete;
  [[nodiscard]] std::expected<bool, std::string>
  RequestRecord(Generators::RoadPageRequest request,
                std::shared_ptr<const Generators::RoadPageProducer> producer = {});
  [[nodiscard]] bool RequestCached(const std::string &key, Manifest dependency = {});
  [[nodiscard]] bool RequestMetadata(Generators::RoadPageAddress address);
  [[nodiscard]] bool SetGeneration(AssetConsumerGeneration generation) noexcept;
  void Cancel(const std::string &key);
  [[nodiscard]] std::vector<Completed> Collect();

  [[nodiscard]] size_t Pending() const noexcept { return Work_.size(); }

private:
  struct Work;
  enum class Admission : uint8_t { Rejected, Pending, Available };
  enum class Kind : uint8_t { Payload, Metadata, Publication };
  using LoadResult = std::expected<std::optional<Product>, Generators::PreparedRoadPages::Failure>;
  using Loader = std::function<LoadResult(const std::stop_token &)>;
  [[nodiscard]] Admission RequestState(const std::string &key, Kind kind) const;
  [[nodiscard]] bool Enqueue(const std::string &key, Kind kind, Loader load);
  [[nodiscard]] bool PostLoad(const std::shared_ptr<Work> &work);
  void PollLoad(const std::shared_ptr<Work> &work);
  static void RunLoad(const std::shared_ptr<Work> &work);
  void PollPublication(const std::shared_ptr<Work> &work);
  void PostBinding(const std::shared_ptr<Work> &work);
  void CancelBinding(const std::shared_ptr<Work> &work);
  static void RunBinding(const std::shared_ptr<Work> &work,
                         const std::shared_ptr<Generators::PreparedRoadPages> &pages);
  static void CompleteBinding(const std::shared_ptr<Work> &work);
  Tasks &Pool_;
  std::shared_ptr<Generators::PreparedRoadPages> Pages_;
  Limits Limits_;
  std::shared_ptr<AssetPipeline> Publications_;
  std::optional<AssetPipeline::Consumer> Consumer_;
  std::unordered_map<std::string, std::shared_ptr<Work>> Work_;
};
}
#endif
