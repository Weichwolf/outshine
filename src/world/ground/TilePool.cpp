#include "Digest.h"
#include "Sha256.h"
#include "math/Units.h"
#include "TilePool.h"
#include "TerrainDelivery.h"
#include "TileGeodesy.h"
#include "CopernicusTerrain.h"
#include "math/Vec3.h"

#include <algorithm>
#include <expected>
#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <numbers>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <ratio>
#include <thread>
#include <tuple>
#include <vector>
#include <utility>

#include "Capacity.h"
#include "Delivery.h"
#include "Heap.h"
#include "Log.h"
#include "SourceSet.h"
#include "StackProbe.h"
#include "TerrainTiles.h"
#include <limits>
#include <world/data/Transport.h>

namespace outshine::Ground {

constexpr unsigned kKindShift = 62u;
constexpr uint64_t kTerrainKind = 1;
constexpr uint64_t kFieldKind = 2;
constexpr uint64_t kVectorKind = 3;
constexpr uint64_t kZoomMask = 31;
constexpr unsigned kZoomShift = 56u;
constexpr uint64_t kColumnMask = 0xFFFFFFFu;
constexpr unsigned kColumnShift = 28u;
constexpr uint64_t kVectorMask = 0x3FFFFFFFFFFFFFFFull;
constexpr double kBytesPerMB = 1024.0 * 1024.0;

namespace {

constexpr int kPollMs = 1;
constexpr int kPollAttempts = 30000;
constexpr size_t kMinimumStitchCacheBytes = size_t{32} * 1024u * 1024u;

thread_local double tFetchBlockedMs = 0.0;
thread_local bool tCarries = false;
thread_local uint64_t tAwaited = 0;
thread_local uint64_t tWorkingJob = 0;
thread_local uint64_t tWorkingAdmission = 0;

uint64_t MeshKey(int z, uint32_t x, uint32_t y) {
  return (kTerrainKind << kKindShift) |
         (static_cast<uint64_t>(static_cast<uint32_t>(z) & kZoomMask) << kZoomShift) |
         (static_cast<uint64_t>(x & kColumnMask) << kColumnShift) |
         static_cast<uint64_t>(y & kColumnMask);
}

uint64_t FieldKey(int z, uint32_t x, uint32_t y) {
  return (kFieldKind << kKindShift) | (MeshKey(z, x, y) & ~(kTerrainKind << kKindShift));
}

uint64_t RequestKey(std::string_view key) {
  uint64_t h = kDigestBasis;
  for (const char c : key) {
    h = (h ^ static_cast<uint64_t>(static_cast<uint8_t>(c))) * kDigestPrime;
  }
  return (kVectorKind << kKindShift) | (h & kVectorMask);
}

class PoolTerrain : public TerrainSource {
public:
  explicit PoolTerrain(TilePool &pool) : Pool_(pool), Native_(pool) {}

  [[nodiscard]] size_t HeapBytes() const noexcept { return Native_.HeapBytes(); }

  [[nodiscard]] uint64_t TerrainScopeRevision() const noexcept override {
    return Pool_.TerrainScopeRevision();
  }

  [[nodiscard]] bool
  AreCurrent(std::span<const TerrainRevisionIndex::Stamp> stamps) const override {
    return Pool_.ValidTerrainStamps(stamps);
  }

  [[nodiscard]] TerrainRevisionIndex::Validation
  InspectStamps(std::span<const TerrainRevisionIndex::Stamp> stamps) const override {
    return Pool_.InspectTerrainStamps(stamps);
  }

  TerrainBytes Take(Data::TileId at) override {
    if (Pool_.HasNativeTerrain()) {
      auto delivery = Native_.Take(at);
      if (Native_.Awaiting()) { tAwaited = RequestKey(Native_.Awaiting()->Key()); }
      return delivery;
    }
    const Data::Fetch request(Data::DataKind::Elevation, Data::Address::At(at));
    TilePool::Landing landing;
    const TilePool::Reply asked = Pool_.Bytes(request, &landing);
    switch (asked) {
      case TilePool::Reply::Ready: return Pool_.DecodeTerrain(request, std::move(landing));
      case TilePool::Reply::Absent:
      case TilePool::Reply::Undeclared: return TerrainBytes::Nothing();
      case TilePool::Reply::Refused: return TerrainBytes::Wire(std::move(landing.Failure));
      case TilePool::Reply::Deferred:
      case TilePool::Reply::Pending: tAwaited = RequestKey(request.Key()); break;
    }
    return TerrainBytes::Waiting();
  }

private:
  TilePool &Pool_;
  CopernicusTerrain Native_;
};

}

TerrainBytes TilePool::DecodeTerrain(const Data::Fetch &request, Landing landing) const {
  for (size_t index = 0; index < Sources_.Count(); ++index) {
    const auto &source = Sources_.At(index);
    if (Data::SourceKey(source.Declaration()) == landing.SourceKey) {
      return FromTerrainDelivery(request, std::move(landing), &source);
    }
  }
  return TerrainBytes::Wire(Data::FetchFailure{.Kind = request.Kind(),
                                               .Requested = request.Where(),
                                               .Served = landing.At,
                                               .SourceId = std::move(landing.SourceId),
                                               .SourceRevision = std::move(landing.SourceRevision),
                                               .SourceKey = std::move(landing.SourceKey),
                                               .Reason = Data::FetchFailureReason::SourceChanged});
}

struct TilePool::ComputeContext {
  PoolTerrain Source;
  TerrainTiles Tiles;

