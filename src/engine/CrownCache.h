#ifndef OUTSHINE_ENGINE_CROWNCACHE_H
#define OUTSHINE_ENGINE_CROWNCACHE_H

#include "ContentStore.h"
#include "CrownAtlas.h"
#include "Tasks.h"
#include <deque>
#include <memory>

namespace outshine {
class CrownCache {
public:
  struct Config {
    Data::ContentStore::Config Store;
    size_t Pending = 2;
    size_t ReadBytes = 16u << 20u;
  };

  struct Loaded {
    std::string Provenance;
    std::optional<CrownAtlas> Atlas;
    std::string Error;
  };
  enum class Request { Queued, Existing, Full };

  CrownCache(Tasks &tasks, const Config &config);
  ~CrownCache();
  [[nodiscard]] bool
  Publish(const CrownAtlas &atlas, std::string_view provenance, std::string &error);
  [[nodiscard]] Request Read(std::string provenance);
  [[nodiscard]] std::optional<Loaded> Take();

private:
  struct Pending {
    std::shared_ptr<Loaded> Result;
    Tasks::Handle Job = Tasks::kNoTask;
  };

  Tasks *Tasks_;
  Data::ContentStore Store_;
  size_t MostPending_, MostBytes_;
  std::deque<Pending> Pending_;
};
} // namespace outshine
#endif
