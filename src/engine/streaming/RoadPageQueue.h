#ifndef OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUE_H
#define OUTSHINE_ENGINE_STREAMING_ROADPAGEQUEUE_H

#include "PreparedRoadPages.h"
#include "Tasks.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace outshine {
class RoadPageQueue {
public:
  using Factory = std::function<std::expected<Generators::RoadPageGeometry, std::string>(
      size_t, const std::stop_token &)>;

  struct Completed {
    std::string Key;
    std::expected<Generators::PreparedRoadPages::Loaded, std::string> Result;
  };

  struct Limits {
    size_t PendingMost = 0;
    size_t PageBytesMost = 0;
  };

  RoadPageQueue(Tasks &pool, std::shared_ptr<Generators::PreparedRoadPages> pages, Limits limits);
  ~RoadPageQueue();
  RoadPageQueue(const RoadPageQueue &) = delete;
  RoadPageQueue &operator=(const RoadPageQueue &) = delete;

  [[nodiscard]] bool Request(Generators::RoadPageAddress address,
                             std::string parent,
                             std::string requestKey,
                             Factory factory);
  void Cancel(const std::string &key);
  [[nodiscard]] std::vector<Completed> Collect();

  [[nodiscard]] size_t Pending() const noexcept { return Work_.size(); }

private:
  struct Work;
  Tasks &Pool_;
  std::shared_ptr<Generators::PreparedRoadPages> Pages_;
  Limits Limits_;
  std::unordered_map<std::string, std::shared_ptr<Work>> Work_;
};
}
#endif