  ComputeContext(TilePool &pool, EnuFrame frame, const std::shared_ptr<DecodedCache> &decoded)
      : Source(pool), Tiles(Source, frame, TerrainTiles::Config{.Shared = decoded}) {}
};

TilePool::TilePool(const Config &config, Data::SourceSet &sources, Data::Transport &transport)
    : Sources_(sources),
      Wire_(transport),
      OriginLatDeg_(config.OriginLatDeg),
      OriginLonDeg_(config.OriginLonDeg),
      ByteBudget_(config.ByteBudget),
      Decoded_(std::make_shared<Ground::DecodedCache>(
          std::max(config.DecodedBytes, kMinimumStitchCacheBytes))),
      PollAttempts_(config.PollAttempts),
      CarrierCount_(config.Carriers),
      OutstandingMost_(config.OutstandingMost),
      Diagnostics_(config.Diagnostics),
      FocusLatDeg_(config.OriginLatDeg),
      FocusLonDeg_(config.OriginLonDeg) {
  auto revisions = TerrainRevisionIndex::Create(config.TerrainRevisionEntries);
  if (revisions) {
    TerrainRevisions_ = std::move(*revisions);
  } else {
    Log::Error(LogTag::World, "invalid_terrain_revision_capacity");
  }
  Sources_.Seal();
  for (size_t index = 0; index < Sources_.Count(); ++index) {
    const auto &decl = Sources_.At(index).Declaration();
    NativeTerrain_ = NativeTerrain_ || (decl.Kind == Data::DataKind::Elevation &&
                                        decl.How == Data::Scheme::GeographicCell &&
                                        decl.Wire == Data::WireFormat::CopernicusCog);
  }
  if (config.Compute != nullptr) {
    Compute_ = config.Compute;
  } else {
    OwnedCompute_ = std::make_unique<Tasks>(config.Threads > 0 ? config.Threads : 1);
    Compute_ = OwnedCompute_.get();
  }
  const int n = Compute_->Threads();
  ContextBytes_ = std::vector<std::atomic<size_t>>(static_cast<size_t>(n));
  Contexts_.resize(static_cast<size_t>(n));
  ComputeStates_.resize(static_cast<size_t>(n), ComputeState::Idle);
  const int carriers = CarrierCount_ > 0 ? CarrierCount_ : 2;
  Carriers_.reserve(static_cast<size_t>(carriers));
  for (int i = 0; i < carriers; i++) {
    Carriers_.emplace_back([this] {
      const LogThreadSinkScope logs(Diagnostics_);
      Carry();
    });
  }
  Log::Info(LogTag::World,
            "tilepool",
            {{"threads", n},
             {"inFlightCap", n},
             {"byteBudgetMB", static_cast<double>(ByteBudget_) / kBytesPerMB},
             {"decodedCacheBytes", static_cast<double>(Decoded_->Bytes())}});
}

TilePool::~TilePool() {
  {
    const std::scoped_lock lock(QueueMutex_);
    Stopping_ = true;
  }
  Wake_.notify_all();
  {
    std::unique_lock lock(QueueMutex_);
    Landed_.wait(lock, [this] { return ActiveCompute_ == 0; });
  }
  for (std::thread &t : Carriers_) { t.join(); }
}

void TilePool::Focus(LongitudeLatitude at) {
  const std::scoped_lock lock(QueueMutex_);
  FocusLatDeg_ = at.LatitudeDeg;
  FocusLonDeg_ = at.LongitudeDeg;
}

double TilePool::TileDistance(Data::TileId of) const {
  const double n = std::ldexp(1.0, of.Zoom);
  const double cx = (FocusLonDeg_ + kDegPerHalfTurn) / kDegPerTurn * n;
  const double lat = FocusLatDeg_ * kDeg2Rad;
  const double cy = (1.0 - std::asinh(std::tan(lat)) / std::numbers::pi) * 0.5 * n;
  const double dx = static_cast<double>(of.X) + 0.5 - cx;
  const double dy = static_cast<double>(of.Y) + 0.5 - cy;
  return dx * dx + dy * dy;
}

TilePool::Ledger TilePool::Counters() const {
  Ledger out;
  {
    const std::scoped_lock lock(LedgerMutex_);
    out = Ledger_;
  }
  out.Decoded = Decoded_->ReadCounters();
  const std::scoped_lock queue(QueueMutex_);
  out.Posts = Posts_;
  out.Repeats = Repeats_;
  out.QueueDepth = static_cast<long long>(Queue_.size());
  out.Outstanding = static_cast<long long>(Posted_.Size() - Done_.Size());
  out.Parked = static_cast<long long>(Awaiting_.Size());
  out.ParkedJobs = 0;
  Awaiting_.Visit([&out](uint64_t, const std::vector<Job> &jobs) {
    out.ParkedJobs += static_cast<long long>(jobs.size());
  });
  out.Held = static_cast<long long>(Done_.Size());
  return out;
}

size_t TilePool::TerrainMetadataBytes() const noexcept {
  return TerrainRevisions_
             ? sizeof(TerrainRevisionIndex) + TerrainRevisions_->PayloadCapacityBytes()
             : 0u;
}

bool TilePool::ValidTerrainStamps(std::span<const TerrainRevisionIndex::Stamp> stamps) const {
  return InspectTerrainStamps(stamps) == TerrainRevisionIndex::Validation::Current;
}

std::expected<TerrainRevisionIndex::Reservation, TerrainRevisionIndex::Error>
TilePool::PrepareTerrainMetadata(std::span<const Data::TileId> fields) {
  if (!TerrainRevisions_) { return std::unexpected(TerrainRevisionIndex::Error::InvalidCapacity); }
  if (fields.size() > TerrainRevisionIndex::MaximumEntries) {
    return std::unexpected(TerrainRevisionIndex::Error::InvalidCapacity);
  }
  std::vector<Data::TileId> dependencies;
  dependencies.reserve(fields.size() * 9u);
  for (const auto field : fields) {
    if (field.Zoom < 0 || field.Zoom > Data::TileId::MaximumZoom ||
        field.X >= (uint32_t{1} << static_cast<uint32_t>(field.Zoom)) ||
        field.Y >= (uint32_t{1} << static_cast<uint32_t>(field.Zoom))) {
      return std::unexpected(TerrainRevisionIndex::Error::InvalidTile);
    }
    for (long dy = -1; dy <= 1; ++dy) {
      for (long dx = -1; dx <= 1; ++dx) {
        long x = static_cast<long>(field.X) + dx;
        const long y = static_cast<long>(field.Y) + dy;
        if (!WrapTile(field.Zoom, &x, &y)) { continue; }
        dependencies.push_back(
            {.Zoom = field.Zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)});
      }
    }
  }
  const auto key = [](Data::TileId tile) { return std::tuple(tile.Zoom, tile.X, tile.Y); };
  std::ranges::sort(dependencies, {}, key);
  dependencies.erase(std::ranges::unique(dependencies).begin(), dependencies.end());
  const std::scoped_lock lock(CacheMutex_);
  return TerrainRevisions_->ReserveFor(dependencies);
}

TerrainRevisionIndex::Validation
TilePool::InspectTerrainStamps(std::span<const TerrainRevisionIndex::Stamp> stamps) const {
  const std::scoped_lock lock(CacheMutex_);
  return TerrainRevisions_ ? TerrainRevisions_->InspectStamps(stamps)
                           : TerrainRevisionIndex::Validation::Unknown;
}

