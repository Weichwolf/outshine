#include "SourceSet.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <cstdint>
#include <utility>
#include <optional>
#include <vector>

namespace outshine::Data {

constexpr double kMsPerS = 1000.0;

namespace {
constexpr double kRetryBaseMs = 250.0;
constexpr double kRetryCapMs = 4000.0;
constexpr int kRetryCapExponent = 4;
static_assert(kRetryBaseMs * (1U << static_cast<unsigned>(kRetryCapExponent)) == kRetryCapMs);
}

SourceSet::Query::Query(Query &&other) noexcept
    : Owner_(std::exchange(other.Owner_, nullptr)),
      Phase_(std::exchange(other.Phase_, Phase::Finished)),
      Request_(other.Request_),
      Candidates_(std::move(other.Candidates_)),
      Next_(std::exchange(other.Next_, 0)),
      Current_(std::exchange(other.Current_, nullptr)),
      At_(other.At_),
      Ticket_(std::exchange(other.Ticket_, Ticket::None)),
      Attempts_(std::exchange(other.Attempts_, 0)),
      RetryAtMs_(std::exchange(other.RetryAtMs_, 0.0)) {}

void SourceSet::Query::Finish() noexcept {
  Phase_ = Phase::Finished;
  Ticket_ = Ticket::None;
  Current_ = nullptr;
  RetryAtMs_ = 0.0;
  Next_ = Candidates_.size();
}

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

  std::ranges::sort(Sources_,

                    [](const std::unique_ptr<Source> &a, const std::unique_ptr<Source> &b) {
                      const SourceDecl &da = a->Declaration();
                      const SourceDecl &db = b->Declaration();
                      if (da.Kind != db.Kind) { return da.Kind < db.Kind; }
                      return da.Order < db.Order;
                    });
  return Registration::Accepted;
}

SourceSet::Query SourceSet::Ask(const Fetch &request) const {
  Query query(*this, request);
  for (const std::unique_ptr<Source> &source : Sources_) {
    if (source->Covers(request) == Coverage::Inside) { query.Candidates_.push_back(source.get()); }
  }
  return query;
}

Delivery SourceSet::Collect(Query &query, Transport &transport) {
  if (query.Phase_ == Query::Phase::Finished) { return Delivery::Consumed(); }
  if (query.Owner_ != this) { return Delivery::WireAfter(kRetryCapMs); }
  if (query.Phase_ == Query::Phase::Backoff) { return ResumeRetry(query, transport); }
  if (query.Candidates_.empty()) {
    const std::scoped_lock lock(LedgerMutex_);
    query.Finish();
    Ledger_.Undeclared++;
    return Delivery::NoSource();
  }
  for (;;) {
    if (query.Phase_ == Query::Phase::Ready) {
      if (query.Next_ >= query.Candidates_.size()) {
        const std::scoped_lock lock(LedgerMutex_);
        query.Finish();
        Ledger_.Vacant++;
        return Delivery::Nothing();
      }
      query.Current_ = query.Candidates_[query.Next_++];
      query.Attempts_ = 0;
      query.At_ = query.Current_->Serves(query.Request_);
      const SourceDecl &decl = query.Current_->Declaration();
      if (decl.Keeps == Cacheability::Forever) {
        if (std::optional<std::vector<uint8_t>> kept = Store_.Read(ContentKey(decl, query.At_))) {
          const std::scoped_lock lock(LedgerMutex_);
          Ledger_.Asked++;
          Ledger_.Delivered++;
          Ledger_.FromStore++;
          Ledger_.DeliveredBytes += static_cast<long long>(kept->size());
          query.Finish();
          return Delivery::From(decl.Id, query.At_, std::move(*kept));
        }
      }
      {
        const std::scoped_lock lock(LedgerMutex_);
        Ledger_.Asked++;
      }
      query.Ticket_ = query.Current_->Begin(query.At_, transport);
      query.Phase_ = Query::Phase::InFlight;
    }

    Fetched answer = query.Current_->Collect(query.At_, query.Ticket_, transport);
    if (answer.Where() == Fetched::State::Working) { return Delivery::Waiting(); }
    query.Ticket_ = Ticket::None;

    std::optional<Fetched::Settled> settled = answer.Take();
    if (!settled) { return Refuse(query, kRetryCapMs); }

    if (auto delivery =
            ProcessResponse(query, std::move(*settled), answer.RetryAfterS(), transport)) {
      return std::move(*delivery);
    }
  }
}

