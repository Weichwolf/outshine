#include "TilePool.h"

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

namespace outshine::World {

namespace {

/* [SET] How often a waiting thread asks the registry again, and how long it waits before it hands
 * the tile back to the queue: 1 ms, 30 000 times, so 30 s of wall. The poll is fine because the
 * transport is asynchronous by shape — the thread is not holding a connection open, it is asking
 * whether one has landed. It is a ceiling on how long a THREAD is held, not on how often a tile is
 * asked for: the caller re-posts, so a slow upstream delays terrain instead of removing it. */
constexpr int kPollMs = 1;
constexpr int kPollAttempts = 30000;

/* WHAT THIS THREAD HAS SPENT INSIDE FetchInto, cumulative, in ms of wall: the wire AND the polling
 * above. A mesh job subtracts its own share of it, because a build that waited on a tile did not
 * spend that time building. */
thread_local double tFetchBlockedMs = 0.0;

uint64_t MeshKey(int z, uint32_t x, uint32_t y) {
  return ((uint64_t)1 << 62) | ((uint64_t)(z & 31) << 56)
       | ((uint64_t)(x & 0xFFFFFFFu) << 28) | (uint64_t)(y & 0xFFFFFFFu);
}
uint64_t DagKey(int id) { return ((uint64_t)2 << 62) | (uint64_t)(uint32_t)id; }
uint64_t RequestKey(const std::string &key) {   /* FNV-1a: the queue keys on a number, the cache on the text */
  uint64_t h = 1469598103934665603ull;
  for (char c : key) h = (h ^ (uint64_t)(uint8_t)c) * 1099511628211ull;
  return ((uint64_t)3 << 62) | (h & 0x3FFFFFFFFFFFFFFFull);
}

/* The one thing this layer knows about a soup: WHICH float carries the seam. What the vertices mean
 * stays with the caller that built them, so the predicate is a table and not a closure. */
template <int A> int SeamAt(const float *v) { return v[A] < 0.0f ? 1 : 0; }
int (*const kSeam[8])(const float *) = {SeamAt<0>, SeamAt<1>, SeamAt<2>, SeamAt<3>,
                                        SeamAt<4>, SeamAt<5>, SeamAt<6>, SeamAt<7>};

}  // namespace

TilePool::TilePool(const Config &config, Data::SourceSet &sources, Data::Transport &transport)
    : Sources_(sources),
      Wire_(transport),
      OriginLatDeg_(config.OriginLatDeg),
      OriginLonDeg_(config.OriginLonDeg),
      ByteBudget_(config.ByteBudget),
      DemCacheTiles_(config.DemCacheTiles),
      CameraLatDeg_(config.OriginLatDeg),
      CameraLonDeg_(config.OriginLonDeg) {
  const int n = config.Threads > 0 ? config.Threads : 1;
  ContextBytes_ = std::vector<std::atomic<size_t>>((size_t)n);
  Threads_.reserve((size_t)n);
  for (int i = 0; i < n; i++) Threads_.emplace_back([this, i] { Work(i); });
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
  const Ledger &l = Ledger_;
  const double meshes = l.MeshTiles > 0 ? (double)l.MeshTiles : 1.0;
  const double fetches = l.Fetches > 0 ? (double)l.Fetches : 1.0;
  Log::Debug("world", "tilepool_closed",
             {{"meshTiles", l.MeshTiles}, {"meshAbsent", l.MeshAbsent},
              {"meshRefused", l.MeshRefused},
              {"meshCpuMs", l.MeshCpuMs}, {"meshCpuMsPerTile", l.MeshCpuMs / meshes},
              {"dags", l.Dags}, {"dagMs", l.DagMs},
              {"fetches", l.Fetches}, {"fetchAbsent", l.FetchAbsent},
              {"fetchRefused", l.FetchRefused},
              {"fetchGaveUp", l.FetchGaveUp}, {"fetchMs", l.FetchMs},
              {"fetchMsPerGet", l.FetchMs / fetches},
              {"fetchBlockedMs", l.FetchBlockedMs},
              {"retryWaitMs", l.FetchBlockedMs - l.FetchMs}, {"fetchedMB", l.FetchedMB},
              {"byteCacheMB", (double)ByteCacheBytes() / 1048576.0},
              {"byteCacheEntries", (int)Cache_.size()}, {"evictions", l.Evictions}});
}

void TilePool::Camera(double latDeg, double lonDeg) {
  std::lock_guard<std::mutex> lock(QueueMutex_);
  CameraLatDeg_ = latDeg;
  CameraLonDeg_ = lonDeg;
}

double TilePool::TileDistance(int z, uint32_t x, uint32_t y) const {
  const double n = std::ldexp(1.0, z);
  const double cx = (CameraLonDeg_ + 180.0) / 360.0 * n;
  const double lat = CameraLatDeg_ * 3.14159265358979 / 180.0;
  const double cy = (1.0 - std::asinh(std::tan(lat)) / 3.14159265358979) * 0.5 * n;
  const double dx = (double)x + 0.5 - cx, dy = (double)y + 0.5 - cy;
  return dx * dx + dy * dy;
}

/* Two locks, one after the other and never nested: the admission counters live beside the queue
 * they describe, and taking that mutex under the ledger's would be the only nesting in this file. */
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
  for (const Job &j : Queue_) bytes += (j.Ask ? j.Ask->Key().capacity() : 0) + CapacityBytes(j.Soup);
  bytes += TreeNodeBytes<uint64_t>(Posted_.size());
  bytes += TreeNodeBytes<std::pair<const uint64_t, Result>>(Done_.size());
  for (const std::pair<const uint64_t, Result> &d : Done_)
    bytes += CapacityBytes(d.second.Build.Verts) + CapacityBytes(d.second.Build.Idx) +
             CapacityBytes(d.second.Build.Clusters);
  return bytes;
}