bool TilePool::CertificateCurrent(const TerrainCertificate &certificate) const {
  const std::scoped_lock lock(QueueMutex_, CacheMutex_);
  return certificate.IsComplete() && certificate.TerrainScopeRevision() != 0 &&
         certificate.TerrainScopeRevision() == TerrainScopeRevision() && TerrainRevisions_ &&
         TerrainRevisions_->AreCurrent(certificate.Dependencies());
}

TerrainCertificate::Validation
TilePool::InspectCertificate(const TerrainCertificate &certificate) const {
  using Validation = TerrainCertificate::Validation;
  if (!certificate.IsComplete() || certificate.TerrainScopeRevision() == 0) {
    return Validation::Unknown;
  }
  const std::unique_lock queue(QueueMutex_, std::try_to_lock);
  if (!queue.owns_lock()) { return Validation::Pending; }
  if (certificate.TerrainScopeRevision() != TerrainScopeRevision()) {
    return Validation::ScopeChanged;
  }
  const std::unique_lock cache(CacheMutex_, std::try_to_lock);
  if (!cache.owns_lock()) { return Validation::Pending; }
  if (!TerrainRevisions_) { return Validation::Unknown; }
  const auto stamps = TerrainRevisions_->TryInspectStamps(certificate.Dependencies());
  if (!stamps) { return Validation::Pending; }
  switch (*stamps) {
    case TerrainRevisionIndex::Validation::Current: return Validation::Current;
    case TerrainRevisionIndex::Validation::Unknown: return Validation::Unknown;
    case TerrainRevisionIndex::Validation::Stale: return Validation::Stale;
  }
  std::unreachable();
}

size_t TilePool::ByteCacheBytes() const {
  const std::scoped_lock lock(CacheMutex_);
  size_t bytes = CapacityBytes(Cache_) + CapacityBytes(CacheAt_);
  for (const CacheEntry &e : Cache_) {
    bytes += e.Key.capacity() + e.SourceId.capacity() + e.SourceRevision.capacity() +
             e.SourceKey.capacity() + CapacityBytes(e.Data) +
             (e.Range ? e.Range->EntityTag.capacity() : 0u) +
             (e.Failure ? e.Failure->HeapBytes() : 0u);
  }
  return bytes;
}

std::optional<size_t> TilePool::CacheEntryOf(std::string_view key) const {
  const uint64_t digest = RequestKey(key);
  const auto first = std::ranges::lower_bound(CacheAt_, digest, {}, &CacheIndex::Digest);
  for (auto found = first; found != CacheAt_.end() && found->Digest == digest; ++found) {
    if (Cache_[found->Entry].Key == key) { return found->Entry; }
  }
  return std::nullopt;
}

void TilePool::IndexCacheEntry(std::string_view key, size_t entry) {
  const uint64_t digest = RequestKey(key);
  const auto at = std::ranges::lower_bound(CacheAt_, digest, {}, &CacheIndex::Digest);
  CacheAt_.insert(at, {.Digest = digest, .Entry = entry});
}

void TilePool::EraseCacheIndex(std::string_view key, size_t entry) {
  const uint64_t digest = RequestKey(key);
  const auto first = std::ranges::lower_bound(CacheAt_, digest, {}, &CacheIndex::Digest);
  const auto found =
      std::ranges::find_if(first, CacheAt_.end(), [digest, entry](const CacheIndex &indexed) {
        return indexed.Digest == digest && indexed.Entry == entry;
      });
  assert(found != CacheAt_.end());
  CacheAt_.erase(found);
}

void TilePool::RemoveCacheEntry(size_t entry) {
  CacheBytes_ -= Cache_[entry].Data.size();
  EraseCacheIndex(Cache_[entry].Key, entry);
  const size_t last = Cache_.size() - 1u;
  if (entry != last) {
    Cache_[entry] = std::move(Cache_[last]);
    RepointCacheIndex({.From = last, .To = entry});
  }
  Cache_.pop_back();
}

void TilePool::RepointCacheIndex(CacheEntryMove move) noexcept {
  const auto found = std::ranges::find_if(
      CacheAt_, [move](const CacheIndex &indexed) { return indexed.Entry == move.From; });
  assert(found != CacheAt_.end());
  found->Entry = move.To;
}

size_t TilePool::DemCacheBytes() const {
  size_t bytes = Decoded_->Bytes() + CapacityBytes(ContextBytes_);
  for (const std::atomic<size_t> &slot : ContextBytes_) {
    bytes += slot.load(std::memory_order_relaxed);
  }
  return bytes;
}

size_t TilePool::ResidentBytes() const {
  return ByteCacheBytes() + DemCacheBytes() + SchedulerBytes() + TerrainMetadataBytes();
}

size_t TilePool::SchedulerBytes() const {
  const std::scoped_lock lock(QueueMutex_);
  const auto jobBytes = [](const std::vector<Job> &jobs) {
    size_t bytes = CapacityBytes(jobs);
    for (const Job &job : jobs) { bytes += job.Ask ? job.Ask->Key().capacity() : 0u; }
    return bytes;
  };
  size_t bytes = jobBytes(Queue_) + jobBytes(Carrying_) + CapacityBytes(Contexts_) +
                 CapacityBytes(ComputeStates_);
  bytes += Posted_.HeapBytes() + Done_.HeapBytes() + Awaiting_.HeapBytes();
  Done_.Visit([&bytes](uint64_t, const Result &result) {
    bytes += CapacityBytes(result.Build.Nodes) + CapacityBytes(result.Build.Sources) +
             CapacityBytes(result.Landed.Bytes) + (result.Field ? result.Field->HeapBytes() : 0u) +
             (result.Landed.Failure ? result.Landed.Failure->HeapBytes() : 0u) +
             result.Landed.SourceId.capacity() + result.Landed.SourceRevision.capacity() +
             result.Landed.SourceKey.capacity() +
             (result.Landed.Range ? result.Landed.Range->EntityTag.capacity() : 0u);
    for (const auto &source : result.Build.Sources) {
      bytes += source.SourceId.capacity() + source.Revision.capacity();
    }
  });
  Awaiting_.Visit(
      [&bytes, &jobBytes](uint64_t, const std::vector<Job> &jobs) { bytes += jobBytes(jobs); });
  return bytes;
}

