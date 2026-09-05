#include "Digest.h"
#include "math/Units.h"
#include "TilePool.h"
#include "math/Vec3.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <numbers>
#include <chrono>
#include <deque>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <ratio>
#include <thread>
#include <vector>
#include <utility>

#include "Capacity.h"
#include "Delivery.h"
#include "Heap.h"
#include "Log.h"
#include "SourceSet.h"
#include "StackProbe.h"
#include "TerrainTiles.h"
#include "Transport.h"

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

thread_local double tFetchBlockedMs = 0.0;
thread_local bool tCarries = false;
constexpr size_t kMostKept = 1024;
constexpr size_t kMostPassing = 1024;

thread_local uint64_t tAwaited = 0;

uint64_t MeshKey(int z, uint32_t x, uint32_t y) {
  return (kTerrainKind << kKindShift) |
         (static_cast<uint64_t>(static_cast<uint32_t>(z) & kZoomMask) << kZoomShift) |
         (static_cast<uint64_t>(x & kColumnMask) << kColumnShift) |
         static_cast<uint64_t>(y & kColumnMask);
}

uint64_t FieldKey(int z, uint32_t x, uint32_t y) {
  return (kFieldKind << kKindShift) | (MeshKey(z, x, y) & ~(kTerrainKind << kKindShift));
}

uint64_t RequestKey(const std::string &key) {
  uint64_t h = kDigestBasis;
  for (const char c : key) {
    h = (h ^ static_cast<uint64_t>(static_cast<uint8_t>(c))) * kDigestPrime;
  }
  return (kVectorKind << kKindShift) | (h & kVectorMask);
}

} // namespace

TilePool::TilePool(const Config &config, Data::SourceSet &sources, Data::Transport &transport)
    : Sources_(sources),
      Wire_(transport),
      OriginLatDeg_(config.OriginLatDeg),
      OriginLonDeg_(config.OriginLonDeg),
      ByteBudget_(config.ByteBudget),
      Decoded_(std::make_shared<Ground::DecodedCache>(config.DecodedBytes)),
      PollAttempts_(config.PollAttempts),
      CarrierCount_(config.Carriers),
      FocusLatDeg_(config.OriginLatDeg),
      FocusLonDeg_(config.OriginLonDeg) {
  const int n = config.Threads > 0 ? config.Threads : 1;
  ContextBytes_ = std::vector<std::atomic<size_t>>(static_cast<size_t>(n));
  Threads_.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; i++) {
    Threads_.emplace_back([this, i] { Work(i); });
  }
  const int carriers = CarrierCount_ > 0 ? CarrierCount_ : 2;
  Carriers_.reserve(static_cast<size_t>(carriers));
  for (int i = 0; i < carriers; i++) {
    Carriers_.emplace_back([this] { Carry(); });
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
  for (std::thread &t : Threads_) { t.join(); }
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
  const std::scoped_lock queue(QueueMutex_);
  out.Posts = Posts_;
  out.Repeats = Repeats_;
  out.QueueDepth = static_cast<long long>(Queue_.size());
  out.Outstanding = static_cast<long long>(Posted_.size());
  out.Parked = static_cast<long long>(Awaiting_.size());
  out.ParkedJobs = 0;
  for (const auto &one : Awaiting_) { out.ParkedJobs += static_cast<long long>(one.second.size()); }
  out.Held = static_cast<long long>(Done_.size());
  return out;
}

size_t TilePool::ByteCacheBytes() const {
  const std::scoped_lock lock(CacheMutex_);
  size_t bytes = CapacityBytes(Cache_);
  for (const CacheEntry &e : Cache_) { bytes += e.Key.capacity() + CapacityBytes(e.Data); }
  return bytes;
}

size_t TilePool::DemCacheBytes() const {
  size_t bytes = CapacityBytes(ContextBytes_);
  for (const std::atomic<size_t> &slot : ContextBytes_) {
    bytes += slot.load(std::memory_order_relaxed);
  }
  return bytes;
}

