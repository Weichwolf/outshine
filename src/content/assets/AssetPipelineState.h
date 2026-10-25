#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPIPELINESTATE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPIPELINESTATE_H
#include "AssetPipeline.h"
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

  struct Job {
    AssetProductKey Key;
    size_t References = 0, Demands = 0, InputBytes = 0;
    bool StopRequested = false, Finished = false;
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
    size_t Tickets = 0;
    bool Alive = true;
  };

  struct Execution {
    std::shared_ptr<State> Owner;
    std::shared_ptr<Job> Work;
    Producer Produce;
    bool Ended = false;
    void Run();
    ~Execution();
  };

  State(Tasks &pool, Limits limits) : Pool(pool), Budget(limits) {}

  [[nodiscard]] std::optional<std::stop_source> Cancel(Demand &demand);
  void CancelTicket(uint64_t ticket) noexcept;
  void ReleaseTicket(uint64_t ticket) noexcept;
  void Retire(AssetConsumerId consumer) noexcept;
  [[nodiscard]] bool Advance(AssetConsumerId consumer, AssetConsumerGeneration generation) noexcept;
  [[nodiscard]] AssetPublication Poll(uint64_t ticket) const noexcept;
  void Finish(const std::shared_ptr<Job> &job, AssetPublication result) noexcept;
  [[nodiscard]] AssetPublication Receipt(const std::shared_ptr<Job> &job,
                                         std::expected<AssetRecord, std::string> result);
  void Remove(const std::shared_ptr<Job> &job);
  Tasks &Pool;
  Limits Budget;
  mutable std::mutex Lock;
  std::map<AssetProductKey, std::shared_ptr<Job>> Jobs;
  std::map<uint64_t, Demand> Tickets;
  std::map<AssetConsumerId, Owner> Consumers;
  uint64_t NextConsumer = 1, NextTicket = 1;
  Snapshot Held;
};
}
#endif