void TilePool::RefuseUntil(const std::string &key,
                           double untilMs,
                           const std::optional<Data::FetchFailure> &failure) {
  const std::scoped_lock lock(CacheMutex_);
  if (const std::optional<size_t> found = CacheEntryOf(key)) {
    Cache_[*found].RefusedUntilMs = untilMs;
    Cache_[*found].Failure = failure;
    return;
  }
  CacheEntry made;
  made.Key = key;
  made.RefusedUntilMs = untilMs;
  made.Failure = failure;
  made.Used = ++CacheClock_;
  Cache_.push_back(std::move(made));
  IndexCacheEntry(key, Cache_.size() - 1u);
}

TilePool::Reply TilePool::ReadCachedDelivery(const std::string &key, Landing *out) {
  out->Failure.reset();
  out->TerrainStamp.reset();
  out->Range.reset();
  const std::scoped_lock lock(CacheMutex_);
  const std::optional<size_t> found = CacheEntryOf(key);
  if (!found) { return Reply::Pending; }
  CacheEntry &e = Cache_[*found];
  e.Used = ++CacheClock_;
  if (e.RefusedUntilMs > 0.0 && e.RefusedUntilMs > Wire_.NowMs()) {
    out->Failure = e.Failure;
    return Reply::Refused;
  }
  if (e.Absent) { return Reply::Absent; }
  if (e.Data.empty() && e.RefusedUntilMs > 0.0) { return Reply::Pending; }
  if (TerrainRevisions_ && e.TerrainStamp) {
    auto current = TerrainRevisions_->CurrentStamp(e.TerrainStamp->Requested);
    if (current && *current != *e.TerrainStamp) { return Reply::Pending; }
    if (!current) {
      auto observed = TerrainRevisions_->RestoreCachedStamp(*e.TerrainStamp);
      if (observed) { current = std::move(*observed); }
    }
    e.TerrainStamp = std::move(current);
    out->TerrainStamp = e.TerrainStamp;
  }
  out->Bytes.assign(e.Data.begin(), e.Data.end());
  out->SourceId = e.SourceId;
  out->SourceRevision = e.SourceRevision;
  out->SourceKey = e.SourceKey;
  out->At = e.At;
  out->Range = e.Range;
  return Reply::Ready;
}

std::optional<TerrainRevisionIndex::Stamp>
TilePool::PublishDelivery(const Data::Fetch &request, const Landing &landing, bool absent) {
  const std::string key = request.Key();
  std::optional<TerrainRevisionIndex::DeliveryFingerprint> fingerprint;
  if (request.Kind() == Data::DataKind::Elevation) {
    std::string identity;
    const auto append = [&identity](std::string_view value) {
      identity += std::to_string(value.size()) + ":";
      identity += value;
    };
    append(key);
    append(landing.SourceId);
    append(landing.SourceRevision);
    append(landing.SourceKey);
    append(landing.At.Text());
    append(absent ? "absent" : "present");
    append(Sha256Hex(landing.Bytes.data(), landing.Bytes.size()));
    const std::string digest = Sha256Hex(identity);
    fingerprint.emplace();
    std::ranges::copy(digest, fingerprint->begin());
  }
  const std::scoped_lock lock(CacheMutex_);
  const size_t len = absent ? 0u : landing.Bytes.size();
  std::optional<TerrainRevisionIndex::Stamp> stamp;
  if (TerrainRevisions_ && request.Kind() == Data::DataKind::Elevation) {
    if (const auto tile = request.Where().Tile()) {
      auto observed = TerrainRevisions_->IssueDeliveryStamp(*tile, fingerprint);
      if (observed) { stamp = std::move(*observed); }
    }
  }
  if (const auto found = CacheEntryOf(key)) { RemoveCacheEntry(*found); }
  long evicted = 0;
  while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_) {
    size_t victim = 0;
    for (size_t i = 1; i < Cache_.size(); i++) {
      if (Cache_[i].Used < Cache_[victim].Used) { victim = i; }
    }
    RemoveCacheEntry(victim);
    evicted++;
  }
  if (evicted > 0) {
    const std::scoped_lock ledger(LedgerMutex_);
    Ledger_.Evictions += evicted;
  }
  CacheEntry e;
  e.Key = key;
  e.At = landing.At;
  e.SourceId = landing.SourceId;
  e.SourceRevision = landing.SourceRevision;
  e.SourceKey = landing.SourceKey;
  e.Range = landing.Range;
  e.Absent = absent;
  e.TerrainStamp = stamp;
  e.Used = ++CacheClock_;
  if (!absent && len > 0) { e.Data = landing.Bytes; }
  CacheBytes_ += e.Data.size();
  Cache_.push_back(std::move(e));
  IndexCacheEntry(key, Cache_.size() - 1u);
  return stamp;
}

TilePool::Reply TilePool::FetchDelivery(const Data::Fetch &request, Landing *out) {
  out->Failure.reset();
  out->TerrainStamp.reset();
  out->Range.reset();
  if (!tCarries) {
    const std::scoped_lock ledger(LedgerMutex_);
    Ledger_.FetchOnCompute++;
  }
  const std::string key = request.Key();
  Data::SourceSet::Query query = Sources_.Ask(request);
  Reply reply = Reply::Pending;
  const auto entered = std::chrono::steady_clock::now();
  double pollMs = 0.0;
  const int attempts = PollAttempts_ > 0 ? PollAttempts_ : kPollAttempts;
  for (int attempt = 0; attempt < attempts && reply == Reply::Pending; attempt++) {
    {
      const std::scoped_lock lock(QueueMutex_);
      if (Stopping_) { break; }
    }
    const auto t0 = std::chrono::steady_clock::now();
    Data::Delivery answer = Sources_.Collect(query, Wire_);
    pollMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (std::optional<Data::Delivery::Answer> taken = answer.Take()) {
      out->Bytes = std::move(taken->Bytes);
      out->SourceId = std::move(taken->SourceId);
      out->SourceRevision = std::move(taken->SourceRevision);
      out->SourceKey = std::move(taken->SourceKey);
      out->At = taken->At;
      out->Range = std::move(taken->Range);
      out->TerrainStamp = PublishDelivery(request, *out, false);
      reply = Reply::Ready;
      break;
    }
    switch (answer.Where()) {
      case Data::Delivery::State::Pending: (void)Wire_.Await(static_cast<double>(kPollMs)); break;
      case Data::Delivery::State::Vacant:

        out->At = request.Where();
        out->TerrainStamp = PublishDelivery(request, *out, true);
        reply = Reply::Absent;
        break;
      case Data::Delivery::State::Undeclared:

        Log::Error(LogTag::World, "tile_undeclared", {{"request", key}});
        reply = Reply::Undeclared;
        break;
      case Data::Delivery::State::Consumed:
      case Data::Delivery::State::Refused:
        out->Failure = answer.Failure();
        RefuseUntil(key, Wire_.NowMs() + answer.AfterMs(), out->Failure);
        Log::Error(LogTag::World,
                   "tile_refused",
                   {{"request", key},
                    {"source", answer.SourceId()},
                    {"revision", answer.SourceRevision()},
                    {"reason",
                     answer.Failure() ? std::string(Data::Name(answer.Failure()->Reason))
                                      : "unknown failure"}});
        reply = Reply::Refused;
        break;
      case Data::Delivery::State::Delivered: break;
    }
  }
  if (reply == Reply::Pending) { outshine::Data::SourceSet::Abandon(query, Wire_); }
  const double blockedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entered).count();
  tFetchBlockedMs += blockedMs;
  {
    const std::scoped_lock ledger(LedgerMutex_);
    Ledger_.Fetches++;
    Ledger_.FetchMs += pollMs;
    Ledger_.FetchBlockedMs += blockedMs;
    if (reply == Reply::Ready) {
      Ledger_.FetchedMB += static_cast<double>(out->Bytes.size()) / kBytesPerMB;
    }
    if (reply == Reply::Absent) { Ledger_.FetchAbsent++; }

    if (reply == Reply::Refused || reply == Reply::Undeclared) { Ledger_.FetchRefused++; }
    if (reply == Reply::Pending) { Ledger_.FetchGaveUp++; }
  }
  if (reply != Reply::Ready) { out->Bytes.clear(); }
  return reply;
}

