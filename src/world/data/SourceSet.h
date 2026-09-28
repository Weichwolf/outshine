#ifndef OUTSHINE_WORLD_DATA_SOURCESET_H
#define OUTSHINE_WORLD_DATA_SOURCESET_H

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ContentStore.h"
#include "Delivery.h"
#include "DataKind.h"
#include "Source.h"

namespace outshine::Data {

class SourceSet {
public:
  enum class Registration { Accepted, DuplicateRank, Unnamed, Sealed };

  explicit SourceSet(ContentStore &store) : Store_(store) {}

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

    Query(const SourceSet &owner, Fetch request) : Owner_(&owner), Request_(request) {}

    void Finish() noexcept;

    const SourceSet *Owner_;
    Phase Phase_ = Phase::Ready;

    Fetch Request_;
    std::vector<const Source *> Candidates_;
    size_t Next_ = 0;
    const Source *Current_ = nullptr;
    Address At_ = Address::Whole(0);
    Ticket Ticket_ = Ticket::None;
    int Attempts_ = 0;
    double RetryAtMs_ = 0.0;
  };

  [[nodiscard]] Query Ask(const Fetch &request) const;

  [[nodiscard]] Delivery Collect(Query &query, Transport &transport);

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
    std::vector<UsedSource> Sources;
  };

  [[nodiscard]] Ledger Counters() const;

private:
  [[nodiscard]] std::optional<Delivery> ReadStored(Query &query);
  [[nodiscard]] std::optional<Delivery> ProcessAbsence(Query &query);
  void RecordStart(const SourceDecl &decl, bool first, bool started);
  void RecordDelivery(const SourceDecl &decl);

  [[nodiscard]] Delivery ResumeRetry(Query &query, Transport &transport);
  [[nodiscard]] std::optional<Delivery> StartCurrent(Query &query, Transport &transport);

  [[nodiscard]] Delivery Refuse(Query &query,
                                double afterMs,
                                FetchFailureReason reason = FetchFailureReason::ProviderRefused);

  [[nodiscard]] std::optional<Delivery> ProcessResponse(Query &query,
                                                        Fetched::Settled response,
                                                        double retryAfterS,
                                                        Transport &transport);

  ContentStore &Store_;
  mutable std::mutex RegistryMutex_;
  bool Sealed_ = false;
  std::vector<std::unique_ptr<Source>> Sources_;

  mutable std::mutex LedgerMutex_;
  Ledger Ledger_;
};

}
#endif
