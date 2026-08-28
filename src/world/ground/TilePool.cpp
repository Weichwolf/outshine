#include "TilePool.h"

#include "CookedTile.h"

#include <numbers>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "Capacity.h"
#include "ChunkMesh.h"
#include "Delivery.h"
#include "Heap.h"
#include "Log.h"
#include "SourceSet.h"
#include "StackProbe.h"
#include "TerrainTiles.h"
#include "Transport.h"

namespace outshine::Ground {

namespace {

constexpr int kPollMs = 1;
constexpr int kPollAttempts = 30000;

thread_local double tFetchBlockedMs = 0.0;
thread_local bool tCarries = false;
thread_local uint64_t tAwaited = 0;

uint64_t MeshKey(int z, uint32_t x, uint32_t y) {
  return ((uint64_t)1 << 62) | ((uint64_t)(z & 31) << 56)
       | ((uint64_t)(x & 0xFFFFFFFu) << 28) | (uint64_t)(y & 0xFFFFFFFu);
}
uint64_t RequestKey(const std::string &key) {
  uint64_t h = 1469598103934665603ull;
  for (char c : key) h = (h ^ (uint64_t)(uint8_t)c) * 1099511628211ull;
  return ((uint64_t)3 << 62) | (h & 0x3FFFFFFFFFFFFFFFull);
}

}

TilePool::TilePool(const Config &config, Data::SourceSet &sources, Data::Transport &transport)
    : Sources_(sources),
      Wire_(transport),
      OriginLatDeg_(config.OriginLatDeg),
      OriginLonDeg_(config.OriginLonDeg),
      ByteBudget_(config.ByteBudget),
      DemCacheTiles_(config.DemCacheTiles),
      PollAttempts_(config.PollAttempts),
      CarrierCount_(config.Carriers),
      FocusLatDeg_(config.OriginLatDeg),
      FocusLonDeg_(config.OriginLonDeg) {
  const int n = config.Threads > 0 ? config.Threads : 1;
  ContextBytes_ = std::vector<std::atomic<size_t>>((size_t)n);
  Threads_.reserve((size_t)n);
  for (int i = 0; i < n; i++) Threads_.emplace_back([this, i] { Work(i); });
  const int carriers = CarrierCount_ > 0 ? CarrierCount_ : 2;
  Carriers_.reserve((size_t)carriers);
  for (int i = 0; i < carriers; i++) Carriers_.emplace_back([this] { Carry(); });
  Log::Info("world", "tilepool",
            {{"threads", n}, {"inFlightCap", n}, {"byteBudgetMB", (double)ByteBudget_ / 1048576.0},
             {"demCacheTilesPerThread", DemCacheTiles_}});
}

TilePool::~TilePool() {
  {
    std::lock_guard<std::mutex> lock(QueueMutex_);
    Stopping_ = true;
  }
  Wake_.notify_all();
  for (std::thread &t : Threads_) t.join();
  for (std::thread &t : Carriers_) t.join();
  const Ledger &l = Ledger_;
  const double meshes = l.MeshTiles > 0 ? (double)l.MeshTiles : 1.0;
  const double fetches = l.Fetches > 0 ? (double)l.Fetches : 1.0;
  Log::Debug("world", "tilepool_closed",
             {{"meshTiles", l.MeshTiles}, {"meshAbsent", l.MeshAbsent},
              {"meshRefused", l.MeshRefused},
              {"meshCpuMs", l.MeshCpuMs}, {"meshCpuMsPerTile", l.MeshCpuMs / meshes},
              {"fetches", l.Fetches}, {"fetchAbsent", l.FetchAbsent},
              {"fetchRefused", l.FetchRefused},
              {"fetchGaveUp", l.FetchGaveUp}, {"fetchMs", l.FetchMs},
              {"fetchMsPerGet", l.FetchMs / fetches},
              {"fetchBlockedMs", l.FetchBlockedMs}, {"fetchOnCompute", (int)l.FetchOnCompute},
              {"retryWaitMs", l.FetchBlockedMs - l.FetchMs}, {"fetchedMB", l.FetchedMB},
              {"byteCacheMB", (double)ByteCacheBytes() / 1048576.0},
              {"byteCacheEntries", (int)Cache_.size()}, {"evictions", l.Evictions}});
}

void TilePool::Focus(double latDeg, double lonDeg) {
  std::lock_guard<std::mutex> lock(QueueMutex_);
  FocusLatDeg_ = latDeg;
  FocusLonDeg_ = lonDeg;
}

double TilePool::TileDistance(int z, uint32_t x, uint32_t y) const {
  const double n = std::ldexp(1.0, z);
  const double cx = (FocusLonDeg_ + 180.0) / 360.0 * n;
  const double lat = FocusLatDeg_ * std::numbers::pi / 180.0;
  const double cy = (1.0 - std::asinh(std::tan(lat)) / std::numbers::pi) * 0.5 * n;
  const double dx = (double)x + 0.5 - cx, dy = (double)y + 0.5 - cy;
  return dx * dx + dy * dy;
}

TilePool::Ledger TilePool::Counters() const {
  Ledger out;
  {
    std::lock_guard<std::mutex> lock(LedgerMutex_);
    out = Ledger_;
  }
  std::lock_guard<std::mutex> queue(QueueMutex_);
  out.Posts = Posts_;
  out.Repeats = Repeats_;
  out.QueueDepth = (long long)Queue_.size();
  return out;
}

size_t TilePool::ByteCacheBytes() const {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  size_t bytes = CapacityBytes(Cache_);
  for (const CacheEntry &e : Cache_) bytes += e.Key.capacity() + CapacityBytes(e.Data);
  return bytes;
}

size_t TilePool::DemCacheBytes() const {
  size_t bytes = CapacityBytes(ContextBytes_);
  for (const std::atomic<size_t> &slot : ContextBytes_) bytes += slot.load(std::memory_order_relaxed);
  return bytes;
}

size_t TilePool::SchedulerBytes() const {
  std::lock_guard<std::mutex> lock(QueueMutex_);
  size_t bytes = CapacityBytes(Queue_);
  for (const Job &j : Queue_) bytes += j.Ask ? j.Ask->Key().capacity() : 0;
  bytes += TreeNodeBytes<uint64_t>(Posted_.size());
  bytes += TreeNodeBytes<std::pair<const uint64_t, Result>>(Done_.size());
  for (const std::pair<const uint64_t, Result> &d : Done_)
    bytes += CapacityBytes(d.second.Build.Verts) + CapacityBytes(d.second.Build.Idx) +
             CapacityBytes(d.second.Build.Clusters);
  return bytes;
}

void TilePool::RefuseUntil(const std::string &key, double untilMs) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
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
  std::lock_guard<std::mutex> lock(CacheMutex_);
  const auto found = CacheAt_.find(key);
  if (found == CacheAt_.end()) { return Reply::Pending; }
  CacheEntry &e = Cache_[found->second];
  e.Used = ++CacheClock_;
  if (e.RefusedUntilMs > 0.0 && e.RefusedUntilMs > Wire_.NowMs()) { return Reply::Refused; }
  if (e.Absent) return Reply::Absent;
  if (e.Data.empty() && e.RefusedUntilMs > 0.0) { return Reply::Pending; }
  out->Bytes.assign(e.Data.begin(), e.Data.end());
  out->At = e.At;
  return Reply::Ready;
}