TilePool::Reply TilePool::Lookup(const std::string &key, Landing *out) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  for (CacheEntry &e : Cache_) {
    if (e.Key != key) continue;
    e.Used = ++CacheClock_;
    if (e.Absent) return Reply::Absent;
    out->Bytes.assign(e.Data.begin(), e.Data.end());
    out->At = e.At;
    return Reply::Ready;
  }
  return Reply::Pending;
}

/* LEAST RECENTLY USED, by bytes. The working set is the ring around the camera and it moves with
 * it, so a cache with no eviction is a leak with a slow fuse. Nothing hands out a pointer into this
 * table — every reader gets a copy — so an eviction cannot pull the ground out from under one. */
void TilePool::Remember(const std::string &key, const uint8_t *data, size_t len,
                        const Data::Address &at, bool absent) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  for (CacheEntry &e : Cache_)
    if (e.Key == key) return;
  long evicted = 0;
  while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_) {
    size_t victim = 0;
    for (size_t i = 1; i < Cache_.size(); i++)
      if (Cache_[i].Used < Cache_[victim].Used) victim = i;
    CacheBytes_ -= Cache_[victim].Data.size();
    Cache_.erase(Cache_.begin() + (long)victim);
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
  Cache_.push_back(std::move(e));
}

/* ONE REQUEST, POLLED TO A SETTLED ANSWER. The registry is what decides who is asked, in which
 * order and how often; this loop only waits for it and keeps the clock. */
TilePool::Reply TilePool::FetchInto(const Data::Request &request, Landing *out) {
  const std::string key = request.Key();
  Data::SourceSet::Query query = Sources_.Ask(request);
  Reply reply = Reply::Pending;
  const auto entered = std::chrono::steady_clock::now();
  double pollMs = 0.0;
  for (int attempt = 0; attempt < kPollAttempts && reply == Reply::Pending; attempt++) {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        break;
      case Data::Delivery::State::Vacant:
        /* EVERY COVERING SOURCE SAID THERE IS NOTHING HERE. That is the world, and it is the one
         * answer this cache may keep. */
        Remember(key, nullptr, 0, request.Where(), true);
        reply = Reply::Absent;
        break;
      case Data::Delivery::State::Undeclared:
        /* NO SOURCE COVERS IT. A declaration error, not a place — and it will not heal by asking
         * again, so it ends the loop without ever becoming an absence. */
        Log::Error("world", "tile_undeclared", {{"request", key}});
        reply = Reply::Undeclared;
        break;
      case Data::Delivery::State::Refused:
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
    /* One counter for both, because the distinction it is kept for is absence against fault and a
     * declaration error is this tree's fault. Which of the two it was is in the log line above. */
    if (reply == Reply::Refused || reply == Reply::Undeclared) Ledger_.FetchRefused++;
    if (reply == Reply::Pending) Ledger_.FetchGaveUp++;
  }
  if (reply != Reply::Ready) out->Bytes.clear();   /* NOT remembered: the next ask starts the clock again */
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
  /* Nothing is cached for a request nothing covers, and the cache's word for "not here" is Pending
   * — so this answer has to be the job's, or the caller waits on a question already settled. */
  if (posted == Reply::Undeclared) return Reply::Undeclared;
  return Lookup(key, out);   /* the fetch's product is the cache entry, not the job's result */
}

TilePool::Reply TilePool::BytesBlocking(const Data::Request &request, Landing *out) {
  const Reply resident = Lookup(request.Key(), out);
  if (resident != Reply::Pending) return resident;
  return FetchInto(request, out);
}