size_t TilePool::SchedulerBytes() const {
  const std::scoped_lock lock(QueueMutex_);
  size_t bytes = CapacityBytes(Queue_);
  for (const Job &j : Queue_) { bytes += j.Ask ? j.Ask->Key().capacity() : 0; }
  bytes += TreeNodeBytes<uint64_t>(Posted_.size());
  bytes += TreeNodeBytes<std::pair<const uint64_t, Result>>(Done_.size());
  for (const std::pair<const uint64_t, Result> &d : Done_) {
    bytes += CapacityBytes(d.second.Build.Nodes) + CapacityBytes(d.second.Landed.Bytes);
  }
  return bytes;
}

void TilePool::RefuseUntil(const std::string &key, double untilMs) {
  const std::scoped_lock lock(CacheMutex_);
  const auto found = CacheAt_.find(key);
  if (found != CacheAt_.end()) {
    Cache_[found->second].RefusedUntilMs = untilMs;
    return;
  }
  CacheEntry made;
  made.Key = key;
  made.RefusedUntilMs = untilMs;
  made.Used = ++CacheClock_;
  CacheAt_.emplace(key, Cache_.size());
  Cache_.push_back(std::move(made));
}

TilePool::Reply TilePool::Lookup(const std::string &key, Landing *out) {
  const std::scoped_lock lock(CacheMutex_);
  const auto found = CacheAt_.find(key);
  if (found == CacheAt_.end()) { return Reply::Pending; }
  CacheEntry &e = Cache_[found->second];
  e.Used = ++CacheClock_;
  if (e.RefusedUntilMs > 0.0 && e.RefusedUntilMs > Wire_.NowMs()) { return Reply::Refused; }
  if (e.Absent) { return Reply::Absent; }
  if (e.Data.empty() && e.RefusedUntilMs > 0.0) { return Reply::Pending; }
  out->Bytes.assign(e.Data.begin(), e.Data.end());
  out->At = e.At;
  return Reply::Ready;
}

void TilePool::Remember(
    const std::string &key, const uint8_t *data, size_t len, const Data::Address &at, bool absent) {
  const std::scoped_lock lock(CacheMutex_);
  if (CacheAt_.contains(key)) { return; }
  long evicted = 0;
  while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_) {
    size_t victim = 0;
    for (size_t i = 1; i < Cache_.size(); i++) {
      if (Cache_[i].Used < Cache_[victim].Used) { victim = i; }
    }
    CacheBytes_ -= Cache_[victim].Data.size();
    CacheAt_.erase(Cache_[victim].Key);
    const size_t last = Cache_.size() - 1u;
    if (victim != last) {
      Cache_[victim] = std::move(Cache_[last]);
      CacheAt_[Cache_[victim].Key] = victim;
    }
    Cache_.pop_back();
    evicted++;
  }
  if (evicted > 0) {
    const std::scoped_lock ledger(LedgerMutex_);
    Ledger_.Evictions += evicted;
  }
  CacheEntry e;
  e.Key = key;
  e.At = at;
  e.Absent = absent;
  e.Used = ++CacheClock_;
  if (!absent && len > 0) { e.Data.assign(data, data + len); }
  CacheBytes_ += e.Data.size();
  CacheAt_.emplace(key, Cache_.size());
  Cache_.push_back(std::move(e));
}

