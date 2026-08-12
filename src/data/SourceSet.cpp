#include "SourceSet.h"

#include <algorithm>
#include <utility>

namespace outshine::Data {

SourceSet::Registration SourceSet::Add(std::unique_ptr<Source> source) {
  if (!source) return Registration::Unnamed;
  const SourceDecl &decl = source->Declaration();
  if (decl.Id.empty()) return Registration::Unnamed;
  for (const std::unique_ptr<Source> &held : Sources_) {
    const SourceDecl &other = held->Declaration();
    if (other.Kind == decl.Kind && other.Order == decl.Order) return Registration::DuplicateRank;
  }
  Sources_.push_back(std::move(source));
  /* Held sorted, so the walk IS the rank order and no comparator travels with the query. */
  std::sort(Sources_.begin(), Sources_.end(),
            [](const std::unique_ptr<Source> &a, const std::unique_ptr<Source> &b) {
              const SourceDecl &da = a->Declaration();
              const SourceDecl &db = b->Declaration();
              if (da.Kind != db.Kind) return da.Kind < db.Kind;
              return da.Order < db.Order;
            });
  return Registration::Accepted;
}

SourceSet::Query SourceSet::Ask(const Request &request) const {
  Query query(request);
  for (const std::unique_ptr<Source> &source : Sources_)
    if (source->Covers(request) == Coverage::Inside) query.Candidates_.push_back(source.get());
  return query;
}

Delivery SourceSet::Collect(Query &query, Transport &transport) {
  if (query.Candidates_.empty()) {
    std::lock_guard<std::mutex> lock(LedgerMutex_);
    Ledger_.Undeclared++;
    return Delivery::NoSource();
  }
  for (;;) {
    if (!query.Current_) {
      if (query.Next_ >= query.Candidates_.size()) {
        std::lock_guard<std::mutex> lock(LedgerMutex_);
        Ledger_.Vacant++;
        return Delivery::Nothing();
      }
      query.Current_ = query.Candidates_[query.Next_++];
      query.Attempts_ = 0;
      query.At_ = query.Current_->Serves(query.Request_);
      const SourceDecl &decl = query.Current_->Declaration();
      if (decl.Keeps == Cacheability::Forever) {
        std::vector<uint8_t> kept;
        if (Store_.TryRead(ContentKey(decl, query.At_), &kept)) {
          std::lock_guard<std::mutex> lock(LedgerMutex_);
          Ledger_.Asked++;
          Ledger_.Delivered++;
          Ledger_.FromStore++;
          Ledger_.DeliveredBytes += (long long)kept.size();
          return Delivery::From(decl.Id, query.At_, std::move(kept));
        }
      }
      {
        std::lock_guard<std::mutex> lock(LedgerMutex_);
        Ledger_.Asked++;
      }
      query.Ticket_ = query.Current_->Begin(query.At_, transport);
    }

    Fetched answer = query.Current_->Collect(query.At_, query.Ticket_, transport);
    if (answer.Where() == Fetched::State::Working) return Delivery::Waiting();
    query.Ticket_ = Ticket::None;

    Meaning what = Meaning::Refused;
    std::vector<uint8_t> bytes;
    if (!answer.TryTake(&what, &bytes)) return Delivery::Waiting();

    const SourceDecl &decl = query.Current_->Declaration();
    switch (what) {
      case Meaning::Bytes: {
        if (decl.Keeps == Cacheability::Forever)
          Store_.Keep(ContentKey(decl, query.At_), bytes.data(), bytes.size());
        std::lock_guard<std::mutex> lock(LedgerMutex_);
        Ledger_.Delivered++;
        Ledger_.DeliveredBytes += (long long)bytes.size();
        return Delivery::From(decl.Id, query.At_, std::move(bytes));
      }
      case Meaning::Absent:
        /* THE HANDOVER. This source declared the place inside its domain and found nothing there;
         * the next one down may still hold it, and only the exhaustion of the list is terminal. */
        query.Current_ = nullptr;
        {
          std::lock_guard<std::mutex> lock(LedgerMutex_);
          Ledger_.HandedOver++;
        }
        continue;
      case Meaning::Retry:
        if (query.Attempts_ < decl.RetryBudget) {
          query.Attempts_++;
          {
            std::lock_guard<std::mutex> lock(LedgerMutex_);
            Ledger_.Retried++;
          }
          query.Ticket_ = query.Current_->Begin(query.At_, transport);
          return Delivery::Waiting();
        }
        [[fallthrough]];
      case Meaning::Refused:
        /* A REFUSAL IS NOT AN ABSENCE and must never be cached as one: it is a statement about this
         * request or the wire, so the query ends and the caller may ask again from scratch. */
        query.Current_ = nullptr;
        {
          std::lock_guard<std::mutex> lock(LedgerMutex_);
          Ledger_.Refused++;
        }
        return Delivery::Wire();
    }
  }
}

void SourceSet::Abandon(Query &query, Transport &transport) const {
  if (query.Ticket_ != Ticket::None) transport.Cancel(query.Ticket_);
  query.Ticket_ = Ticket::None;
  query.Current_ = nullptr;
  query.Next_ = query.Candidates_.size();
}

SourceSet::Ledger SourceSet::Counters() const {
  std::lock_guard<std::mutex> lock(LedgerMutex_);
  return Ledger_;
}

} // namespace outshine::Data