namespace {

/* THE POOL AS A TERRAIN SOURCE: one ask into the shared byte cache, and the pool's answers become
 * the four the decoder reads. Nothing travels on a thread-local any more. */
class PoolTerrain : public TerrainSource {
 public:
  explicit PoolTerrain(TilePool &pool) : Pool_(pool) {}

  TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
    const Data::Request request(Data::DataKind::Elevation, Data::Address::Tile(z, x, y));
    TilePool::Landing landing;
    switch (Pool_.BytesBlocking(request, &landing)) {
      case TilePool::Reply::Ready: return Answered(landing);
      case TilePool::Reply::Absent: return TerrainBytes::Nothing();
      /* The DEM seam has four words and none of them is "nobody covers this". Nothing() is the only
       * terminal one, and terminal is the half that decides here: no elevation will ever arrive.
       * The cause is not lost, it is one level down — the pool logs it and counts it as a fault. */
      case TilePool::Reply::Undeclared: return TerrainBytes::Nothing();
      case TilePool::Reply::Refused: return TerrainBytes::Wire();
      case TilePool::Reply::Pending: break;
    }
    return TerrainBytes::Waiting();
  }

 private:
  /* THE ADDRESS THAT ANSWERED, not the one that was asked for: a request above the source's last
   * native zoom comes back from an ancestor, and the crop downstream is computed from the
   * difference. A source that answered with a whole-world address for an elevation tile has broken
   * its own contract, and that is a refusal rather than a plausible wrong crop. */
  static TerrainBytes Answered(TilePool::Landing &landing) {
    int az = 0;
    uint32_t ax = 0, ay = 0;
    if (!landing.At.TryTile(&az, &ax, &ay)) return TerrainBytes::Wire();
    return TerrainBytes::From(az, ax, ay, std::move(landing.Bytes));
  }

 private:
  TilePool &Pool_;
};

}  // namespace

/* WHY A MESH WAS NOT BUILT, and only ONE of these is a statement about the world: a Hole is every
 * covering source answering that there is no terrain tile here. A Refused is this tree, the request
 * or the wire being wrong — a PNG that will not decode, a source that is not the grid the stitch
 * promises, a partition with no cluster in it. A Wait is a promise not yet kept.
 *
 * It matters because Absent is TERMINAL at the node (world/World.h MeshState::Vacant): a rung
 * retracted for a proxy error or a slow upstream deletes ground for good. The tile's four stitch
 * neighbours are folded into its own answer inside TerrainTiles, so this reads one state and no
 * longer a thread-local set somewhere below it. */
