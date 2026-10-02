#include "SourceSet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <memory>
#include <mutex>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <optional>
#include <vector>

#include "SourceRange.h"

namespace outshine::Data {

constexpr double kMsPerS = 1000.0;

namespace {
constexpr double kRetryBaseMs = 250.0;
constexpr double kRetryCapMs = 4000.0;
constexpr int kRetryCapExponent = 4;
static_assert(kRetryBaseMs * (1U << static_cast<unsigned>(kRetryCapExponent)) == kRetryCapMs);

bool Matches(const Fetch &request, const std::optional<RangeResponse> &origin, size_t bytes) {
  if (!request.Range()) { return !origin; }
  return origin && origin->Valid(bytes) && origin->Bytes == *request.Range() &&
         (request.EntityTag().empty() || request.EntityTag() == origin->EntityTag);
}

std::optional<FetchFailureReason> ValidateDemand(const Fetch &request, size_t maximum) {
  const auto &range = request.Range();
  if (!range) { return std::nullopt; }
  if (!range->Valid() || (!request.EntityTag().empty() && !StrongEntityTag(request.EntityTag()))) {
    return FetchFailureReason::InvalidRequest;
  }
  if (range->Length > std::numeric_limits<size_t>::max() - MaximumRangeRecordOverhead ||
      (maximum != 0 && range->Length > maximum)) {
    return FetchFailureReason::CapacityRefused;
  }
  return std::nullopt;
}

}

SourceSet::Query::Query(Query &&other) noexcept
    : Owner_(std::exchange(other.Owner_, nullptr)),
      Phase_(std::exchange(other.Phase_, Phase::Finished)),
      Request_(std::move(other.Request_)),
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

std::string SourceSet::Query::CacheKey() const {
  return ContentKey(Current_->Declaration(), At_, Request_.Range(), Request_.EntityTag());
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
      if (auto delivery = StartNext(query, transport)) { return std::move(*delivery); }
      if (query.Current_ == nullptr) { continue; }
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

std::optional<Delivery> SourceSet::StartNext(Query &query, Transport &transport) {
  if (query.Next_ >= query.Candidates_.size()) {
    const std::scoped_lock lock(LedgerMutex_);
    query.Finish();
    Ledger_.Vacant++;
    return Delivery::Nothing();
  }
  query.Current_ = query.Candidates_[query.Next_++];
  query.Attempts_ = 0;
  query.At_ = query.Current_->Serves(query.Request_);
  if (const auto problem =
          ValidateDemand(query.Request_, query.Current_->Declaration().MaximumPayloadBytes)) {
    return Refuse(query, kRetryCapMs, *problem);
  }
  if (auto stored = ReadStored(query)) { return stored; }
  if (query.Current_ == nullptr) { return std::nullopt; }
  return StartCurrent(query, transport);
}

std::optional<Delivery> SourceSet::ProcessAbsence(Query &query, std::optional<int> httpStatus) {
  if (query.Current_->Declaration().OnAbsent == AbsencePolicy::Fail) {
    return Refuse(query, kRetryCapMs, FetchFailureReason::ConfirmedAbsent, httpStatus);
  }
  query.Current_ = nullptr;
  query.Phase_ = Query::Phase::Ready;
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.HandedOver;
  return std::nullopt;
}

std::optional<Delivery> SourceSet::ReadStored(Query &query) {
  const SourceDecl &decl = query.Current_->Declaration();
  if (decl.Keeps != Cacheability::Forever) { return std::nullopt; }
  const auto &range = query.Request_.Range();
  const size_t maximum = range ? static_cast<size_t>(range->Length) + MaximumRangeRecordOverhead
                               : decl.MaximumPayloadBytes;
  const auto cell = query.At_.GeoCell();
  auto kept = !range && decl.Kind == DataKind::OriginalOsm && cell
                  ? Store_.LookupCell(decl, *cell)
                  : Store_.Lookup(query.CacheKey(), maximum);
  if (kept.Where == ContentStore::Presence::Unknown) { return std::nullopt; }
  if (kept.Where == ContentStore::Presence::Absent) {
    {
      const std::scoped_lock lock(LedgerMutex_);
      ++Ledger_.Asked;
    }
    return ProcessAbsence(query);
  }
  std::optional<RangeResponse> origin = std::nullopt;
  if (range) {
    origin = UnpackSourceRange(kept.Bytes);
    if (!Matches(query.Request_, origin, kept.Bytes.size())) { return std::nullopt; }
  }
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.Asked;
  ++Ledger_.Delivered;
  ++Ledger_.FromStore;
  Ledger_.DeliveredBytes += static_cast<long long>(kept.Bytes.size());
  RecordDelivery(decl);
  query.Finish();
  return Delivery::From(
      decl.Id, decl.Revision, query.At_, std::move(kept.Bytes), SourceKey(decl), std::move(origin));
}

Delivery SourceSet::ResumeRetry(Query &query, Transport &transport) {
  const double nowMs = transport.NowMs();
  if (!std::isfinite(nowMs) || nowMs < 0.0) { return Refuse(query, kRetryCapMs); }
  if (nowMs < query.RetryAtMs_) { return Delivery::Waiting(); }
  query.RetryAtMs_ = 0.0;
  if (auto refused = StartCurrent(query, transport)) { return std::move(*refused); }
  return Delivery::Waiting();
}

std::optional<Delivery> SourceSet::StartCurrent(Query &query, Transport &transport) {
  const auto &range = query.Request_.Range();
  const Fetch served =
      range
          ? Fetch(query.Request_.Kind(), query.At_, *range, std::string(query.Request_.EntityTag()))
          : Fetch(query.Request_.Kind(), query.At_);
  const auto started = query.Current_->Begin(served, transport);
  RecordStart(query.Current_->Declaration(),
              query.Phase_ == Query::Phase::Ready,
              started && *started != Ticket::None);
  if (!started) { return Refuse(query, kRetryCapMs, started.error()); }
  query.Ticket_ = *started;
  query.Phase_ = Query::Phase::InFlight;
  return std::nullopt;
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

Delivery SourceSet::Deliver(Query &query, Fetched::Settled response) {
  const SourceDecl &decl = query.Current_->Declaration();
  if (decl.MaximumPayloadBytes != 0 && response.Bytes.size() > decl.MaximumPayloadBytes) {
    return Refuse(query, kRetryCapMs, FetchFailureReason::CapacityRefused);
  }
  if (!Matches(query.Request_, response.Range, response.Bytes.size())) {
    return Refuse(query, kRetryCapMs, FetchFailureReason::CorruptPayload);
  }
  if (decl.Keeps == Cacheability::Forever) {
    if (response.Range) {
      auto record = PackSourceRange(response.Bytes, *response.Range);
      if (!record) { return Refuse(query, kRetryCapMs, FetchFailureReason::CapacityRefused); }
      (void)Store_.Keep(query.CacheKey(), record->data(), record->size());
    } else {
      const auto cell = query.At_.GeoCell();
      if (decl.Kind == DataKind::OriginalOsm && cell) {
        (void)Store_.KeepCell(decl, *cell, response.Bytes);
      } else {
        (void)Store_.Keep(query.CacheKey(), response.Bytes.data(), response.Bytes.size());
      }
    }
  }
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.Delivered;
  Ledger_.DeliveredBytes += static_cast<long long>(response.Bytes.size());
  RecordDelivery(decl);
  query.Finish();
  return Delivery::From(decl.Id,
                        decl.Revision,
                        query.At_,
                        std::move(response.Bytes),
                        SourceKey(decl),
                        std::move(response.Range));
}

std::optional<Delivery> SourceSet::ProcessResponse(Query &query,
                                                   Fetched::Settled response,
                                                   double retryAfterS,
                                                   Transport &transport) {
  const SourceDecl &decl = query.Current_->Declaration();
  const double retryAfterMs = retryAfterS * kMsPerS;
  const bool validDelay = std::isfinite(retryAfterMs) && retryAfterMs >= 0.0;
  switch (response.What) {
    case Meaning::Bytes: return Deliver(query, std::move(response));
    case Meaning::Absent: {
      if (response.Evidence == AbsenceEvidence::HttpNotFound &&
          decl.Keeps == Cacheability::Forever) {
        const auto lifetime = decl.Revision.empty() ? ContentStore::UnpinnedAbsenceLifetimeS
                                                    : ContentStore::PinnedAbsenceLifetimeS;
        (void)Store_.KeepAbsent(query.CacheKey(), lifetime);
      }
      return ProcessAbsence(query, response.HttpStatus);
    }
    case Meaning::Retry:
      if (validDelay && query.Attempts_ < decl.RetryBudget) {
        const double nowMs = transport.NowMs();
        const double backoffMs =
            std::ldexp(kRetryBaseMs, std::min(query.Attempts_, kRetryCapExponent));
        const double deadlineMs = nowMs + std::max(retryAfterMs, backoffMs);
        if (!std::isfinite(nowMs) || nowMs < 0.0 || !std::isfinite(deadlineMs) ||
            deadlineMs <= nowMs) {
          return Refuse(query, kRetryCapMs, response.Reason, response.HttpStatus);
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
    default: return Refuse(query, kRetryCapMs, response.Reason, response.HttpStatus);
  }
  return Refuse(query,
                validDelay ? std::max(retryAfterMs, kRetryCapMs) : kRetryCapMs,
                response.Reason,
                response.HttpStatus);
}

Delivery SourceSet::Refuse(Query &query,
                           double afterMs,
                           FetchFailureReason reason,
                           std::optional<int> httpStatus) {
  const SourceDecl &decl = query.Current_->Declaration();
  const std::string sourceId = decl.Id;
  const std::string sourceRevision = decl.Revision;
  FetchFailure failure{.Kind = query.Request_.Kind(),
                       .Requested = query.Request_.Where(),
                       .Served = query.At_,
                       .SourceId = sourceId,
                       .SourceRevision = sourceRevision,
                       .SourceKey = SourceKey(decl),
                       .Reason = reason,
                       .HttpStatus = httpStatus,
                       .Retries = query.Attempts_};
  query.Finish();
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.Refused;
  return Delivery::WireAfter(afterMs, sourceId, sourceRevision, std::move(failure));
}

void SourceSet::Abandon(Query &query, Transport &transport) {
  if (query.Ticket_ != Ticket::None && query.Current_ != nullptr) {
    query.Current_->Cancel(query.Ticket_, transport);
  }
  query.Finish();
}

SourceSet::Ledger SourceSet::Counters() const {
  const std::scoped_lock lock(LedgerMutex_);
  return Ledger_;
}

}