TilePool::Reply TilePool::Bytes(const Data::Fetch &request, Landing *out) {
  const std::string key = request.Key();
  const Reply resident = ReadCachedDelivery(key, out);
  if (resident != Reply::Pending) { return resident; }
  Job job;
  job.Kind = Rank::Fetch;
  job.Key = RequestKey(key);
  job.Ask = request;
  Result result;
  const Reply posted = Poll(job, &result);
  if (posted == Reply::Ready || posted == Reply::Refused) { *out = std::move(result.Landed); }
  return posted;
}

TilePool::Reply TilePool::BytesBlocking(const Data::Fetch &request, Landing *out) {
  const Reply resident = ReadCachedDelivery(request.Key(), out);
  if (resident != Reply::Pending) { return resident; }
  return FetchDelivery(request, out);
}

namespace {

enum class Miss { None, Hole, Wait, Refused };

[[nodiscard]] Miss MissOf(TerrainGrid::State state) {
  switch (state) {
    case TerrainGrid::State::Decoded: return Miss::None;
    case TerrainGrid::State::NotHere: return Miss::Hole;
    case TerrainGrid::State::Deferred: return Miss::Wait;
    case TerrainGrid::State::Undecodable:
    case TerrainGrid::State::Refused: return Miss::Refused;
  }
  return Miss::Refused;
}

}

void TilePool::RunMesh(TerrainTiles &tiles, const Job &job, Result *out) {
  const TerrainGrid::State state = tiles.SampleNodeHeights({.Zoom = job.Z, .X = job.X, .Y = job.Y},
                                                           job.Grid,
                                                           &out->Build.Nodes,
                                                           &out->Build.Sources,
                                                           &out->Build.Postings,
                                                           &out->Build.Side,
                                                           &out->Landed.Failure);
  Miss miss = MissOf(state);
  const char *stage = "source";
  if (miss == Miss::None && (out->Build.Side < 2 || out->Build.Nodes.empty())) {
    miss = Miss::Refused;
    stage = "grid";
  }
  if (miss == Miss::None) {
    out->State = Reply::Ready;
    return;
  }
  if (miss == Miss::Refused) {
    Log::Warn(
        LogTag::World,
        "tile_mesh_refused",
        {{"z", job.Z},
         {"x", static_cast<int>(job.X)},
         {"y", static_cast<int>(job.Y)},
         {"stage", stage},
         {"rc", static_cast<int>(state)},
         {"reason",
          out->Landed.Failure ? std::string(Data::Name(out->Landed.Failure->Reason))
                              : "invalid grid"},
         {"request", out->Landed.Failure ? out->Landed.Failure->Requested.Text() : std::string{}},
         {"source", out->Landed.Failure ? out->Landed.Failure->SourceId : std::string{}}});
    const std::scoped_lock ledger(LedgerMutex_);
    Ledger_.MeshRefused++;
  }

  switch (miss) {
    case Miss::Hole: out->State = Reply::Absent; break;
    case Miss::Refused: out->State = Reply::Refused; break;
    case Miss::Wait:
    case Miss::None: out->State = Reply::Pending; break;
  }
}

void TilePool::Acknowledge(Result &result) {
  if (result.Delivered == Delivery::Taken) { return; }
  assert(UnclaimedResults_ > 0);
  --UnclaimedResults_;
  result.Delivered = Delivery::Taken;
}

void TilePool::EraseDone(uint64_t key) {
  if (Result *result = Done_.Find(key)) { Acknowledge(*result); }
  Done_.Erase(key);
}

bool TilePool::StoresDone(uint64_t key, Result result) {
  result.Delivered = Delivery::Pending;
  if (Result *const replaced = Done_.Find(key)) {
    Acknowledge(*replaced);
    *replaced = std::move(result);
  } else if (!Done_.Emplace(key, std::move(result))) {
    return false;
  }
  ++UnclaimedResults_;
  ++CompletionRevision_;
  return true;
}

TilePool::Reply TilePool::PublishesCarried(const Job &job, Result result) {
  const Reply said = result.State;
  if (!OwnsReservation(job)) { return said; }
  result.Admission = job.Admission;
  result.TerrainScope = job.TerrainScope;
  if (said == Reply::Pending) {
    ReleaseReservation({.Key = job.Key, .Admission = job.Admission});
  } else {
    if (StoresDone(job.Key, std::move(result))) {
      Lands(job.Key, false);
    } else {
      ReleaseReservation({.Key = job.Key, .Admission = job.Admission});
    }
  }
  return said;
}

