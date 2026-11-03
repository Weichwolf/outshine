#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPIPELINESTATE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPIPELINESTATE_H
#include "AssetPipeline.h"
#include "MemoryBudget.h"
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace outshine {
struct AssetPipeline::State : std::enable_shared_from_this<State> {
  struct PublicationReservation {
    MemoryBudget::Reservation Receipt;
    MemoryBudget::Reservation Workspace;
    size_t ConsumerBytes = 0;
  };

  struct ScopedKey {
    AssetStoreScope Scope;
    KeyDomain Domain;
    AssetProductKey Key;
  };

  struct KeyLess {
    [[nodiscard]] bool operator()(const ScopedKey &a, const ScopedKey &b) const noexcept {
      if (a.Scope.Identity.owner_before(b.Scope.Identity)) { return true; }
      if (b.Scope.Identity.owner_before(a.Scope.Identity)) { return false; }
      if (a.Domain != b.Domain) { return a.Domain < b.Domain; }
      return a.Key < b.Key;
    }
  };

  struct Job {
    uint64_t Sequence = 0;
    AssetProductKey Key;
    KeyDomain Domain;
    AssetStoreScope Scope;
    size_t References = 0, Demands = 0;
    MemoryBudget::Reservation Inputs;
    std::optional<size_t> NextInputBytes;
    bool StopRequested = false, Finished = false, Running = true;
    LogicalProducer Resume;
    std::chrono::steady_clock::time_point RetryAt;
    std::chrono::milliseconds Delay{1};
    std::stop_source Stop;
    std::shared_ptr<PublicationReservation> Output;
    AssetPublication Result;
  };

  struct Demand {
    AssetConsumerId Consumer;
    AssetConsumerGeneration Generation;
    MemoryBudget::Reservation Workspace;
    bool Cancelled = false;
    std::shared_ptr<Job> Work;
    std::optional<AssetRequestKey> Address;
  };

  struct Owner {
    AssetConsumerGeneration Generation;
    AssetStoreScope Scope;
    size_t Tickets = 0;
    MemoryBudget::Reservation Workspace;
    bool Alive = true;
  };

  struct Execution {
    std::shared_ptr<State> Owner;
    std::shared_ptr<Job> Work;
    LogicalProducer Produce;
    bool Ended = false;
    bool Joined = false;
    [[nodiscard]] std::expected<bool, AssetPublicationError> PrepareStep();
    void Run();
    ~Execution();
  };

  State(Tasks &pool, Limits limits)
      : Pool(pool),
        Budget(limits),
        Inputs(limits.InputBytesMost),
        Receipts(limits.ReceiptBytesMost) {}

  [[nodiscard]] static std::optional<std::stop_source> Cancel(Demand &demand);
  void CancelTicket(uint64_t ticket) noexcept;
  void ReleaseTicket(uint64_t ticket) noexcept;
  void Retire(AssetConsumerId consumer) noexcept;
  [[nodiscard]] bool Advance(AssetConsumerId consumer, AssetConsumerGeneration generation) noexcept;
  [[nodiscard]] AssetPublication Poll(uint64_t ticket) noexcept;
  void Dispatch(const std::shared_ptr<Job> &job);
  void WakeStopped();
  [[nodiscard]] bool Defer(const std::shared_ptr<Job> &job, LogicalProducer producer);
  [[nodiscard]] std::expected<bool, AssetPublicationError>
  AdmitInput(const std::shared_ptr<Job> &job);
  [[nodiscard]] size_t OldestInputGrowth(uint64_t sequence) const;
  [[nodiscard]] size_t InputRoom(uint64_t sequence) const;
  [[nodiscard]] size_t ProducerInputLimit() const;
  [[nodiscard]] std::expected<bool, AssetPublicationError> Resolve(const std::shared_ptr<Job> &job,
                                                                   const ResolvedWork &resolved);
  void Join(const std::shared_ptr<Job> &from, const std::shared_ptr<Job> &to);
  void RetireJoined(const std::shared_ptr<Job> &job);
  void PruneRequest(const Demand &demand);
  [[nodiscard]] static AssetPublication Failure(const std::shared_ptr<Job> &job,
                                                LogicalFailure error);
  void Finish(const std::shared_ptr<Job> &job, AssetPublication result) noexcept;
  [[nodiscard]] AssetPublication Receipt(const std::shared_ptr<Job> &job,
                                         std::expected<AssetProduct, std::string> result);
  void Remove(const std::shared_ptr<Job> &job);
  static void ReleaseConsumerWorkspace(Job &job);
  Tasks &Pool;
  Limits Budget;
  MemoryBudget Inputs, Receipts;
  mutable std::mutex Lock;
  std::map<ScopedKey, std::shared_ptr<Job>, KeyLess> Jobs;
  std::map<uint64_t, Demand> Tickets;
  std::map<AssetConsumerId, Owner> Consumers;
  uint64_t NextConsumer = 1, NextTicket = 1;
  Snapshot Held;
  size_t JobInputBytes = 0;
};
}
#endif