void TilePool::Remember(const std::string &key, const uint8_t *data, size_t len,
                        const Data::Address &at, bool absent) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  if (CacheAt_.find(key) != CacheAt_.end()) { return; }
  long evicted = 0;
  while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_) {
    size_t victim = 0;
    for (size_t i = 1; i < Cache_.size(); i++)
      if (Cache_[i].Used < Cache_[victim].Used) victim = i;
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
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.Evictions += evicted;
  }
  CacheEntry e;
  e.Key = key;
  e.At = at;
  e.Absent = absent;
  e.Used = ++CacheClock_;
  if (!absent && len > 0) e.Data.assign(data, data + len);
  CacheBytes_ += e.Data.size();
  CacheAt_.emplace(key, Cache_.size());
  Cache_.push_back(std::move(e));
}

TilePool::Reply TilePool::FetchInto(const Data::Request &request, Landing *out) {
  if (!tCarries) {
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
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
    pollMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    Data::Delivery::Answer taken;
    if (answer.TryTake(&taken)) {
      out->Bytes.assign(taken.Bytes.begin(), taken.Bytes.end());
      out->At = taken.At;
      Remember(key, out->Bytes.data(), out->Bytes.size(), taken.At, false);
      reply = Reply::Ready;
      break;
    }
    switch (answer.Where()) {
      case Data::Delivery::State::Pending:
        (void)Wire_.Await((double)kPollMs);
        break;
      case Data::Delivery::State::Vacant:

        Remember(key, nullptr, 0, request.Where(), true);
        reply = Reply::Absent;
        break;
      case Data::Delivery::State::Undeclared:

        Log::Error("world", "tile_undeclared", {{"request", key}});
        reply = Reply::Undeclared;
        break;
      case Data::Delivery::State::Refused:
        RefuseUntil(key, Wire_.NowMs() + answer.AfterMs());
        Log::Error("world", "tile_refused", {{"request", key}});
        reply = Reply::Refused;
        break;
      case Data::Delivery::State::Delivered:
        break;
    }
  }
  if (reply == Reply::Pending) Sources_.Abandon(query, Wire_);
  const double blockedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entered).count();
  tFetchBlockedMs += blockedMs;
  {
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.Fetches++;
    Ledger_.FetchMs += pollMs;
    Ledger_.FetchBlockedMs += blockedMs;
    if (reply == Reply::Ready) Ledger_.FetchedMB += (double)out->Bytes.size() / 1048576.0;
    if (reply == Reply::Absent) Ledger_.FetchAbsent++;

    if (reply == Reply::Refused || reply == Reply::Undeclared) Ledger_.FetchRefused++;
    if (reply == Reply::Pending) Ledger_.FetchGaveUp++;
  }
  if (reply != Reply::Ready) out->Bytes.clear();
  return reply;
}

