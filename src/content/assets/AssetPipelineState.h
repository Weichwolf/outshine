#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPIPELINESTATE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPIPELINESTATE_H
#include "AssetPipeline.h"
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace outshine {
struct AssetPipeline::State : std::enable_shared_from_this<State> {
  struct Reservation {
    std::shared_ptr<State> Owner;
    size_t Bytes = 0;
    ~Reservation();
  };

  struct ScopedKey {
    AssetStoreScope Scope;
    AssetProductKey Product;
  };

  struct KeyLess {
    [[nodiscard]] bool operator()(const ScopedKey &a, const ScopedKey &b) const noexcept {
      if (a.Scope.Identity.owner_before(b.Scope.Identity)) { return true; }
      if (b.Scope.Identity.owner_before(a.Scope.Identity)) { return false; }
      return a.Product < b.Product;
    }
  };

  struct Job {
    AssetProductKey Key;
    AssetStoreScope Scope;
    size_t References = 0, Demands = 0, InputBytes = 0;
    bool StopRequested = false, Finished = false, Running = true;
    ResumableProducer Resume;
    std::chrono::steady_clock::time_point RetryAt;
    std::chrono::milliseconds Delay{1};
    std::stop_source Stop;
    std::shared_ptr<Reservation> Output;
    AssetPublication Result;
  };

  struct Demand {
    AssetConsumerId Consumer;
    AssetConsumerGeneration Generation;
    size_t Bytes = 0;
    bool Cancelled = false;
    std::shared_ptr<Job> Work;
  };

  struct Owner {
    AssetConsumerGeneration Generation;
    AssetStoreScope Scope;
    size_t Tickets = 0;
    bool Alive = true;
  };

  struct Execution {
    std::shared_ptr<State> Owner;
    std::shared_ptr<Job> Work;
    ResumableProducer Produce;
    bool Ended = false;
    void Run();
    ~Execution();
  };

  State(Tasks &pool, Limits limits) : Pool(pool), Budget(limits) {}

  [[nodiscard]] static std::optional<std::stop_source> Cancel(Demand &demand);
  void CancelTicket(uint64_t ticket) noexcept;
  void ReleaseTicket(uint64_t ticket) noexcept;
  void Retire(AssetConsumerId consumer) noexcept;
  [[nodiscard]] bool Advance(AssetConsumerId consumer, AssetConsumerGeneration generation) noexcept;
  [[nodiscard]] AssetPublication Poll(uint64_t ticket) noexcept;
  void Dispatch(const std::shared_ptr<Job> &job);
  void WakeStopped();
  [[nodiscard]] bool Defer(const std::shared_ptr<Job> &job, ResumableProducer producer);
  void Finish(const std::shared_ptr<Job> &job, AssetPublication result) noexcept;
  [[nodiscard]] static AssetPublication Receipt(const std::shared_ptr<Job> &job,
                                                std::expected<AssetRecord, std::string> result);
  void Remove(const std::shared_ptr<Job> &job);
  Tasks &Pool;
  Limits Budget;
  mutable std::mutex Lock;
  std::map<ScopedKey, std::shared_ptr<Job>, KeyLess> Jobs;
  std::map<uint64_t, Demand> Tickets;
  std::map<AssetConsumerId, Owner> Consumers;
  uint64_t NextConsumer = 1, NextTicket = 1;
  Snapshot Held;
};
}
#endif