TilePool::Reply TilePool::FetchInto(const Data::Fetch &request, Landing *out) {
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
    const auto t0 = std::chrono::steady_clock::now();
    Data::Delivery answer = Sources_.Collect(query, Wire_);
    pollMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (const std::optional<Data::Delivery::Answer> taken = answer.Take()) {
      out->Bytes.assign(taken->Bytes.begin(), taken->Bytes.end());
      out->At = taken->At;
      Remember(key, out->Bytes.data(), out->Bytes.size(), taken->At, false);
      reply = Reply::Ready;
      break;
    }
    switch (answer.Where()) {
      case Data::Delivery::State::Pending: (void)Wire_.Await(static_cast<double>(kPollMs)); break;
      case Data::Delivery::State::Vacant:

        Remember(key, nullptr, 0, request.Where(), true);
        reply = Reply::Absent;
        break;
      case Data::Delivery::State::Undeclared:

        Log::Error(LogTag::World, "tile_undeclared", {{"request", key}});
        reply = Reply::Undeclared;
        break;
      case Data::Delivery::State::Refused:
        RefuseUntil(key, Wire_.NowMs() + answer.AfterMs());
        Log::Error(LogTag::World, "tile_refused", {{"request", key}});
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
  const Reply resident = Lookup(key, out);
  if (resident != Reply::Pending) { return resident; }
  Job job;
  job.Kind = Rank::Fetch;
  job.Key = RequestKey(key);
  job.Ask = request;
  Result result;
  const Reply posted = Poll(job, &result);
  if (posted != Reply::Ready) { return posted; }
  *out = std::move(result.Landed);
  return Reply::Ready;
}

TilePool::Reply TilePool::BytesBlocking(const Data::Fetch &request, Landing *out) {
  const Reply resident = Lookup(request.Key(), out);
  if (resident != Reply::Pending) { return resident; }
  return FetchInto(request, out);
}

namespace {

class PoolTerrain : public TerrainSource {
public:
  explicit PoolTerrain(TilePool &pool) : Pool_(pool) {}

  TerrainBytes Take(Data::TileId at) override {
    const Data::Fetch request(Data::DataKind::Elevation, Data::Address::At(at));
    TilePool::Landing landing;
    const TilePool::Reply asked =
        Pool_.Carries() ? Pool_.Bytes(request, &landing) : Pool_.BytesBlocking(request, &landing);
    switch (asked) {
      case TilePool::Reply::Ready: return Answered(landing);
      case TilePool::Reply::Absent:
      case TilePool::Reply::Undeclared: return TerrainBytes::Nothing();
      case TilePool::Reply::Refused: return TerrainBytes::Wire();
      case TilePool::Reply::Pending:
        if (Pool_.Carries()) { tAwaited = RequestKey(request.Key()); }
        break;
    }
    return TerrainBytes::Waiting();
  }

private:
  static TerrainBytes Answered(TilePool::Landing &landing) {
    const std::optional<Data::TileId> at = landing.At.Tile();
    if (!at) { return TerrainBytes::Wire(); }
    return TerrainBytes::From(*at, std::move(landing.Bytes));
  }

  TilePool &Pool_;
};

} // namespace

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

} // namespace

void TilePool::RunMesh(TerrainTiles &tiles, const Job &job, Result *out) {
  const TerrainGrid::State stood = tiles.NodesOf({.Zoom = job.Z, .X = job.X, .Y = job.Y},
                                                 job.Grid,
                                                 &out->Build.Nodes,
                                                 &out->Build.Postings,
                                                 &out->Build.Side);
  Miss miss = MissOf(stood);
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
    Log::Warn(LogTag::World,
              "tile_mesh_refused",
              {{"z", job.Z},
               {"x", static_cast<int>(job.X)},
               {"y", static_cast<int>(job.Y)},
               {"stage", stage},
               {"rc", static_cast<int>(stood)}});
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

void TilePool::Carry() {
  const Heap::Tagged carrying("tile-carrier");
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
    result.State = job.Ask ? FetchInto(*job.Ask, &result.Landed) : Reply::Refused;
    const double spanMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    {
      const std::scoped_lock ledger(LedgerMutex_);
      Ledger_.FetchMs += spanMs;
      Ledger_.FetchBlockedMs += tFetchBlockedMs - blockedBefore;
    }
    {
      const std::scoped_lock lock(QueueMutex_);
      const Reply said = result.State;
      if (said == Reply::Pending) {
        Posted_.erase(job.Key);
      } else if (Posted_.contains(job.Key)) {
        Done_[job.Key] = std::move(result);
        Lands(job.Key, false);
      }
      const auto parked = Awaiting_.find(job.Key);
      if (parked != Awaiting_.end()) {
        if (said != Reply::Pending) {
          for (const Job &held : parked->second) { Queue_.push_back(held); }
        } else {
          for (const Job &held : parked->second) { Posted_.erase(held.Key); }
        }
        Awaiting_.erase(parked);
      }
      Landed_.notify_all();
      Wake_.notify_all();
    }
  }
}

