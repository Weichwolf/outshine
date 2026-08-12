#ifndef SOURCESET_H
#define SOURCESET_H

#include <memory>
#include <mutex>
#include <vector>

#include "ContentStore.h"
#include "Delivery.h"
#include "Source.h"

namespace outshine::Data {

/* THE SELECTOR, and it is the same pattern the generators use: filter by kind, keep what declares
 * the request inside its own domain, ask in declared rank order.
 *
 * AN ABSENCE FROM ONE SOURCE HANDS OVER TO THE NEXT, and the terminal absence is the exhaustion of
 * the list — never the first refusal. This is the whole reason to have plugins: a national 1 m DEM
 * over one country falling through to a global pyramid everywhere else is a registration here, and
 * was unspellable while the first absence was final at the node.
 *
 * A DUPLICATE RANK WITHIN ONE KIND IS REFUSED AT REGISTRATION — the rule the generator registry
 * already states — so who answers first is never a run-time coin toss. */
class SourceSet {
public:
  enum class Registration { Accepted, DuplicateRank, Unnamed };

  explicit SourceSet(ContentStore &store) : Store_(store) {}

  SourceSet(const SourceSet &) = delete;
  SourceSet &operator=(const SourceSet &) = delete;

  [[nodiscard]] Registration Add(std::unique_ptr<Source> source);

  [[nodiscard]] size_t Count() const noexcept { return Sources_.size(); }
  [[nodiscard]] const Source &At(size_t i) const { return *Sources_[i]; }

  /* ONE QUESTION IN FLIGHT. The candidate list is fixed when the query is made — by `Covers` alone,
   * with no I/O — and the query then walks it. A caller holds this across polls, so it is movable
   * and not copyable: two copies would collect one ticket twice. */
  class Query {
  public:
    Query(Query &&) = default;
    Query &operator=(Query &&) = default;
    Query(const Query &) = delete;
    Query &operator=(const Query &) = delete;

    /* No source declared this request inside its domain. A declaration error, decidable before any
     * work, and it is why the caller need not poll at all. */
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
  };

  /* PURE SELECTION: `Covers` and the declared rank, no allocation beyond the candidate list, no
   * transport touched. */
  [[nodiscard]] Query Ask(const Request &request) const;

  /* Advances the query by one poll. Pending means ask again; every other answer is final for this
   * query. */
  [[nodiscard]] Delivery Collect(Query &query, Transport &transport);

  /* Cancel is a real operation: a caller that lets go must not leave a transfer running. */
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
  /* The pool's threads collect concurrently and every one of them keeps score. */
  mutable std::mutex LedgerMutex_;
  Ledger Ledger_;
};

} // namespace outshine::Data
#endif
