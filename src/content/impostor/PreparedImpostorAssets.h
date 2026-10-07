#ifndef OUTSHINE_CONTENT_IMPOSTOR_PREPAREDIMPOSTORASSETS_H
#define OUTSHINE_CONTENT_IMPOSTOR_PREPAREDIMPOSTORASSETS_H

#include "content/AssetCache.h"
#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "Tasks.h"
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

namespace outshine::Content {
class PreparedImpostorAssets {
public:
  struct Config {
    std::string Directory;
    size_t Pending = 2;
    static constexpr size_t kDefaultReadBytes = size_t{16} * 1024 * 1024;
    size_t ReadBytes = kDefaultReadBytes;
  };

  struct Loaded {
    std::string Provenance;
    std::optional<Content::ImpostorAtlas> Atlas;
    std::optional<Content::ImpostorCards> Cards;
    std::string Error;
  };
  enum class Request { Queued, Existing, Full };

  struct Counters {
    uint64_t Hits = 0, Misses = 0, Writes = 0, ReadBytes = 0;
    uint64_t Preparations = 0;
    double ReadMs = 0.0, WriteMs = 0.0, PreparationMs = 0.0;
  };

  using Prepare = std::optional<ImpostorCards> (*)(const ImpostorAtlas &, std::string &);
  PreparedImpostorAssets(Tasks &tasks, const Config &config, Prepare prepare = nullptr);
  ~PreparedImpostorAssets();
  [[nodiscard]] bool
  Publish(const Content::ImpostorAtlas &atlas, std::string_view provenance, std::string &error);
  [[nodiscard]] Request Read(std::string provenance);
  [[nodiscard]] std::optional<Loaded> Take();

  [[nodiscard]] bool Enabled() const noexcept { return !Directory_.empty(); }

  [[nodiscard]] bool AwaitProgress(double seconds);
  [[nodiscard]] Counters Costs() const noexcept;

private:
  struct Pending {
    std::shared_ptr<Loaded> Result;
    Tasks::Handle Job = Tasks::kNoTask;
  };

  Tasks *Tasks_;
  std::string Directory_;
  std::unique_ptr<AssetCache> Cache_;
  std::mutex Lock_;
  size_t MostPending_, MostBytes_;
  Prepare Prepare_;
  std::deque<Pending> Pending_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic_uint64_t Preparations_{0};
  std::atomic<double> ReadMs_{0}, WriteMs_{0};
  std::atomic<double> PreparationMs_{0};
  [[nodiscard]] bool Opens(std::string &error);
  [[nodiscard]] std::optional<CachedAsset> LoadBytes(std::string_view key, std::string &error);
  [[nodiscard]] Loaded Load(std::string provenance);
  [[nodiscard]] std::optional<ImpostorCards> BuildCards(const ImpostorAtlas &atlas,
                                                        std::string &error);
  [[nodiscard]] bool
  PublishCards(const ImpostorCards &cards, std::string_view provenance, std::string &error);
};
}
#endif