Delivery SourceSet::ResumeRetry(Query &query, Transport &transport) {
  const double nowMs = transport.NowMs();
  if (!std::isfinite(nowMs) || nowMs < 0.0) { return Refuse(query, kRetryCapMs); }
  if (nowMs < query.RetryAtMs_) { return Delivery::Waiting(); }
  query.RetryAtMs_ = 0.0;
  query.Ticket_ = query.Current_->Begin(query.At_, transport);
  query.Phase_ = Query::Phase::InFlight;
  return Delivery::Waiting();
}

std::optional<Delivery> SourceSet::ProcessResponse(Query &query,
                                                   Fetched::Settled response,
                                                   double retryAfterS,
                                                   Transport &transport) {
  const SourceDecl &decl = query.Current_->Declaration();
  const double retryAfterMs = retryAfterS * kMsPerS;
  const bool validDelay = std::isfinite(retryAfterMs) && retryAfterMs >= 0.0;
  switch (response.What) {
    case Meaning::Bytes: {
      if (decl.Keeps == Cacheability::Forever) {
        (void)Store_.Keep(
            ContentKey(decl, query.At_), response.Bytes.data(), response.Bytes.size());
      }
      const std::scoped_lock lock(LedgerMutex_);
      ++Ledger_.Delivered;
      Ledger_.DeliveredBytes += static_cast<long long>(response.Bytes.size());
      query.Finish();
      return Delivery::From(decl.Id, query.At_, std::move(response.Bytes));
    }
    case Meaning::Absent: {
      query.Current_ = nullptr;
      query.Phase_ = Query::Phase::Ready;
      const std::scoped_lock lock(LedgerMutex_);
      ++Ledger_.HandedOver;
      return std::nullopt;
    }
    case Meaning::Retry:
      if (validDelay && query.Attempts_ < decl.RetryBudget) {
        const double nowMs = transport.NowMs();
        const double backoffMs =
            std::ldexp(kRetryBaseMs, std::min(query.Attempts_, kRetryCapExponent));
        const double deadlineMs = nowMs + std::max(retryAfterMs, backoffMs);
        if (!std::isfinite(nowMs) || nowMs < 0.0 || !std::isfinite(deadlineMs) ||
            deadlineMs <= nowMs) {
          return Refuse(query, kRetryCapMs);
        }
        ++query.Attempts_;
        {
          const std::scoped_lock lock(LedgerMutex_);
          ++Ledger_.Retried;
        }
        query.Phase_ = Query::Phase::Backoff;
        query.RetryAtMs_ = deadlineMs;
        return Delivery::Waiting();
      }
      break;
    case Meaning::Refused: break;
    default: return Refuse(query, kRetryCapMs);
  }
  return Refuse(query, validDelay ? std::max(retryAfterMs, kRetryCapMs) : kRetryCapMs);
}

Delivery SourceSet::Refuse(Query &query, double afterMs) {
  query.Finish();
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.Refused;
  return Delivery::WireAfter(afterMs);
}

void SourceSet::Abandon(Query &query, Transport &transport) {
  if (query.Ticket_ != Ticket::None) { transport.Cancel(query.Ticket_); }
  query.Finish();
}

SourceSet::Ledger SourceSet::Counters() const {
  const std::scoped_lock lock(LedgerMutex_);
  return Ledger_;
}

}
