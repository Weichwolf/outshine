#ifndef OUTSHINE_CONTENT_ASSETS_ASSETCACHERECLAIMER_H
#define OUTSHINE_CONTENT_ASSETS_ASSETCACHERECLAIMER_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace outshine {
class Tasks;
enum class AssetCacheError;

class AssetCacheReclaimer {
public:
  enum class Status : uint8_t { Idle, Running, Deferred, Failed, Stopped };

  struct Snapshot {
    Status Phase = Status::Idle;
    std::optional<AssetCacheError> Error;
    uint64_t ChargedBytes = 0;
    uint64_t LimitBytes = 0;
    uint64_t Steps = 0;
  };

  AssetCacheReclaimer(Tasks &workers, std::string directory);
  ~AssetCacheReclaimer();
  AssetCacheReclaimer(const AssetCacheReclaimer &) = delete;
  AssetCacheReclaimer &operator=(const AssetCacheReclaimer &) = delete;
  // Frame owner: coalesced dispatch/status only, never filesystem or SQLite work.
  void Advance();
  [[nodiscard]] Snapshot Inspect() const noexcept;

private:
  struct State;
  struct Execution;
  Tasks &Workers_;
  std::shared_ptr<State> State_;
  std::chrono::steady_clock::time_point RetryAt_;
};
}
#endif
