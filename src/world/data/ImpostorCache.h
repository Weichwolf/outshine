#ifndef OUTSHINE_WORLD_DATA_IMPOSTORCACHE_H
#define OUTSHINE_WORLD_DATA_IMPOSTORCACHE_H

#include "ContentStore.h"
#include "ImpostorAtlas.h"
#include "Tasks.h"
#include <deque>
#include <memory>

namespace outshine::Data {
class ImpostorCache {
public:
  struct Config {
    ContentStore::Config Store;
    size_t Pending = 2;
    static constexpr size_t kDefaultReadBytes = size_t{16} * 1024 * 1024;
    size_t ReadBytes = kDefaultReadBytes;
  };

  struct Loaded {
    std::string Provenance;
    std::optional<Content::ImpostorAtlas> Atlas;
    std::string Error;
  };
  enum class Request { Queued, Existing, Full };

  ImpostorCache(Tasks &tasks, const Config &config);
  ~ImpostorCache();
  [[nodiscard]] bool
  Publish(const Content::ImpostorAtlas &atlas, std::string_view provenance, std::string &error);
  [[nodiscard]] Request Read(std::string provenance);
  [[nodiscard]] std::optional<Loaded> Take();

private:
  struct Pending {
    std::shared_ptr<Loaded> Result;
    Tasks::Handle Job = Tasks::kNoTask;
  };

  Tasks *Tasks_;
  ContentStore Store_;
  size_t MostPending_, MostBytes_;
  std::deque<Pending> Pending_;
};
}
#endif
