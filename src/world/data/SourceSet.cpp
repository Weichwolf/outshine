#include "SourceSet.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace outshine::Data {

namespace {
constexpr double kRetryBaseMs = 250.0;
constexpr double kRetryCapMs = 4000.0;
} // namespace

SourceSet::Registration SourceSet::Add(std::unique_ptr<Source> source) {
  if (!source) { return Registration::Unnamed; }
  const SourceDecl &decl = source->Declaration();
  if (decl.Id.empty()) { return Registration::Unnamed; }
  for (const std::unique_ptr<Source> &held : Sources_) {
    const SourceDecl &other = held->Declaration();
    if (other.Kind == decl.Kind && other.Order == decl.Order) {
      return Registration::DuplicateRank;
    }
  }
  Sources_.push_back(std::move(source));

  std::sort(Sources_.begin(),
            Sources_.end(),
            [](const std::unique_ptr<Source> &a, const std::unique_ptr<Source> &b) {
              const SourceDecl &da = a->Declaration();
              const SourceDecl &db = b->Declaration();
              if (da.Kind != db.Kind) { return da.Kind < db.Kind; }
              return da.Order < db.Order;
            });
  return Registration::Accepted;
}

SourceSet::Query SourceSet::Ask(const Request &request) const {
  Query query(request);
  for (const std::unique_ptr<Source> &source : Sources_) {
    if (source->Covers(request) == Coverage::Inside) { query.Candidates_.push_back(source.get()); }
  }
  return query;
}

Delivery SourceSet::Collect(Query &query, Transport &transport) {
  if (query.RetryAtMs_ > 0.0 && query.Current_ != nullptr) {
    if (transport.NowMs() < query.RetryAtMs_) { return Delivery::Waiting(); }
    query.RetryAtMs_ = 0.0;
    query.Ticket_ = query.Current_->Begin(query.At_, transport);
    return Delivery::Waiting();
  }
  if (query.Candidates_.empty()) {
    const std::lock_guard<std::mutex> lock(LedgerMutex_);
    Ledger_.Undeclared++;
    return Delivery::NoSource();
  }
  for (;;) {
    if (!query.Current_) {
      if (query.Next_ >= query.Candidates_.size()) {
        const std::lock_guard<std::mutex> lock(LedgerMutex_);
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
          const std::lock_guard<std::mutex> lock(LedgerMutex_);
          Ledger_.Asked++;
          Ledger_.Delivered++;
          Ledger_.FromStore++;
          Ledger_.DeliveredBytes += static_cast<long long>(kept.size());
          return Delivery::From(decl.Id, query.At_, std::move(kept));
        }
      }
      {
        const std::lock_guard<std::mutex> lock(LedgerMutex_);
        Ledger_.Asked++;
      }
      query.Ticket_ = query.Current_->Begin(query.At_, transport);
    }

    Fetched answer = query.Current_->Collect(query.At_, query.Ticket_, transport);
    if (answer.Where() == Fetched::State::Working) { return Delivery::Waiting(); }
    query.Ticket_ = Ticket::None;

    Meaning what = Meaning::Refused;
    std::vector<uint8_t> bytes;
    if (!answer.TryTake(&what, &bytes)) {
      {
        const std::lock_guard<std::mutex> ledger(LedgerMutex_);
        ++Ledger_.Refused;
      }
      return Delivery::WireAfter(kRetryCapMs);
    }

    const SourceDecl &decl = query.Current_->Declaration();
    switch (what) {
      case Meaning::Bytes: {
        if (decl.Keeps == Cacheability::Forever) {
          Store_.Keep(ContentKey(decl, query.At_), bytes.data(), bytes.size());
        }
        const std::lock_guard<std::mutex> lock(LedgerMutex_);
        Ledger_.Delivered++;
        Ledger_.DeliveredBytes += static_cast<long long>(bytes.size());
        return Delivery::From(decl.Id, query.At_, std::move(bytes));
      }
      case Meaning::Absent:

        query.Current_ = nullptr;
        {
          const std::lock_guard<std::mutex> lock(LedgerMutex_);
          Ledger_.HandedOver++;
        }
        continue;
      case Meaning::Retry:
        if (query.Attempts_ < decl.RetryBudget) {
          query.Attempts_++;
          {
            const std::lock_guard<std::mutex> lock(LedgerMutex_);
            Ledger_.Retried++;
          }
          query.Ticket_ = Ticket::None;
          query.RetryAtMs_ =
              transport.NowMs() +
              std::fmax(answer.RetryAfterS() * 1000.0,
                        std::fmin(std::ldexp(kRetryBaseMs, query.Attempts_ - 1), kRetryCapMs));
          return Delivery::Waiting();
        }
        [[fallthrough]];
      case Meaning::Refused:

        query.Current_ = nullptr;
        {
          const std::lock_guard<std::mutex> lock(LedgerMutex_);
          Ledger_.Refused++;
        }
        return Delivery::WireAfter(std::fmax(answer.RetryAfterS() * 1000.0, kRetryCapMs));
    }
  }
}

void SourceSet::Abandon(Query &query, Transport &transport) const {
  if (query.Ticket_ != Ticket::None) { transport.Cancel(query.Ticket_); }
  query.Ticket_ = Ticket::None;
  query.Current_ = nullptr;
  query.Next_ = query.Candidates_.size();
}

SourceSet::Ledger SourceSet::Counters() const {
  const std::lock_guard<std::mutex> lock(LedgerMutex_);
  return Ledger_;
}

} // namespace outshine::Data
