#ifndef OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADER_H
#define OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADER_H

#include "OsmSourceSnapshot.h"
#include "OsmChunkSetLoader.h"
#include <world/data/Transport.h>
#include "Tasks.h"
#include <world/SourceProvider.h>
#include <world/Provider.h>

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
#include <variant>

namespace outshine {

class OsmSourceLoader {
public:
  enum class Phase : uint8_t { Inactive, Loading, Ready, Failed };

  explicit OsmSourceLoader(Tasks &tasks,
                           Data::Transport *wire = nullptr,
                           std::string cacheDirectory = {});

  ~OsmSourceLoader();
  OsmSourceLoader(const OsmSourceLoader &) = delete;
  OsmSourceLoader &operator=(const OsmSourceLoader &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Request(std::span<const Data::SourceProvider> providers,
          std::string_view root,
          const Data::ProviderRegistry *registry = nullptr);
  void Poll();
  [[nodiscard]] bool AwaitSlice(double seconds) const;

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] size_t PendingCount() const noexcept { return Pending_ ? 1 : 0; }

  [[nodiscard]] const std::shared_ptr<const Data::OsmSourceSnapshot> &Current() const noexcept {
    return Current_;
  }

private:
  using LoadResult = std::expected<std::shared_ptr<const Data::OsmSourceSnapshot>, std::string>;
  using ReadResult = std::expected<std::vector<Data::OsmSourceChunk>, std::string>;

  struct Result {
    std::variant<std::monostate, ReadResult, LoadResult> Value;
  };

  struct Pending {
    Tasks::Handle Handle = Tasks::kNoTask;
    Tasks *Owner = nullptr;
    uint64_t Revision = 0;
    std::shared_ptr<Result> Output;
    std::stop_source Stop;
  };

  void StartRequested();
  void StartDecode(std::vector<Data::OsmSourceChunk> input, std::stop_source stop);
  void CompletePending(Pending finished);
  struct Access;
  Tasks *Tasks_;
  Tasks Io_{1};
  std::shared_ptr<Access> Access_;
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