TilePool::Reply TilePool::Bytes(const Data::Request &request, Landing *out) {
  const std::string key = request.Key();
  const Reply resident = Lookup(key, out);
  if (resident != Reply::Pending) return resident;
  Job job;
  job.Kind = Rank::Fetch;
  job.Key = RequestKey(key);
  job.Ask = request;
  Result result;
  const Reply posted = Poll(std::move(job), &result);
  if (posted == Reply::Pending) return Reply::Pending;

  if (posted == Reply::Undeclared) return Reply::Undeclared;
  return Lookup(key, out);
}

TilePool::Reply TilePool::BytesBlocking(const Data::Request &request, Landing *out) {
  const Reply resident = Lookup(request.Key(), out);
  if (resident != Reply::Pending) return resident;
  return FetchInto(request, out);
}

namespace {

class PoolTerrain : public TerrainSource {
 public:
  explicit PoolTerrain(TilePool &pool) : Pool_(pool) {}

  TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
    const Data::Request request(Data::DataKind::Elevation, Data::Address::Tile(z, x, y));
    TilePool::Landing landing;
    const TilePool::Reply asked =
        Pool_.Carries() ? Pool_.Bytes(request, &landing) : Pool_.BytesBlocking(request, &landing);
    switch (asked) {
      case TilePool::Reply::Ready: return Answered(landing);
      case TilePool::Reply::Absent: return TerrainBytes::Nothing();

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
    int az = 0;
    uint32_t ax = 0, ay = 0;
    if (!landing.At.TryTile(&az, &ax, &ay)) return TerrainBytes::Wire();
    return TerrainBytes::From(az, ax, ay, std::move(landing.Bytes));
  }

 private:
  TilePool &Pool_;
};

}

namespace {

enum class Miss { None, Hole, Wait, Refused };

[[nodiscard]] Miss MissOf(TerrainMesh::State state) {
  switch (state) {
    case TerrainMesh::State::Built: return Miss::None;

    case TerrainMesh::State::NoTile: return Miss::Hole;
    case TerrainMesh::State::Deferred: return Miss::Wait;
    case TerrainMesh::State::SourceUndecodable:
    case TerrainMesh::State::SourceRefused:
    case TerrainMesh::State::FieldTooSmall:
    case TerrainMesh::State::StrideDoesNotDivide:
    case TerrainMesh::State::FrameUnusable: return Miss::Refused;
  }
  return Miss::Refused;
}

}

void TilePool::RunMesh(TerrainTiles &tiles, const Job &job, Result *out) {
  const TerrainMesh mesh = tiles.MeshOf(job.Z, job.X, job.Y);
  Chunk chunk = {};
  double origin[3] = {0.0, 0.0, 0.0};
  const char *stage = "source";
  Miss miss = MissOf(mesh.Where());
  if (miss == Miss::None &&
      (!ChunkBuildEcef(mesh, job.Z, job.X, job.Y, job.Grid, &chunk, origin) || chunk.nverts <= 0)) {
    miss = Miss::Refused;
    stage = "grid";
  }
  if (miss == Miss::None) {
    CookTile((const float *)chunk.verts, chunk.nverts, chunk.gridverts, origin,
             out->Build.Verts, out->Build.Idx, out->Build.Clusters);
    out->Build.ErrM = chunk.err;
    if (out->Build.Verts.empty() || out->Build.Idx.empty() || out->Build.Clusters.empty()) {
      miss = Miss::Refused;
      stage = "partition";
    } else {
      for (int a = 0; a < 3; a++) out->Build.OriginEcef[a] = origin[a];
    }
  }
  ChunkFree(&chunk);
  if (miss == Miss::None) {
    out->State = Reply::Ready;
    return;
  }
  if (miss == Miss::Refused) {
    Log::Warn("world", "tile_mesh_refused", {{"z", job.Z}, {"x", (int)job.X}, {"y", (int)job.Y},
                                             {"stage", stage}, {"rc", (int)mesh.Where()}});
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.MeshRefused++;
  }

  switch (miss) {
    case Miss::Hole: out->State = Reply::Absent; break;
    case Miss::Refused: out->State = Reply::Refused; break;
    case Miss::Wait:
    case Miss::None: out->State = Reply::Pending; break;
  }
}


