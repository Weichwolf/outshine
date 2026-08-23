#ifndef SOURCESET_H
#define SOURCESET_H

#include <memory>
#include <mutex>
#include <vector>

#include "ContentStore.h"
#include "Delivery.h"
#include "Source.h"

namespace outshine::Data {

class SourceSet {
public:
  enum class Registration { Accepted, DuplicateRank, Unnamed };

  explicit SourceSet(ContentStore &store) : Store_(store) {}

  SourceSet(const SourceSet &) = delete;
  SourceSet &operator=(const SourceSet &) = delete;

  [[nodiscard]] Registration Add(std::unique_ptr<Source> source);

  [[nodiscard]] size_t Count() const noexcept { return Sources_.size(); }
  [[nodiscard]] const Source &At(size_t i) const { return *Sources_[i]; }

  class Query {
  public:
    Query(Query &&) = default;
    Query &operator=(Query &&) = default;
    Query(const Query &) = delete;
    Query &operator=(const Query &) = delete;

    [[nodiscard]] bool Undeclared() const noexcept { return Candidates_.empty(); }

  private:
    friend class SourceSet;
    explicit Query(Request request) : Request_(std::move(request)) {}

    Request Request_;
    std::vector<const Source *> Candidates_;
    size_t Next_ = 0;
    const Source *Current_ = nullptr;
    Address At_ = Address::Whole(0);
    Ticket Ticket_ = Ticket::None;
    int Attempts_ = 0;
    double RetryAtMs_ = 0.0;
  };

  [[nodiscard]] Query Ask(const Request &request) const;

  [[nodiscard]] Delivery Collect(Query &query, Transport &transport);

  void Abandon(Query &query, Transport &transport) const;

  struct Ledger {
    long long Asked = 0, Delivered = 0, HandedOver = 0, Vacant = 0, Undeclared = 0;
    long long Refused = 0, Retried = 0, FromStore = 0;
    long long DeliveredBytes = 0;
  };
  [[nodiscard]] Ledger Counters() const;

private:
  ContentStore &Store_;
  std::vector<std::unique_ptr<Source>> Sources_;

  mutable std::mutex LedgerMutex_;
  Ledger Ledger_;
};

}
#endif