namespace {

enum class Miss { None, Hole, Wait, Refused };

[[nodiscard]] Miss MissOf(TerrainMesh::State state) {
  switch (state) {
    case TerrainMesh::State::Built: return Miss::None;
    /* A place with no DEM is not a defect: it is cached as a final answer. A source that will not
     * decode is this run saying something is wrong with it — a terrarium tile is 256 by 256 or it is
     * malformed, so a field too small is the source's defect and not the ground under it. */
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

}  // namespace

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
    TileDagBuild((const float *)chunk.verts, chunk.nverts, chunk.gridverts, origin,
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
  /* THE MESH PATH'S ONE DOOR TO A TERMINAL ANSWER, and only a Hole goes through it. RunDag below has
   * a door of its own and it is not this one: a soup too small to partition is a statement about
   * content the caller supplied, not about the world. */
  switch (miss) {
    case Miss::Hole: out->State = Reply::Absent; break;
    case Miss::Refused: out->State = Reply::Refused; break;
    case Miss::Wait:
    case Miss::None: out->State = Reply::Pending; break;
  }
}

void TilePool::RunDag(const Job &job, Result *out) {
  const uint32_t nverts = (uint32_t)(job.Soup.size() / 8);
  if (nverts < 3) {
    out->State = Reply::Absent;
    return;
  }
  const float *soup = job.Soup.data();
  ClusterDag dag;
  ClusterDagOpts opts;
  if (job.SeamAttr >= 0 && job.SeamAttr < 8) opts.ClassOf = kSeam[job.SeamAttr];
  if (ClusterDagBuild(soup, nverts, 8, opts, &dag)) {
    out->Build.Verts = std::move(dag.Verts);
    out->Build.Idx = std::move(dag.Idx);
    out->Build.Clusters = std::move(dag.Clusters);
  } else {
    /* A soup too small to partition still has to appear as ONE cluster, or the caller's cluster list
     * stops describing its own buffer. */
    out->Build.Verts.assign(soup, soup + job.Soup.size());
    out->Build.Idx.resize(nverts);
    for (uint32_t i = 0; i < nverts; i++) out->Build.Idx[i] = i;
    DagCluster c{};
    c.Count = nverts;
    c.ParentErr = kDagRootErr;
    BoundingSphere(soup, nverts, 8, c.SelfCenter, &c.SelfRadius);
    out->Build.Clusters.push_back(c);
  }
  out->State = Reply::Ready;
}

/* EACH THREAD OWNS ITS OWN TerrainTiles: it carries a decoded-DEM cache that the tile being
 * built writes, so sharing one would need a lock around the whole build and the pool would be a
 * queue. The byte cache below it IS shared and hands out copies only. */
void TilePool::Work(int slot) {
  StackProbe::Enter(StackProbe::Purpose::Tile);
  const EnuFrame frame = EnuFrame::At(OriginLatDeg_, OriginLonDeg_);
  /* A THREAD WHOSE FRAME IS NONSENSE IS WORSE THAN NO THREAD: it still takes mesh jobs off the queue
   * and would place every one of them at the origin. */
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
        if (a.Kind < b.Kind || (a.Kind == b.Kind && a.TileDist < b.TileDist)) best = i;
      }
      job = std::move(Queue_[best]);
      Queue_.erase(Queue_.begin() + (long)best);
    }
    Result result;
    const double blockedBefore = tFetchBlockedMs;
    const auto t0 = std::chrono::steady_clock::now();
    switch (job.Kind) {
      case Rank::Mesh:
        RunMesh(tiles, job, &result);
        break;
      case Rank::Dag:
        RunDag(job, &result);
        break;
      case Rank::Fetch:
        result.State = job.Ask ? FetchInto(*job.Ask, &scratch) : Reply::Refused;
        break;
    }
    const double spanMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    /* THE JOB'S OWN WORK, not the wall it occupied. A mesh nests its transport inside itself, so the
     * span is decode + build + whatever the server took; only the difference is a cost this code can
     * be held to. */
    const double cpuMs = spanMs - (tFetchBlockedMs - blockedBefore);
    {
      std::lock_guard<std::mutex> ledger(LedgerMutex_);
      if (job.Kind == Rank::Mesh) {
        Ledger_.MeshTiles++;
        Ledger_.MeshCpuMs += cpuMs;
        if (result.State == Reply::Absent) Ledger_.MeshAbsent++;
      } else if (job.Kind == Rank::Dag) {
        Ledger_.Dags++;
        Ledger_.DagMs += cpuMs;
      }
    }
    StackProbe::Mark();
    ContextBytes_[(size_t)slot].store(tiles.HeapBytes(), std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(QueueMutex_);
      /* A job that could not be finished this time leaves no result and no posting, so the next ask
       * queues it again — the difference between a slow server and a missing tile. */
      if (result.State == Reply::Pending) Posted_.erase(job.Key);
      /* A key that is no longer posted is a caller that let go (ForgetMesh): storing its product
       * would put an entry in Done_ that nothing can ever take out again. */
      else if (Posted_.find(job.Key) != Posted_.end()) {
        /* An Absent outlives the pool's queue (Poll keeps it), so it must not keep a partial build
         * alive with it. */
        if (result.State == Reply::Absent || result.State == Reply::Undeclared)
          result.Build = TileBuild{};
        Done_[job.Key] = std::move(result);
      }
    }
  }
  ContextBytes_[(size_t)slot].store(0, std::memory_order_relaxed);
}

TilePool::Reply TilePool::Poll(Job &&job, Result *out) {
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const auto done = Done_.find(job.Key);
  if (done != Done_.end()) {
    /* An Absent is KEPT, posted and done: the answer is final, so every later ask gets the same one
     * instead of a thread spun on a tile that does not exist. An Undeclared is kept for the same
     * reason and it is the stronger case — nothing declares this request, so a second ask cannot
     * reach a different source. A build is handed over once — the caller owns it after that, and
     * the key leaves both tables with it. */
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
    Queue_.push_back(std::move(job));
    lock.unlock();
    Wake_.notify_one();
  } else {
    Repeats_++;
  }
  return Reply::Pending;
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

TilePool::Reply TilePool::Dag(int id, const float *soup, int nverts, int seamAttr, TileBuild *out) {
  Job job;
  job.Kind = Rank::Dag;
  job.Key = DagKey(id);
  job.SeamAttr = seamAttr;
  /* The soup is copied on the FIRST ask only: the caller repeats the id every frame until the ladder
   * lands, and a megabyte per frame on the frame thread is the cost this pool exists to remove. */
  if (!Known(job.Key)) job.Soup.assign(soup, soup + (size_t)nverts * 8);
  Result result;
  const Reply state = Poll(std::move(job), &result);
  if (state == Reply::Ready) *out = std::move(result.Build);
  return state;
}

} // namespace outshine::World
