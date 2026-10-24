#ifndef OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUE_H
#define OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUE_H

#include "PreparedRoadPages.h"
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
  using Product = std::variant<Generators::RoadPageLease, AssetRecord>;
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

  RoadPageQueue(Tasks &pool, std::shared_ptr<Generators::PreparedRoadPages> pages, Limits limits);
  ~RoadPageQueue();
  RoadPageQueue(const RoadPageQueue &) = delete;
  RoadPageQueue &operator=(const RoadPageQueue &) = delete;

  [[nodiscard]] bool Request(Generators::RoadPageRequest request,
                             std::shared_ptr<const Generators::RoadPageProducer> producer = {});
  [[nodiscard]] bool RequestCached(const std::string &key, Manifest dependency = {});
  [[nodiscard]] bool RequestMetadata(Generators::RoadPageAddress address);
  void Cancel(const std::string &key);
  [[nodiscard]] std::vector<Completed> Collect();

  [[nodiscard]] size_t Pending() const noexcept { return Work_.size(); }

private:
  struct Work;
  enum class Admission : uint8_t { Rejected, Pending, Available };
  enum class Kind : uint8_t { Payload, Metadata };
  using Loader = std::function<Result(const std::stop_token &)>;
  [[nodiscard]] Admission RequestState(const std::string &key, Kind kind) const;
  [[nodiscard]] bool Enqueue(const std::string &key, Kind kind, Loader load);
  Tasks &Pool_;
  std::shared_ptr<Generators::PreparedRoadPages> Pages_;
  Limits Limits_;
  std::unordered_map<std::string, std::shared_ptr<Work>> Work_;
};
}
#endif
