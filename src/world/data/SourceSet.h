#ifndef OUTSHINE_WORLD_DATA_SOURCESET_H
#define OUTSHINE_WORLD_DATA_SOURCESET_H

#include <memory>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "RawSourceCache.h"
#include "SourceDelivery.h"
#include <DataKind.h>
#include <Source.h>

namespace outshine::Data {

class SourceSet {
public:
  enum class Registration { Accepted, DuplicateRank, Unnamed, Sealed, InvalidDefinition };

  explicit SourceSet(RawSourceCache &store) : Store_(store) {}

  SourceSet(const SourceSet &) = delete;
  SourceSet &operator=(const SourceSet &) = delete;

  [[nodiscard]] Registration Add(std::unique_ptr<Source> source);
  [[nodiscard]] Registration AddAll(std::vector<std::unique_ptr<Source>> sources);

  void Seal() noexcept;

  [[nodiscard]] size_t Count() const noexcept { return Sources_.size(); }

  [[nodiscard]] const Source &At(size_t i) const { return *Sources_[i]; }

  class Query {
  public:
    Query(Query &&other) noexcept;
    Query &operator=(Query &&) = delete;
    Query(const Query &) = delete;
    Query &operator=(const Query &) = delete;

    [[nodiscard]] bool Undeclared() const noexcept { return Candidates_.empty(); }

  private:
    friend class SourceSet;

    enum class Phase { Ready, InFlight, Backoff, Finished };

    Query(const SourceSet &owner, SourceRequest request)
        : Owner_(&owner), Request_(std::move(request)) {}

    void Finish() noexcept;
    [[nodiscard]] std::string CacheKey() const;

    const SourceSet *Owner_;
    Phase Phase_ = Phase::Ready;

    SourceRequest Request_;
    std::vector<const Source *> Candidates_;
    size_t Next_ = 0;
    const Source *Current_ = nullptr;
    Address At_ = Address::Whole(0);
    Ticket Ticket_ = Ticket::None;
    int Attempts_ = 0;
    double RetryAtMs_ = 0.0;
  };

  [[nodiscard]] Query Ask(const SourceRequest &request) const;

  [[nodiscard]] SourceDelivery Collect(Query &query, Transport &transport);

  static void Abandon(Query &query, Transport &transport);

  struct Ledger {
    struct UsedSource {
      DataKind Kind = DataKind::Elevation;
      std::string Id;
      std::string Revision;
      std::string Key;
      Rank Order = Rank{0};
      long long Deliveries = 0;
    };

    long long Asked = 0, Delivered = 0, HandedOver = 0, Vacant = 0, Undeclared = 0;
    long long Refused = 0, Retried = 0, FromStore = 0;
    long long ProviderStarts = 0, RemoteStarts = 0;
    long long DeliveredBytes = 0;

    struct ProviderCost {
      uint64_t Calls = 0;
      double Ms = 0.0;
    };

    static constexpr size_t KindCount = static_cast<size_t>(DataKind::Weather) + 1;
    std::array<ProviderCost, KindCount> Providers{};
    std::vector<UsedSource> Sources;
  };

  [[nodiscard]] Ledger Counters() const;
  [[nodiscard]] std::array<Ledger::ProviderCost, Ledger::KindCount> ProviderCosts() const;

private:
  [[nodiscard]] std::optional<SourceDelivery> ReadStored(Query &query, Transport &transport);
  [[nodiscard]] SourceDelivery Deliver(Query &query, SourceResponse::Settled response);
  [[nodiscard]] std::optional<SourceDelivery>
  ProcessAbsence(Query &query, Transport &transport, std::optional<int> httpStatus = std::nullopt);
  void RecordStart(const SourceDecl &decl, bool first, bool started);
  void RecordDelivery(const SourceDecl &decl);
  void RecordProviderCall(DataKind kind, std::chrono::steady_clock::time_point began);

  [[nodiscard]] SourceDelivery ResumeRetry(Query &query, Transport &transport);
  [[nodiscard]] std::optional<SourceDelivery> StartCurrent(Query &query, Transport &transport);
  [[nodiscard]] std::optional<SourceDelivery> StartNext(Query &query, Transport &transport);

  [[nodiscard]] SourceDelivery
  Refuse(Query &query,
         double afterMs,
         SourceFailureReason reason = SourceFailureReason::ProviderRefused,
         std::optional<int> httpStatus = std::nullopt);

  [[nodiscard]] std::optional<SourceDelivery> ProcessResponse(Query &query,
                                                              SourceResponse::Settled response,
                                                              double retryAfterS,
                                                              Transport &transport);

  RawSourceCache &Store_;
  mutable std::mutex RegistryMutex_;
  bool Sealed_ = false;
  std::vector<std::unique_ptr<Source>> Sources_;

  mutable std::mutex LedgerMutex_;
  Ledger Ledger_;
  std::array<std::atomic<uint64_t>, Ledger::KindCount> ProviderNs_{};
  std::array<std::atomic<uint64_t>, Ledger::KindCount> ProviderCalls_{};
};

}
#endif