void TilePool::Carry() {
  static const Heap::Tag kCarryingTag("tile-carrier");
  const Heap::Tagged carrying(kCarryingTag);
  StackProbe::Enter(StackProbe::Purpose::Tile);
  tCarries = true;
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(QueueMutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Carrying_.empty(); });
      if (Stopping_) { break; }
      job = Carrying_.front();
      Carrying_.erase(Carrying_.begin());
    }
    Result result;
    const double blockedBefore = tFetchBlockedMs;
    const auto t0 = std::chrono::steady_clock::now();
    result.State = job.Ask ? FetchDelivery(*job.Ask, &result.Landed) : Reply::Refused;
    const double spanMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    {
      const std::scoped_lock ledger(LedgerMutex_);
      Ledger_.FetchMs += spanMs;
      Ledger_.FetchBlockedMs += tFetchBlockedMs - blockedBefore;
    }
    {
      const std::scoped_lock lock(QueueMutex_);
      const Reply said = PublishesCarried(job, std::move(result));
      ResumeDependants(job.Key, said != Reply::Pending);
      ScheduleComputeLocked();
      Landed_.notify_all();
      Wake_.notify_all();
    }
  }
}

std::optional<TilePool::Job> TilePool::NextJob(int slot) {
  const std::scoped_lock lock(QueueMutex_);
  ComputeStates_[static_cast<size_t>(slot)] = ComputeState::Working;
  if (Stopping_ || Queue_.empty()) { return std::nullopt; }
  size_t best = 0;
  for (size_t i = 1; i < Queue_.size(); i++) {
    const Job &a = Queue_[i];
    const Job &b = Queue_[best];
    if (std::tuple(a.Kind, -a.Z, a.TileDist, a.Y, a.X) <
        std::tuple(b.Kind, -b.Z, b.TileDist, b.Y, b.X)) {
      best = i;
    }
  }
  Job job = Queue_[best];
  Queue_.erase(Queue_.begin() + static_cast<long>(best));
  return job;
}

TilePool::Result TilePool::RunJob(TerrainTiles &tiles, const Job &job) {
  Result result;
  if (job.Kind != Rank::Fetch) {
    ShapedGround told;
    {
      const std::scoped_lock lock(QueueMutex_);
      if (!IsCurrentJob(job)) { return result; }
      told = Shape_;
    }
    TerrainTiles::Shaped how;
    how.Kind = told.Kind;
    how.AmplitudeM = told.AmplitudeM;
    how.WavelengthM = told.WavelengthM;
    how.Gradient = told.Gradient;
    how.BearingDeg = told.BearingDeg;
    how.FocusLatDeg = told.FocusLatDeg;
    how.FocusLonDeg = told.FocusLonDeg;
    how.Seed = told.Seed;
    tiles.Shapes(how);
  }
  switch (job.Kind) {
    case Rank::Mesh:
      RunMesh(tiles, job, &result);
      result.Holds = true;
      break;
    case Rank::Field:
      RunField(tiles, job, &result);
      result.Holds = false;
      break;
    case Rank::Fetch:
      result.State = job.Ask ? FetchDelivery(*job.Ask, &result.Landed) : Reply::Refused;
      break;
  }
  return result;
}

void TilePool::PublishResult(const Job &job, Result result) {
  {
    const std::scoped_lock lock(QueueMutex_);

    if (!IsCurrentJob(job)) {
      DiscardJob(job);
      Landed_.notify_all();
      return;
    }
    result.Admission = job.Admission;
    result.TerrainScope = job.TerrainScope;

    if (result.State == Reply::Pending) {
      ReleaseReservation({.Key = job.Key, .Admission = job.Admission});
      {
        const std::scoped_lock ledger(LedgerMutex_);
        if (job.Kind == Rank::Mesh) { Ledger_.MeshDropped++; }
        if (job.Kind == Rank::Field) { Ledger_.FieldDropped++; }
      }
      Landed_.notify_all();
    }

    else {
      if (result.State == Reply::Absent || result.State == Reply::Undeclared) {
        result.Build = TileBuild{};
      }
      const bool holds = result.Holds;
      if (StoresDone(job.Key, std::move(result))) {
        Lands(job.Key, holds);
      } else {
        ReleaseReservation({.Key = job.Key, .Admission = job.Admission});
      }
      Landed_.notify_all();
    }
  }
}

bool TilePool::AwaitDependency(const Job &job, uint64_t dependency) {
  std::unique_lock<std::mutex> lock(QueueMutex_);
  if (!IsCurrentJob(job)) {
    DiscardJob(job);
    Landed_.notify_all();
    return true;
  }
  if (Done_.Holds(dependency)) {
    Queue_.push_back(job);
    lock.unlock();
    ScheduleCompute();
    Wake_.notify_all();
    return true;
  }
  if (!Posted_.Holds(dependency)) { return false; }
  const auto parked = Awaiting_.Emplace(dependency, {});
  if (!parked) {
    ReleaseReservation({.Key = job.Key, .Admission = job.Admission});
    Landed_.notify_all();
    return true;
  }
  parked->first->push_back(job);
  Posted_.Find(job.Key)->Parked = true;
  ++CurrentParkedJobs_;
  ++ParkedJobs_;
  return true;
}

void TilePool::ScheduleCompute() {
  const std::scoped_lock lock(QueueMutex_);
  ScheduleComputeLocked();
}

void TilePool::ScheduleComputeLocked() {
  if (Stopping_) { return; }
  const auto queued = static_cast<size_t>(std::ranges::count(ComputeStates_, ComputeState::Queued));
  size_t waiting = Queue_.size() > queued ? Queue_.size() - queued : 0;
  for (size_t slot = 0; slot < ComputeStates_.size() && waiting != 0; ++slot) {
    if (ComputeStates_[slot] != ComputeState::Idle) { continue; }
    ComputeStates_[slot] = ComputeState::Queued;
    ++ActiveCompute_;
    --waiting;
    if (!Compute_->PostDetached([this, slot] {
          {
            const LogThreadSinkScope logs(Diagnostics_);
            Work(static_cast<int>(slot));
          }
          FinishCompute(static_cast<int>(slot));
        })) {
      ComputeStates_[slot] = ComputeState::Idle;
      --ActiveCompute_;
    }
  }
}

void TilePool::FinishCompute(int slot) {
  const std::scoped_lock lock(QueueMutex_);
  ComputeStates_[static_cast<size_t>(slot)] = ComputeState::Idle;
  --ActiveCompute_;
  ScheduleComputeLocked();
  Landed_.notify_all();
}

