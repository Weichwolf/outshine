#include "SourceSet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <memory>
#include <mutex>
#include <cstdint>
#include <iterator>
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
  std::vector<std::unique_ptr<Source>> one;
  one.push_back(std::move(source));
  return AddAll(std::move(one));
}

SourceSet::Registration SourceSet::AddAll(std::vector<std::unique_ptr<Source>> sources) {
  const std::scoped_lock lock(RegistryMutex_);
  if (Sealed_) { return Registration::Sealed; }
  for (size_t at = 0; at < sources.size(); ++at) {
    if (!sources[at] || sources[at]->Declaration().Id.empty()) { return Registration::Unnamed; }
    const SourceDecl &decl = sources[at]->Declaration();
    for (const std::unique_ptr<Source> &held : Sources_) {
      const SourceDecl &other = held->Declaration();
      if (other.Kind == decl.Kind && other.Order == decl.Order) {
        return Registration::DuplicateRank;
      }
    }
    for (size_t previous = 0; previous < at; ++previous) {
      const SourceDecl &other = sources[previous]->Declaration();
      if (other.Kind == decl.Kind && other.Order == decl.Order) {
        return Registration::DuplicateRank;
      }
    }
  }
  std::vector<Ledger::UsedSource> added;
  added.reserve(sources.size());
  for (const auto &source : sources) {
    const SourceDecl &decl = source->Declaration();
    added.push_back({.Kind = decl.Kind,
                     .Id = decl.Id,
                     .Revision = decl.Revision,
                     .Key = SourceKey(decl),
                     .Order = decl.Order});
  }
  Sources_.insert(Sources_.end(),
                  std::make_move_iterator(sources.begin()),
                  std::make_move_iterator(sources.end()));

  std::ranges::sort(Sources_,

                    [](const std::unique_ptr<Source> &a, const std::unique_ptr<Source> &b) {
                      const SourceDecl &da = a->Declaration();
                      const SourceDecl &db = b->Declaration();
                      if (da.Kind != db.Kind) { return da.Kind < db.Kind; }
                      return da.Order < db.Order;
                    });
  const std::scoped_lock ledger(LedgerMutex_);
  Ledger_.Sources.insert(Ledger_.Sources.end(),
                         std::make_move_iterator(added.begin()),
                         std::make_move_iterator(added.end()));
  std::ranges::sort(Ledger_.Sources, [](const Ledger::UsedSource &a, const Ledger::UsedSource &b) {
    if (a.Kind != b.Kind) { return a.Kind < b.Kind; }
    return a.Order < b.Order;
  });
  return Registration::Accepted;
}

void SourceSet::Seal() noexcept {
  const std::scoped_lock lock(RegistryMutex_);
  Sealed_ = true;
}

SourceSet::Query SourceSet::Ask(const Fetch &request) const {
  const std::scoped_lock lock(RegistryMutex_);
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
          RecordDelivery(decl);
          query.Finish();
          return Delivery::From(decl.Id, decl.Revision, query.At_, std::move(*kept));
        }
      }
      query.Ticket_ = query.Current_->Begin(query.At_, transport);
      RecordStart(decl, true, query.Ticket_ != Ticket::None);
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
  RecordStart(query.Current_->Declaration(), false, query.Ticket_ != Ticket::None);
  query.Phase_ = Query::Phase::InFlight;
  return Delivery::Waiting();
}

void SourceSet::RecordStart(const SourceDecl &decl, bool first, bool started) {
  const std::scoped_lock lock(LedgerMutex_);
  if (first) { Ledger_.Asked++; }
  Ledger_.ProviderStarts++;
  if (started && decl.Latency != LatencyClass::Local) { Ledger_.RemoteStarts++; }
}

void SourceSet::RecordDelivery(const SourceDecl &decl) {
  const auto use = std::ranges::find_if(Ledger_.Sources, [&decl](const Ledger::UsedSource &row) {
    return row.Kind == decl.Kind && row.Order == decl.Order;
  });
  assert(use != Ledger_.Sources.end());
  if (use != Ledger_.Sources.end()) { ++use->Deliveries; }
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
      RecordDelivery(decl);
      query.Finish();
      return Delivery::From(decl.Id, decl.Revision, query.At_, std::move(response.Bytes));
    }
    case Meaning::Absent: {
      if (decl.OnAbsent == AbsencePolicy::Fail) { return Refuse(query, kRetryCapMs); }
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
  return Refuse(
      query, validDelay ? std::max(retryAfterMs, kRetryCapMs) : kRetryCapMs, response.Reason);
}

Delivery SourceSet::Refuse(Query &query, double afterMs, FetchFailureReason reason) {
  const SourceDecl &decl = query.Current_->Declaration();
  const std::string sourceId = decl.Id;
  const std::string sourceRevision = decl.Revision;
  FetchFailure failure{.Kind = query.Request_.Kind(),
                       .Requested = query.Request_.Where(),
                       .Served = query.At_,
                       .SourceId = sourceId,
                       .SourceRevision = sourceRevision,
                       .SourceKey = SourceKey(decl),
                       .Reason = reason};
  query.Finish();
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.Refused;
  return Delivery::WireAfter(afterMs, sourceId, sourceRevision, std::move(failure));
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