void TilePool::Work(int slot) {
  const Heap::Tagged working("tile-worker");
  StackProbe::Enter(StackProbe::Purpose::Tile);
  const EnuFrame frame =
      EnuFrame::At(Geo{.LongitudeDeg = OriginLonDeg_, .LatitudeDeg = OriginLatDeg_});

  if (frame.Where() != EnuFrame::State::Usable) {
    Log::Error(LogTag::World, "tilepool_origin_too_polar", {{"lat", OriginLatDeg_}});
    std::abort();
  }
  PoolTerrain source(*this);
  TerrainTiles::Config config;
  config.Shared = Decoded_;
  TerrainTiles tiles(source, frame, config);

  ContextBytes_[static_cast<size_t>(slot)].store(tiles.HeapBytes(), std::memory_order_relaxed);

  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(QueueMutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Queue_.empty(); });
      if (Stopping_) { break; }
      size_t best = 0;
      for (size_t i = 1; i < Queue_.size(); i++) {
        const Job &a = Queue_[i];
        const Job &b = Queue_[best];
        if (a.Kind < b.Kind || (a.Kind == b.Kind && a.Z > b.Z) ||
            (a.Kind == b.Kind && a.Z == b.Z && a.TileDist < b.TileDist)) {
          best = i;
        }
      }
      job = Queue_[best];
      Queue_.erase(Queue_.begin() + static_cast<long>(best));
    }
    Result result;
    const double blockedBefore = tFetchBlockedMs;
    const auto t0 = std::chrono::steady_clock::now();
    tAwaited = 0;
    switch (job.Kind) {
      case Rank::Mesh: {
        ShapedGround told;
        {
          const std::scoped_lock lock(QueueMutex_);
          told = Shape_;
        }
        if (!told.Kind.empty()) {
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
        RunMesh(tiles, job, &result);
      }
        result.Holds = true;
        break;
      case Rank::Field:
        RunField(tiles, job, &result);
        result.Holds = false;
        break;
      case Rank::Fetch:
        result.State = job.Ask ? FetchInto(*job.Ask, &result.Landed) : Reply::Refused;
        break;
    }
    if (result.State == Reply::Pending && tAwaited != 0) {
      const uint64_t awaited = tAwaited;
      tAwaited = 0;
      std::unique_lock<std::mutex> lock(QueueMutex_);
      if (Done_.contains(awaited)) {
        Queue_.push_back(job);
        lock.unlock();
        Wake_.notify_all();
        continue;
      }
      if (Posted_.contains(awaited)) {
        Awaiting_[awaited].push_back(job);
        continue;
      }
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
    ContextBytes_[static_cast<size_t>(slot)].store(tiles.HeapBytes(), std::memory_order_relaxed);
    {
      const std::scoped_lock lock(QueueMutex_);

      if (result.State == Reply::Pending) {
        Posted_.erase(job.Key);
        {
          const std::scoped_lock ledger(LedgerMutex_);
          if (job.Kind == Rank::Mesh) { Ledger_.MeshDropped++; }
          if (job.Kind == Rank::Field) { Ledger_.FieldDropped++; }
        }
        Landed_.notify_all();
      }

      else if (Posted_.contains(job.Key)) {
        if (result.State == Reply::Absent || result.State == Reply::Undeclared) {
          result.Build = TileBuild{};
        }
        const bool holds = result.Holds;
        Done_[job.Key] = std::move(result);
        Lands(job.Key, holds);
        Landed_.notify_all();
      }
    }
  }
  ContextBytes_[static_cast<size_t>(slot)].store(0, std::memory_order_relaxed);
}

void TilePool::Lands(uint64_t key, bool holds) {
  std::deque<uint64_t> &kept = holds ? Kept_ : Passing_;
  const size_t most = holds ? kMostKept : kMostPassing;
  kept.push_back(key);
  while (kept.size() > most) {
    const uint64_t oldest = kept.front();
    kept.pop_front();
    if (std::ranges::find(kept, oldest) != kept.end()) { continue; }
    Done_.erase(oldest);
    Posted_.erase(oldest);
  }
}