void TilePool::Work(int slot) {
  static const Heap::Tag kWorkingTag("tile-worker");
  const Heap::Tagged working(kWorkingTag);
  StackProbe::Enter(StackProbe::Purpose::Tile);
  const EnuFrame frame =
      EnuFrame::At(Geo{.LongitudeDeg = OriginLonDeg_, .LatitudeDeg = OriginLatDeg_});

  if (frame.Where() != EnuFrame::State::Usable) {
    Log::Error(LogTag::World, "tilepool_origin_too_polar", {{"lat", OriginLatDeg_}});
    std::abort();
  }
  auto &context = Contexts_[static_cast<size_t>(slot)];
  if (!context) { context = std::make_unique<ComputeContext>(*this, frame, Decoded_); }
  TerrainTiles &tiles = context->Tiles;
  const auto retainBytes = [&] {
    ContextBytes_[static_cast<size_t>(slot)].store(sizeof(ComputeContext) - sizeof(TerrainTiles) +
                                                       tiles.HeapBytes() +
                                                       context->Source.HeapBytes(),
                                                   std::memory_order_relaxed);
  };
  retainBytes();
  const auto next = NextJob(slot);
  if (!next) { return; }
  const Job &job = *next;
  Result result;
  const double blockedBefore = tFetchBlockedMs;
  const auto t0 = std::chrono::steady_clock::now();
  tAwaited = 0;
  tWorkingJob = job.Key;
  tWorkingAdmission = job.Admission;
  result = RunJob(tiles, job);
  retainBytes();
  tWorkingJob = 0;
  tWorkingAdmission = 0;
  if (result.State == Reply::Pending && tAwaited != 0) {
    const uint64_t awaited = tAwaited;
    tAwaited = 0;
    if (AwaitDependency(job, awaited)) { return; }
  }
  tAwaited = 0;
  const double spanMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

  const double cpuMs = spanMs - (tFetchBlockedMs - blockedBefore);
  {
    const std::scoped_lock ledger(LedgerMutex_);
    if (job.Kind == Rank::Mesh) {
      Ledger_.MeshTiles++;
      Ledger_.MeshCpuMs += cpuMs;
      if (result.State == Reply::Absent) { Ledger_.MeshAbsent++; }
    } else if (job.Kind == Rank::Field) {
      Ledger_.FieldTiles++;
      Ledger_.FieldCpuMs += cpuMs;
    }
  }
  StackProbe::Mark();
  PublishResult(job, std::move(result));
}

void TilePool::Lands(uint64_t key, bool holds) {
  RecentKeys<1024> &kept = holds ? Kept_ : Passing_;
  const std::optional<uint64_t> oldest = kept.Push(key);
  if (!oldest || kept.Holds(*oldest)) { return; }
  if (const Result *done = Done_.Find(*oldest)) {
    ReleaseReservation({.Key = *oldest, .Admission = done->Admission});
    EraseDone(*oldest);
  }
}

void TilePool::DeferredAdmission() {
  const std::scoped_lock lock(LedgerMutex_);
  ++Ledger_.AdmissionDeferred;
}

bool TilePool::OwnsReservation(const Job &job) const noexcept {
  const Reservation *held = Posted_.Find(job.Key);
  return held != nullptr && held->Admission == job.Admission;
}

bool TilePool::IsCurrentJob(const Job &job) const noexcept {
  return OwnsReservation(job) &&
         (job.Kind == Rank::Fetch || job.TerrainScope == TerrainScopeRevision());
}

void TilePool::ReleaseReservation(ReservationOwner owner) {
  const Reservation *held = Posted_.Find(owner.Key);
  if (held == nullptr || held->Admission != owner.Admission) { return; }
  if (held->Parked) { --CurrentParkedJobs_; }
  Posted_.Erase(owner.Key);
}

void TilePool::DiscardJob(const Job &job) {
  ReleaseReservation({.Key = job.Key, .Admission = job.Admission});
  const std::scoped_lock ledger(LedgerMutex_);
  if (job.Kind == Rank::Mesh) { ++Ledger_.MeshDropped; }
  if (job.Kind == Rank::Field) { ++Ledger_.FieldDropped; }
}

void TilePool::ResumeDependants(uint64_t key, bool resume) {
  const std::vector<Job> *parked = Awaiting_.Find(key);
  if (parked == nullptr) { return; }
  ParkedJobs_ -= parked->size();
  for (const Job &held : *parked) {
    if (!IsCurrentJob(held)) {
      DiscardJob(held);
      continue;
    }
    if (!resume) {
      ReleaseReservation({.Key = held.Key, .Admission = held.Admission});
      continue;
    }
    Posted_.Find(held.Key)->Parked = false;
    --CurrentParkedJobs_;
    Queue_.push_back(held);
  }
  Awaiting_.Erase(key);
}

std::optional<TilePool::Reply> TilePool::TakeCompleted(const Job &job, Result *out) {
  Result *done = Done_.Find(job.Key);
  if (done == nullptr) { return std::nullopt; }
  const uint64_t scope = job.Kind == Rank::Fetch ? 0 : TerrainScopeRevision();
  const Reservation *owner = Posted_.Find(job.Key);
  if (done->TerrainScope != scope || owner == nullptr || owner->Admission != done->Admission) {
    ReleaseReservation({.Key = job.Key, .Admission = done->Admission});
    EraseDone(job.Key);
    return std::nullopt;
  }
  Acknowledge(*done);
  if (done->State == Reply::Absent || done->State == Reply::Undeclared) {
    out->State = done->State;
    return out->State;
  }
  if (!done->Holds) {
    *out = std::move(*done);
    ReleaseReservation({.Key = job.Key, .Admission = out->Admission});
    EraseDone(job.Key);
  } else {
    *out = *done;
  }
  ResumeDependants(job.Key, true);
  return out->State;
}

