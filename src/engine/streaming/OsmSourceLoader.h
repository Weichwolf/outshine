#ifndef OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADER_H
#define OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADER_H

#include "OsmSourceSnapshot.h"
#include "Tasks.h"
#include <world/SourceProvider.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

class OsmSourceLoader {
public:
  enum class Phase : uint8_t { Inactive, Loading, Ready, Failed };

  explicit OsmSourceLoader(Tasks &tasks) : Tasks_(&tasks) {}

  ~OsmSourceLoader();
  OsmSourceLoader(const OsmSourceLoader &) = delete;
  OsmSourceLoader &operator=(const OsmSourceLoader &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Request(std::span<const Data::SourceProvider> providers, std::string_view root);
  void Poll();

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] size_t PendingCount() const noexcept { return Pending_ ? 1 : 0; }

  [[nodiscard]] const std::shared_ptr<const Data::OsmSourceSnapshot> &Current() const noexcept {
    return Current_;
  }

private:
  using LoadResult = std::expected<std::shared_ptr<const Data::OsmSourceSnapshot>, std::string>;

  struct Result {
    std::optional<LoadResult> Value;
  };

  struct Pending {
    Tasks::Handle Handle = Tasks::kNoTask;
    uint64_t Revision = 0;
    std::shared_ptr<Result> Output;
    std::stop_source Stop;
  };

  void StartRequested();
  Tasks *Tasks_;
  std::vector<Data::SourceProvider> Requested_;
  std::string Root_;
  std::optional<Pending> Pending_;
  std::shared_ptr<const Data::OsmSourceSnapshot> Current_;
  std::string Error_;
  uint64_t Revision_ = 0;
  Phase Phase_ = Phase::Inactive;
};

}
#endif