TilePool::Reply TilePool::Poll(const Job &job, Result *out) {
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const auto done = Done_.find(job.Key);
  if (done != Done_.end()) {
    if (done->second.State == Reply::Absent || done->second.State == Reply::Undeclared) {
      out->State = done->second.State;
      return out->State;
    }
    const bool consumed = !done->second.Holds;
    if (consumed) {
      *out = std::move(done->second);
      Done_.erase(done);
      Posted_.erase(job.Key);
    } else {
      *out = done->second;
    }
    const auto parked = Awaiting_.find(job.Key);
    if (parked != Awaiting_.end()) {
      for (const Job &held : parked->second) { Queue_.push_back(held); }
      Awaiting_.erase(parked);
      lock.unlock();
      Wake_.notify_all();
    }
    (void)consumed;
    return out->State;
  }
  if (Posted_.insert(job.Key).second) {
    Posts_++;
    Job posting = job;
    if (posting.Kind == Rank::Mesh || posting.Kind == Rank::Field) {
      posting.TileDist = TileDistance({.Zoom = posting.Z, .X = posting.X, .Y = posting.Y});
    }
    const bool carries = posting.Kind == Rank::Fetch;
    (carries ? Carrying_ : Queue_).push_back(posting);
    lock.unlock();
    Wake_.notify_all();
  } else {
    Repeats_++;
  }
  return Reply::Pending;
}

TilePool::Reply TilePool::Wants(Data::TileId of, int grid) {
  const int z = of.Zoom;
  const uint32_t x = of.X;
  const uint32_t y = of.Y;
  const uint64_t key = MeshKey(z, x, y);
  {
    const std::scoped_lock lock(QueueMutex_);
    const auto done = Done_.find(key);
    if (done != Done_.end()) { return done->second.State; }
    if (Posted_.contains(key)) { return Reply::Pending; }
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
  Shape_ = how;
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

TilePool::Reply TilePool::Field(Data::TileId of, std::shared_ptr<const TerrainField> *out) {
  Job job;
  job.Kind = Rank::Field;
  job.Z = of.Zoom;
  job.X = of.X;
  job.Y = of.Y;
  job.Key = FieldKey(of.Zoom, of.X, of.Y);
  Result result;
  const Reply state = Poll(job, &result);
  if (state == Reply::Ready) { *out = std::move(result.Field); }
  return state;
}

TilePool::Reply TilePool::FieldAwaited(Data::TileId of, std::shared_ptr<const TerrainField> *out) {
  const Reply asked = Field(of, out);
  if (asked != Reply::Pending) { return asked; }
  const uint64_t key = FieldKey(of.Zoom, of.X, of.Y);
  {
    std::unique_lock<std::mutex> lock(QueueMutex_);
    Landed_.wait(lock, [&] { return Done_.contains(key) || !Posted_.contains(key); });
  }
  return Field(of, out);
}

TilePool::Reply TilePool::MeshAwaited(Data::TileId of, int grid, TileBuild *out) {
  const Reply asked = Mesh(of, grid, out);
  if (asked != Reply::Pending) { return asked; }
  const uint64_t key = MeshKey(of.Zoom, of.X, of.Y);
  {
    std::unique_lock<std::mutex> lock(QueueMutex_);
    Landed_.wait(lock, [&] { return Done_.contains(key) || !Posted_.contains(key); });
  }
  return Mesh(of, grid, out);
}

bool TilePool::AwaitLanding(double seconds) {
  if (seconds <= 0.0) { return false; }
  std::unique_lock<std::mutex> lock(QueueMutex_);
  return Landed_.wait_for(lock, std::chrono::duration<double>(seconds)) ==
         std::cv_status::no_timeout;
}

void TilePool::ForgetMesh(int z, uint32_t x, uint32_t y) {
  const uint64_t key = MeshKey(z, x, y);
  const std::scoped_lock lock(QueueMutex_);
  Posted_.erase(key);
  Done_.erase(key);
}

bool TilePool::Known(uint64_t key) {
  const std::scoped_lock lock(QueueMutex_);
  return Done_.contains(key) || Posted_.contains(key);
}
} // namespace outshine::Ground