TilePool::Reply TilePool::Poll(const Job &job, Result *out) {
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const uint64_t scope = job.Kind == Rank::Fetch ? 0 : TerrainScopeRevision();
  if (const std::optional<Reply> completed = TakeCompleted(job, out)) {
    lock.unlock();
    ScheduleCompute();
    Wake_.notify_all();
    return *completed;
  }
  if (const Reservation *held = Posted_.Find(job.Key)) {
    if (held->TerrainScope == scope) {
      ++Repeats_;
      return Reply::Pending;
    }
    ReleaseReservation({.Key = job.Key, .Admission = held->Admission});
  }
  const size_t active = Posted_.Size() - Done_.Size() - CurrentParkedJobs_;
  const size_t waiting = Queue_.size() + Carrying_.size() + ParkedJobs_;
  const Reservation *working = Posted_.Find(tWorkingJob);
  const bool dependency =
      job.Kind == Rank::Fetch && working != nullptr && working->Admission == tWorkingAdmission;
  if (((active >= OutstandingMost_ || waiting >= OutstandingMost_) && !dependency) ||
      AdmissionClock_ == std::numeric_limits<uint64_t>::max()) {
    lock.unlock();
    DeferredAdmission();
    return Reply::Deferred;
  }
  const uint64_t admission = ++AdmissionClock_;
  const auto posted =
      Posted_.Emplace(job.Key, Reservation{.Admission = admission, .TerrainScope = scope});
  if (!posted) {
    lock.unlock();
    DeferredAdmission();
    return Reply::Deferred;
  }
  if (!posted->second) {
    ++Repeats_;
    return Reply::Pending;
  }
  ++Posts_;
  Job posting = job;
  posting.Admission = admission;
  posting.TerrainScope = scope;
  if (posting.Kind == Rank::Mesh || posting.Kind == Rank::Field) {
    Data::TileId locality{.Zoom = posting.Z, .X = posting.X, .Y = posting.Y};
    if (posting.Kind == Rank::Field && locality.Zoom > 0) {
      locality = {.Zoom = locality.Zoom - 1, .X = locality.X >> 1u, .Y = locality.Y >> 1u};
    }
    posting.TileDist = TileDistance(locality);
  }
  const bool carries = posting.Kind == Rank::Fetch;
  (carries ? Carrying_ : Queue_).push_back(posting);
  lock.unlock();
  ScheduleCompute();
  Wake_.notify_all();
  return Reply::Pending;
}

TilePool::Reply TilePool::Wants(Data::TileId of, int grid) {
  const int z = of.Zoom;
  const uint32_t x = of.X;
  const uint32_t y = of.Y;
  const uint64_t key = MeshKey(z, x, y);
  {
    const std::scoped_lock lock(QueueMutex_);
    if (const Result *done = Done_.Find(key);
        done != nullptr && done->TerrainScope == TerrainScopeRevision() &&
        Posted_.Find(key) != nullptr && Posted_.Find(key)->Admission == done->Admission) {
      return done->State;
    }
    if (const Reservation *held = Posted_.Find(key);
        held != nullptr && held->TerrainScope == TerrainScopeRevision()) {
      return Reply::Pending;
    }
  }
  Job job;
  job.Kind = Rank::Mesh;
  job.Z = z;
  job.X = x;
  job.Y = y;
  job.Grid = grid;
  job.Key = key;
  Result result;
  return Poll(job, &result);
}

ShapedGround TilePool::Shaped() const {
  const std::scoped_lock lock(QueueMutex_);
  return Shape_;
}

void TilePool::Shapes(const ShapedGround &how) {
  const std::scoped_lock lock(QueueMutex_);
  if (Shape_ == how) { return; }
  Shape_ = how;
  TerrainScopeRevision_.fetch_add(1, std::memory_order_release);
}

TilePool::Reply TilePool::Mesh(Data::TileId of, int grid, TileBuild *out) {
  const int z = of.Zoom;
  const uint32_t x = of.X;
  const uint32_t y = of.Y;
  Job job;
  job.Kind = Rank::Mesh;
  job.Z = z;
  job.X = x;
  job.Y = y;
  job.Grid = grid;
  job.Key = MeshKey(z, x, y);
  Result result;
  const Reply state = Poll(job, &result);
  if (state == Reply::Ready) { *out = std::move(result.Build); }
  return state;
}

void TilePool::RunField(TerrainTiles &tiles, const Job &job, Result *out) {
  TerrainGrid grid = tiles.StitchedGrid(job.Z, job.X, job.Y);
  TerrainField *field = grid.TryFieldMutable();
  out->Landed.Failure = grid.Failure();
  const Miss miss = MissOf(grid.Where());
  if (miss == Miss::None && field != nullptr) {
    out->Field = std::make_shared<const TerrainField>(std::move(*field));
    out->State = Reply::Ready;
    return;
  }
  switch (miss) {
    case Miss::Hole: out->State = Reply::Absent; break;
    case Miss::Refused: out->State = Reply::Refused; break;
    case Miss::Wait:
    case Miss::None: out->State = Reply::Pending; break;
  }
}

TilePool::Reply TilePool::Field(Data::TileId of,
                                std::shared_ptr<const TerrainField> *out,
                                std::optional<Data::FetchFailure> *failure) {
  Job job;
  job.Kind = Rank::Field;
  job.Z = of.Zoom;
  job.X = of.X;
  job.Y = of.Y;
  job.Key = FieldKey(of.Zoom, of.X, of.Y);
  Result result;
  const Reply state = Poll(job, &result);
  if (failure != nullptr) { *failure = std::move(result.Landed.Failure); }
  if (state == Reply::Ready) { *out = std::move(result.Field); }
  return state;
}

TilePool::Reply TilePool::MeshAwaited(Data::TileId of, int grid, TileBuild *out) {
  const Reply asked = Mesh(of, grid, out);
  if (asked != Reply::Pending) { return asked; }
  const uint64_t key = MeshKey(of.Zoom, of.X, of.Y);
  {
    std::unique_lock<std::mutex> lock(QueueMutex_);
    Landed_.wait(lock, [&] { return Done_.Holds(key) || !Posted_.Holds(key); });
  }
  return Mesh(of, grid, out);
}

bool TilePool::AwaitLanding(double seconds) {
  if (seconds <= 0.0) { return false; }
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const bool ready = Landed_.wait_for(lock, std::chrono::duration<double>(seconds), [this] {
    return Stopping_ || UnclaimedResults_ != 0;
  });
  return ready && UnclaimedResults_ != 0;
}

bool TilePool::AwaitLanding(double seconds, LandingCursor &cursor) {
  if (seconds <= 0.0) { return false; }
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const bool ready =
      Landed_.wait_for(lock, std::chrono::duration<double>(seconds), [this, &cursor] {
        return Stopping_ || cursor.Revision != CompletionRevision_;
      });
  if (!ready || cursor.Revision == CompletionRevision_) { return false; }
  cursor.Revision = CompletionRevision_;
  return true;
}

void TilePool::ForgetMesh(int z, uint32_t x, uint32_t y) {
  const uint64_t key = MeshKey(z, x, y);
  const std::scoped_lock lock(QueueMutex_);
  if (const Reservation *held = Posted_.Find(key)) {
    ReleaseReservation({.Key = key, .Admission = held->Admission});
  }
  EraseDone(key);
}

bool TilePool::Known(uint64_t key) {
  const std::scoped_lock lock(QueueMutex_);
  return Done_.Holds(key) || Posted_.Holds(key);
}
}
