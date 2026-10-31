#include "SourceSet.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
constexpr double kNsPerMs = 1'000'000.0;
constexpr double kRetryBaseMs = 250.0;
constexpr double kRetryCapMs = 4000.0;
constexpr int kRetryCapExponent = 4;
static_assert(kRetryBaseMs * (1U << static_cast<unsigned>(kRetryCapExponent)) == kRetryCapMs);

bool Matches(const SourceRequest &request,
             const std::optional<RangeResponse> &origin,
             size_t bytes) {
  if (!request.Range()) { return !origin; }
  return origin && origin->Valid(bytes) && origin->Bytes == *request.Range() &&
         (request.EntityTag().empty() || request.EntityTag() == origin->EntityTag);
}

std::optional<SourceFailureReason> ValidateDemand(const SourceRequest &request, size_t maximum) {
  const auto &range = request.Range();
  if (!range) { return std::nullopt; }
  if (!range->Valid() || (!request.EntityTag().empty() && !StrongEntityTag(request.EntityTag()))) {
    return SourceFailureReason::InvalidRequest;
  }
  if (range->Length > std::numeric_limits<size_t>::max() - MaximumRangeRecordOverhead ||
      (maximum != 0 && range->Length > maximum)) {
    return SourceFailureReason::CapacityRefused;
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
    if (!SourceIdentityEncodable(decl)) { return Registration::InvalidDefinition; }
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

std::optional<size_t> SourceSet::Query::PayloadBytesMost() const noexcept {
  size_t bytes = 0;
  for (const auto *source : Candidates_) {
    const auto most = source->Declaration().MaximumPayloadBytes;
    if (most == 0) { return std::nullopt; }
    bytes = std::max(bytes, most);
  }
  return bytes;
}

SourceSet::Query SourceSet::Ask(const SourceRequest &request) const {
  const std::scoped_lock lock(RegistryMutex_);
  Query query(*this, request);
  for (const std::unique_ptr<Source> &source : Sources_) {
    if (source->Covers(request) == Coverage::Inside) { query.Candidates_.push_back(source.get()); }
  }
  return query;
}

SourceDelivery SourceSet::Collect(Query &query, SourceTransport &transport) {
  if (query.Phase_ == Query::Phase::Finished) { return SourceDelivery::Consumed(); }
  if (query.Owner_ != this) { return SourceDelivery::WireAfter(kRetryCapMs); }
  if (query.Phase_ == Query::Phase::Backoff) { return ResumeRetry(query, transport); }
  if (query.Candidates_.empty()) {
    const std::scoped_lock lock(LedgerMutex_);
    query.Finish();
    Ledger_.Undeclared++;
    return SourceDelivery::NoSource();
  }
  for (;;) {
    if (query.Phase_ == Query::Phase::Ready) {
      if (auto delivery = StartNext(query, transport)) { return std::move(*delivery); }
      if (query.Current_ == nullptr) { continue; }
    }

    const auto began = std::chrono::steady_clock::now();
    SourceResponse answer = query.Current_->Collect(query.At_, query.Ticket_, transport);
    RecordProviderCall(query.Current_->Declaration().Kind, began);
    if (answer.Where() == SourceResponse::State::Working) { return SourceDelivery::Waiting(); }
    query.Ticket_ = Ticket::None;

    std::optional<SourceResponse::Settled> settled = answer.Take();
    if (!settled) { return Refuse(query, kRetryCapMs); }

    if (auto delivery =
            ProcessResponse(query, std::move(*settled), answer.RetryAfterS(), transport)) {
      return std::move(*delivery);
    }
  }
}

std::optional<SourceDelivery> SourceSet::StartNext(Query &query, SourceTransport &transport) {
  if (query.Next_ >= query.Candidates_.size()) {
    const std::scoped_lock lock(LedgerMutex_);
    query.Finish();
    Ledger_.Vacant++;
    return SourceDelivery::Nothing();
  }
  query.Current_ = query.Candidates_[query.Next_++];
  query.Attempts_ = 0;
  query.At_ = query.Current_->Serves(query.Request_);
  if (const auto problem =
          ValidateDemand(query.Request_, query.Current_->Declaration().MaximumPayloadBytes)) {
    return Refuse(query, kRetryCapMs, *problem);
  }
  if (auto stored = ReadStored(query, transport)) { return stored; }
  if (query.Current_ == nullptr) { return std::nullopt; }
  return StartCurrent(query, transport);
}

std::optional<SourceDelivery>
SourceSet::ProcessAbsence(Query &query, SourceTransport &transport, std::optional<int> httpStatus) {
  const auto &decl = query.Current_->Declaration();
  const auto tile = query.At_.Tile();
  if (decl.TileAbsence == TileAbsencePolicy::Parent && tile && !query.Request_.Range() &&
      tile->Zoom > decl.MinZoom) {
    query.At_ = Address::At(TileId{.Zoom = tile->Zoom - 1, .X = tile->X / 2, .Y = tile->Y / 2});
    query.Attempts_ = 0;
    query.Phase_ = Query::Phase::Ready;
    if (auto stored = ReadStored(query, transport)) { return stored; }
    if (query.Current_ == nullptr) { return std::nullopt; }
    if (query.Phase_ == Query::Phase::InFlight) { return SourceDelivery::Waiting(); }
    return StartCurrent(query, transport);
  }
  if (query.Current_->Declaration().OnAbsent == AbsencePolicy::Fail) {
    return Refuse(query, kRetryCapMs, SourceFailureReason::ConfirmedAbsent, httpStatus);
  }
  query.Current_ = nullptr;
  query.Phase_ = Query::Phase::Ready;
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.HandedOver;
  return std::nullopt;
}

std::optional<SourceDelivery> SourceSet::ReadStored(Query &query, SourceTransport &transport) {
  const SourceDecl &decl = query.Current_->Declaration();
  if (decl.Keeps != Cacheability::Forever) { return std::nullopt; }
  const auto &range = query.Request_.Range();
  const size_t maximum = range ? static_cast<size_t>(range->Length) + MaximumRangeRecordOverhead
                               : decl.MaximumPayloadBytes;
  const auto cell = query.At_.GeoCell();
  auto kept =
      !range && cell ? Store_.LookupCell(decl, *cell) : Store_.Lookup(query.CacheKey(), maximum);
  if (kept.Where == RawSourceCache::Presence::Unknown) { return std::nullopt; }
  if (kept.Where == RawSourceCache::Presence::Absent) {
    {
      const std::scoped_lock lock(LedgerMutex_);
      ++Ledger_.Asked;
    }
    return ProcessAbsence(query, transport);
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
  return SourceDelivery::From(
      decl.Id, decl.Revision, query.At_, std::move(kept.Bytes), SourceKey(decl), std::move(origin));
}

SourceDelivery SourceSet::ResumeRetry(Query &query, SourceTransport &transport) {
  const double nowMs = transport.NowMs();
  if (!std::isfinite(nowMs) || nowMs < 0.0) { return Refuse(query, kRetryCapMs); }
  if (nowMs < query.RetryAtMs_) { return SourceDelivery::Waiting(); }
  query.RetryAtMs_ = 0.0;
  if (auto refused = StartCurrent(query, transport)) { return std::move(*refused); }
  return SourceDelivery::Waiting();
}

std::optional<SourceDelivery> SourceSet::StartCurrent(Query &query, SourceTransport &transport) {
  const auto &range = query.Request_.Range();
  const SourceRequest served =
      range ? SourceRequest(
                  query.Request_.Kind(), query.At_, *range, std::string(query.Request_.EntityTag()))
            : SourceRequest(query.Request_.Kind(), query.At_);
  const auto began = std::chrono::steady_clock::now();
  const auto started = query.Current_->Begin(served, transport);
  RecordProviderCall(query.Current_->Declaration().Kind, began);
  RecordStart(query.Current_->Declaration(),
              query.Phase_ == Query::Phase::Ready,
              started && *started != Ticket::None);
  if (!started) { return Refuse(query, kRetryCapMs, started.error()); }
  query.Ticket_ = *started;
  query.Phase_ = Query::Phase::InFlight;
  return std::nullopt;
}

void SourceSet::RecordProviderCall(DataKind kind, std::chrono::steady_clock::time_point began) {
  const auto at = static_cast<size_t>(kind);
  if (at >= ProviderNs_.size()) { return; }
  const auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began)
          .count();
  ProviderNs_[at].fetch_add(static_cast<uint64_t>(ns), std::memory_order_relaxed);
  ProviderCalls_[at].fetch_add(1, std::memory_order_relaxed);
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

SourceDelivery SourceSet::Deliver(Query &query, SourceResponse::Settled response) {
  const SourceDecl &decl = query.Current_->Declaration();
  if (decl.MaximumPayloadBytes != 0 && response.Bytes.size() > decl.MaximumPayloadBytes) {
    return Refuse(query, kRetryCapMs, SourceFailureReason::CapacityRefused);
  }
  if (!Matches(query.Request_, response.Range, response.Bytes.size())) {
    return Refuse(query, kRetryCapMs, SourceFailureReason::CorruptPayload);
  }
  if (decl.Keeps == Cacheability::Forever) {
    if (response.Range) {
      auto record = PackSourceRange(response.Bytes, *response.Range);
      if (!record) { return Refuse(query, kRetryCapMs, SourceFailureReason::CapacityRefused); }
      (void)Store_.Keep(query.CacheKey(), record->data(), record->size());
    } else {
      const auto cell = query.At_.GeoCell();
      if (cell) {
        (void)Store_.KeepCell(decl, *cell, response.Bytes);
      } else if (response.Bytes.empty()) {
        (void)Store_.KeepEmpty(query.CacheKey());
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
  return SourceDelivery::From(decl.Id,
                              decl.Revision,
                              query.At_,
                              std::move(response.Bytes),
                              SourceKey(decl),
                              std::move(response.Range));
}

std::optional<SourceDelivery> SourceSet::ProcessResponse(Query &query,
                                                         SourceResponse::Settled response,
                                                         double retryAfterS,
                                                         SourceTransport &transport) {
  const SourceDecl &decl = query.Current_->Declaration();
  const double retryAfterMs = retryAfterS * kMsPerS;
  const bool validDelay = std::isfinite(retryAfterMs) && retryAfterMs >= 0.0;
  switch (response.What) {
    case Meaning::Bytes: return Deliver(query, std::move(response));
    case Meaning::Absent: {
      if (response.Evidence == AbsenceEvidence::HttpNotFound &&
          decl.Keeps == Cacheability::Forever) {
        (void)Store_.KeepAbsent(query.CacheKey());
      }
      return ProcessAbsence(query, transport, response.HttpStatus);
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
        return SourceDelivery::Waiting();
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

SourceDelivery SourceSet::Refuse(Query &query,
                                 double afterMs,
                                 SourceFailureReason reason,
                                 std::optional<int> httpStatus) {
  const SourceDecl &decl = query.Current_->Declaration();
  const std::string sourceId = decl.Id;
  const std::string sourceRevision = decl.Revision;
  SourceFailure failure{.Kind = query.Request_.Kind(),
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
  return SourceDelivery::WireAfter(afterMs, sourceId, sourceRevision, std::move(failure));
}

void SourceSet::Abandon(Query &query, SourceTransport &transport) {
  if (query.Ticket_ != Ticket::None && query.Current_ != nullptr) {
    query.Current_->Cancel(query.Ticket_, transport);
  }
  query.Finish();
}

SourceSet::Ledger SourceSet::Counters() const {
  const std::scoped_lock lock(LedgerMutex_);
  auto out = Ledger_;
  out.Providers = ProviderCosts();
  return out;
}

std::array<SourceSet::Ledger::ProviderCost, SourceSet::Ledger::KindCount>
SourceSet::ProviderCosts() const {
  std::array<Ledger::ProviderCost, Ledger::KindCount> out{};
  for (size_t at = 0; at < out.size(); ++at) {
    out[at].Calls = ProviderCalls_[at].load(std::memory_order_relaxed);
    out[at].Ms = static_cast<double>(ProviderNs_[at].load(std::memory_order_relaxed)) / kNsPerMs;
  }
  return out;
}

}