void TilePool::Carry(void) {
  const Heap::Tagged carrying("tile-carrier");
  StackProbe::Enter(StackProbe::Purpose::Tile);
  tCarries = true;
  Landing scratch;
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(QueueMutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Carrying_.empty(); });
      if (Stopping_) { break; }
      job = std::move(Carrying_.front());
      Carrying_.erase(Carrying_.begin());
    }
    Result result;
    const double blockedBefore = tFetchBlockedMs;
    const auto t0 = std::chrono::steady_clock::now();
    result.State = job.Ask ? FetchInto(*job.Ask, &scratch) : Reply::Refused;
    const double spanMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    {
      std::lock_guard<std::mutex> ledger(LedgerMutex_);
      Ledger_.FetchMs += spanMs;
      Ledger_.FetchBlockedMs += tFetchBlockedMs - blockedBefore;
    }
    {
      std::lock_guard<std::mutex> lock(QueueMutex_);
      if (result.State == Reply::Pending) {
        Posted_.erase(job.Key);
      } else if (Posted_.find(job.Key) != Posted_.end()) {
        Done_[job.Key] = std::move(result);
      }
      const auto parked = Awaiting_.find(job.Key);
      if (parked != Awaiting_.end()) {
        if (result.State != Reply::Pending) {
          for (Job &held : parked->second) { Queue_.push_back(std::move(held)); }
        } else {
          for (Job &held : parked->second) { Posted_.erase(held.Key); }
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
  const EnuFrame frame = EnuFrame::At(OriginLatDeg_, OriginLonDeg_);

  if (frame.Where() != EnuFrame::State::Usable) {
    Log::Error("world", "tilepool_origin_too_polar", {{"lat", OriginLatDeg_}});
    std::abort();
  }
  PoolTerrain source(*this);
  TerrainTiles::Config config;
  config.DemCacheTiles = DemCacheTiles_;
  TerrainTiles tiles(source, frame, config);

  ContextBytes_[(size_t)slot].store(tiles.HeapBytes(), std::memory_order_relaxed);

  Landing scratch;
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(QueueMutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Queue_.empty(); });
      if (Stopping_) break;
      size_t best = 0;
      for (size_t i = 1; i < Queue_.size(); i++) {
        const Job &a = Queue_[i], &b = Queue_[best];
        // FINER FIRST, THEN NEARER. Sorting by distance alone inverts the priority: a coarse tile
        // covering hundreds of kilometres is CENTRED on the camera, so it wins against every fine
        // tile the eye actually resolves -- measured, the Grand Canyon loaded 55 per cent of its
        // tiles and read 59 m of relief where its rim-to-river drop is over 1 500 m, because what
        // arrived was the coarse levels. Cesium selects by screen-space error and loads the
        // selected set nearest-first; the depth in the tree is this cascade's stand-in for that
        // error, so depth leads and distance breaks the tie.
        if (a.Kind < b.Kind || (a.Kind == b.Kind && a.Z > b.Z) ||
            (a.Kind == b.Kind && a.Z == b.Z && a.TileDist < b.TileDist)) {
          best = i;
        }
      }
      job = std::move(Queue_[best]);
      Queue_.erase(Queue_.begin() + (long)best);
    }
    Result result;
    const double blockedBefore = tFetchBlockedMs;
    const auto t0 = std::chrono::steady_clock::now();
    tAwaited = 0;
    switch (job.Kind) {
      case Rank::Mesh:
        RunMesh(tiles, job, &result);
        break;
      case Rank::Fetch:
        result.State = job.Ask ? FetchInto(*job.Ask, &scratch) : Reply::Refused;
        break;
    }
    if (result.State == Reply::Pending && tAwaited != 0) {
      const uint64_t awaited = tAwaited;
      tAwaited = 0;
      std::unique_lock<std::mutex> lock(QueueMutex_);
      if (Posted_.find(awaited) != Posted_.end()) {
        Awaiting_[awaited].push_back(std::move(job));
        continue;
      }
    }
    tAwaited = 0;
    const double spanMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    const double cpuMs = spanMs - (tFetchBlockedMs - blockedBefore);
    {
      std::lock_guard<std::mutex> ledger(LedgerMutex_);
      if (job.Kind == Rank::Mesh) {
        Ledger_.MeshTiles++;
        Ledger_.MeshCpuMs += cpuMs;
        if (result.State == Reply::Absent) Ledger_.MeshAbsent++;
      }
    }
    StackProbe::Mark();
    ContextBytes_[(size_t)slot].store(tiles.HeapBytes(), std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(QueueMutex_);

      if (result.State == Reply::Pending) {
        Posted_.erase(job.Key);
        Landed_.notify_all();
      }

      else if (Posted_.find(job.Key) != Posted_.end()) {

        if (result.State == Reply::Absent || result.State == Reply::Undeclared)
          result.Build = TileBuild{};
        Done_[job.Key] = std::move(result);
        Landed_.notify_all();
      }
    }
  }
  ContextBytes_[(size_t)slot].store(0, std::memory_order_relaxed);
}

TilePool::Reply TilePool::Poll(Job &&job, Result *out) {
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const auto done = Done_.find(job.Key);
  if (done != Done_.end()) {

    if (done->second.State == Reply::Absent || done->second.State == Reply::Undeclared) {
      out->State = done->second.State;
      return out->State;
    }
    *out = std::move(done->second);
    Done_.erase(done);
    Posted_.erase(job.Key);
    return out->State;
  }
  if (Posted_.insert(job.Key).second) {
    Posts_++;
    if (job.Kind == Rank::Mesh) job.TileDist = TileDistance(job.Z, job.X, job.Y);
    const bool carries = job.Kind == Rank::Fetch;
    (carries ? Carrying_ : Queue_).push_back(std::move(job));
    lock.unlock();
    Wake_.notify_all();
  } else {
    Repeats_++;
  }
  return Reply::Pending;
}

// WANTS ASKS WITHOUT TAKING. `Poll` CONSUMES a finished result -- it moves it out, erases it from
// `Done_` and drops the key from `Posted_` -- which is right for a caller that is going to use the
// mesh and wrong for one that only wants to know whether it has landed. A residency poll built on
// `Poll` throws away every tile it finds and re-posts the job, so the pool re-parses the same tile
// forever and never settles. This posts the job if it is unknown and reports what it sees, and it
// takes nothing.
TilePool::Reply TilePool::Wants(int z, uint32_t x, uint32_t y, int grid) {
  const uint64_t key = MeshKey(z, x, y);
  {
    std::lock_guard<std::mutex> lock(QueueMutex_);
    const auto done = Done_.find(key);
    if (done != Done_.end()) { return done->second.State; }
    if (Posted_.find(key) != Posted_.end()) { return Reply::Pending; }
  }
  Job job;
  job.Kind = Rank::Mesh;
  job.Z = z;
  job.X = x;
  job.Y = y;
  job.Grid = grid;
  job.Key = key;
  Result result;
  return Poll(std::move(job), &result);
}

TilePool::Reply TilePool::Mesh(int z, uint32_t x, uint32_t y, int grid, TileBuild *out) {
  Job job;
  job.Kind = Rank::Mesh;
  job.Z = z;
  job.X = x;
  job.Y = y;
  job.Grid = grid;
  job.Key = MeshKey(z, x, y);
  Result result;
  const Reply state = Poll(std::move(job), &result);
  if (state == Reply::Ready) *out = std::move(result.Build);
  return state;
}

TilePool::Reply TilePool::MeshAwaited(int z, uint32_t x, uint32_t y, int grid,
                                      TileBuild *out) {
  const Reply asked = Mesh(z, x, y, grid, out);
  if (asked != Reply::Pending) { return asked; }
  const uint64_t key = MeshKey(z, x, y);
  {
    std::unique_lock<std::mutex> lock(QueueMutex_);
    Landed_.wait(lock, [&] {
      return Done_.find(key) != Done_.end() || Posted_.find(key) == Posted_.end();
    });
  }
  return Mesh(z, x, y, grid, out);
}

void TilePool::ForgetMesh(int z, uint32_t x, uint32_t y) {
  const uint64_t key = MeshKey(z, x, y);
  std::lock_guard<std::mutex> lock(QueueMutex_);
  Posted_.erase(key);
  Done_.erase(key);
}

bool TilePool::Known(uint64_t key) {
  std::lock_guard<std::mutex> lock(QueueMutex_);
  return Done_.find(key) != Done_.end() || Posted_.find(key) != Posted_.end();
}


}
