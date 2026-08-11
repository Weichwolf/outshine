#include "TilePool.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "Capacity.h"
#include "ChunkMesh.h"
#include "Heap.h"
#include "Log.h"
#include "StackProbe.h"
#include "osmmesh.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#else
#include <curl/curl.h>
#endif

namespace outshine::World {

namespace {

constexpr int kProviderTerrainMaxZ = 15;   /* fb-tiles' terrarium source stops here (tiles/src/tilesrc.c) */
const char *const kKindPath[3] = {"vector", "terrain", "imagery"};

/* [SET] How long one thread keeps asking before it hands the tile back to the queue: 60 tries at
 * 50 ms = 3 s. Flat, not backed off, because 202 is fb-tiles saying "I am fetching this upstream
 * right now" and the wait is the upstream round trip, not congestion — a backoff would sleep
 * through the arrival. It is a ceiling on how long a THREAD is held, not on how often a tile is
 * asked for: the caller re-posts, so a slow server delays terrain instead of removing it. */
constexpr int kFetchAttempts = 60;
constexpr int kRetryMs = 50;

/* THE ONE STATUS CONTRACT, and fb-tiles/src/main.c is what it reads: 200 hands over bytes, 204 is a
 * hole the caller may cache, 202 means the server is fetching upstream.
 *
 * The split below the 200s is RFC 9110's own: a 4xx (§15.5) says the REQUEST is wrong and repeating
 * it unchanged changes nothing — 408 and 429 (§15.5.9, §15.5.30) are the two that name a retry. A 5xx
 * (§15.6) and a transport failure are the server or the wire, and those do heal. fb-tiles 404s an
 * unknown route, so without this split one mistyped path is every thread issuing a GET every 50 ms
 * against a world that can never arrive.
 *
 * REFUSED IS NOT ABSENT, and that is the whole reason for the fourth answer: 204 is the server
 * making a statement about the WORLD, while a 403 from something between here and it is a statement
 * about the request. Cached as absent, one proxy error during a deploy leaves that quadrant coarse
 * for the life of the process with nothing in the record to trace it to. */
enum class Status { Bytes, Hole, Refused, Again };

Status Classify(int httpStatus, size_t len) {
  if (httpStatus == 200) return len > 0 ? Status::Bytes : Status::Again;
  if (httpStatus == 204) return Status::Hole;
  if (httpStatus >= 400 && httpStatus < 500 && httpStatus != 408 && httpStatus != 429)
    return Status::Refused;
  return Status::Again;
}

/* WHAT THIS THREAD HAS SPENT INSIDE FetchInto, cumulative, in ms of wall: transport AND the flat
 * retry sleeps above, which is the span the HttpGet clock cannot see. A mesh job subtracts its own
 * share of it, because a build that waited on a 202 did not spend that time building. */
thread_local double tFetchBlockedMs = 0.0;

/* WHY A MESH WAS NOT BUILT, and only ONE of these is a statement about the world: a Hole is the tile
 * server's own 204 for this tile's terrain. A Refused is this tree, the request or the wire being
 * wrong — a 4xx that is not the 204 contract, a PNG that will not decode, a source that is not the
 * grid the stitch promises, a partition with no cluster in it. A Wait is a promise not yet kept.
 *
 * The C island answers all four with no bytes by contract (terrain/osmmesh.h), so the reason travels
 * beside the return code on a thread-local: the provider callback has no channel back. It matters
 * because Absent is TERMINAL at the node (world/World.h MeshState::Vacant) — a rung retracted for a
 * proxy error or a slow server deletes ground for good.
 *
 * WORST WINS, and Refused outranks Wait on purpose: a refusal is minted inside a fetch that then
 * reports Pending to the provider, so the other order would erase every name this exists to keep. */
enum class Miss { None, Hole, Wait, Refused };
thread_local Miss tMiss = Miss::None;
Miss Worse(Miss a, Miss b) { return a > b ? a : b; }

uint64_t MeshKey(int z, uint32_t x, uint32_t y) {
  return ((uint64_t)1 << 62) | ((uint64_t)(z & 31) << 56)
       | ((uint64_t)(x & 0xFFFFFFFu) << 28) | (uint64_t)(y & 0xFFFFFFFu);
}
uint64_t DagKey(int id) { return ((uint64_t)2 << 62) | (uint64_t)(uint32_t)id; }
uint64_t PathKey(const char *path) {   /* FNV-1a: the queue keys on a number, the cache on the text */
  uint64_t h = 1469598103934665603ull;
  for (const char *p = path; *p; p++) h = (h ^ (uint64_t)(uint8_t)*p) * 1099511628211ull;
  return ((uint64_t)3 << 62) | (h & 0x3FFFFFFFFFFFFFFFull);
}

/* The one thing this layer knows about a soup: WHICH float carries the seam. What the vertices mean
 * stays with the caller that built them, so the predicate is a table and not a closure. */
template <int A> int SeamAt(const float *v) { return v[A] < 0.0f ? 1 : 0; }
int (*const kSeam[8])(const float *) = {SeamAt<0>, SeamAt<1>, SeamAt<2>, SeamAt<3>,
                                        SeamAt<4>, SeamAt<5>, SeamAt<6>, SeamAt<7>};

int Provider(void *user, osmmesh_tile_kind kind, uint32_t z, uint32_t x, uint32_t y,
             uint8_t **out, size_t *len);

#ifdef __EMSCRIPTEN__
/* WHAT JAVASCRIPT TAKES OUT OF THIS MODULE'S FIXED LINEAR MEMORY, one entry point and one item: a
 * fixed heap turns a refusal into the run's last event, and the event has to say what was being
 * taken. Not inside the anonymous namespace — the linker resolves it by this exact name. */
}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void *fb_take_http_body(int bytes) {
  return Heap::Take("http body", (size_t)bytes);
}

namespace {

/* The byte primitive, and the ONLY JavaScript this module contains. A synchronous XMLHttpRequest is
 * legal on a Web Worker and a pthread IS one, so the thread that waits is never the thread that
 * draws — which is the whole reason the pool no longer needs a module of its own. */
EM_JS(int, FbSyncGet, (const char *url, uint8_t **outPtr, uint32_t *outLen), {
  try {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', UTF8ToString(url), false);
    xhr.responseType = 'arraybuffer';
    xhr.send(null);
    if (xhr.status !== 200 || !xhr.response) return xhr.status | 0;
    var b = new Uint8Array(xhr.response);
    var p = _fb_take_http_body(b.length);
    HEAPU8.set(b, p);
    HEAPU32[outPtr >> 2] = p;
    HEAPU32[outLen >> 2] = b.length;
    return 200;
  } catch (e) { return -1; }
})

int HttpGet(const char *url, std::vector<uint8_t> *out) {
  uint8_t *body = nullptr;
  uint32_t len = 0;
  const int status = FbSyncGet(url, &body, &len);
  if (status == 200 && body && len > 0) out->assign(body, body + len);
  std::free(body);
  return status == 200 && len == 0 ? 0 : status;
}
#else
struct CurlSink { std::vector<uint8_t> *Out; };

size_t CurlWrite(void *ptr, size_t size, size_t members, void *user) {
  const size_t n = size * members;
  std::vector<uint8_t> *out = ((CurlSink *)user)->Out;
  out->insert(out->end(), (const uint8_t *)ptr, (const uint8_t *)ptr + n);
  return n;
}

int HttpGet(const char *url, std::vector<uint8_t> *out) {
  CURL *c = curl_easy_init();
  if (!c) return -1;
  CurlSink sink{out};
  curl_easy_setopt(c, CURLOPT_URL, url);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, CurlWrite);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
  const CURLcode res = curl_easy_perform(c);
  long status = 0;
  if (res == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(c);
  return res == CURLE_OK ? (int)status : -1;
}
#endif

}  // namespace

TilePool::TilePool(const Config &config)
    : Base_(config.TilesBase),
      OriginLatDeg_(config.OriginLatDeg),
      OriginLonDeg_(config.OriginLonDeg),
      ByteBudget_(config.ByteBudget),
      DemCacheTiles_(config.DemCacheTiles),
      CameraLatDeg_(config.OriginLatDeg),
      CameraLonDeg_(config.OriginLonDeg) {
#ifndef __EMSCRIPTEN__
  curl_global_init(CURL_GLOBAL_DEFAULT);   /* curl_easy_init's lazy global init is not thread-safe */
#endif
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
              {"httpGets", l.Fetches}, {"httpAbsent", l.FetchAbsent},
              {"httpRefused", l.FetchRefused},
              {"httpGaveUp", l.FetchGaveUp}, {"httpMs", l.FetchMs},
              {"httpMsPerGet", l.FetchMs / fetches},
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
  for (const CacheEntry &e : Cache_) bytes += e.Path.capacity() + CapacityBytes(e.Data);
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
  for (const Job &j : Queue_) bytes += j.Path.capacity() + CapacityBytes(j.Soup);
  bytes += TreeNodeBytes<uint64_t>(Posted_.size());
  bytes += TreeNodeBytes<std::pair<const uint64_t, Result>>(Done_.size());
  for (const std::pair<const uint64_t, Result> &d : Done_)
    bytes += CapacityBytes(d.second.Build.Verts) + CapacityBytes(d.second.Build.Idx) +
             CapacityBytes(d.second.Build.Clusters);
  return bytes;
}

TilePool::Reply TilePool::Lookup(const char *path, std::vector<uint8_t> *out) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  for (CacheEntry &e : Cache_) {
    if (e.Path != path) continue;
    e.Used = ++CacheClock_;
    if (e.Absent) return Reply::Absent;
    out->assign(e.Data.begin(), e.Data.end());
    return Reply::Ready;
  }
  return Reply::Pending;
}

/* LEAST RECENTLY USED, by bytes. The working set is the ring around the camera and it moves with
 * it, so a cache with no eviction is a leak with a slow fuse. Nothing hands out a pointer into this
 * table — every reader gets a copy — so an eviction cannot pull the ground out from under one. */
void TilePool::Remember(const std::string &path, const uint8_t *data, size_t len, bool absent) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  for (CacheEntry &e : Cache_)
    if (e.Path == path) return;
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
  e.Path = path;
  e.Absent = absent;
  e.Used = ++CacheClock_;
  if (!absent && len > 0) e.Data.assign(data, data + len);
  CacheBytes_ += e.Data.size();
  Cache_.push_back(std::move(e));
}

TilePool::Reply TilePool::FetchInto(const char *path, std::vector<uint8_t> *out) {
#ifdef __EMSCRIPTEN__
  /* A browser's frame thread has no synchronous transport at all, so this is not a stall to be
   * measured later but a call that cannot return bytes: it is the simulation view that belongs
   * there, and reaching here from it is a mistake about which view the caller holds. */
  if (emscripten_is_main_browser_thread()) {
    Log::Error("world", "tilepool_blocking_on_frame_thread", {{"path", std::string(path)}});
    std::abort();
  }
#endif
  std::string url = Base_;
  url += path;
  Reply reply = Reply::Pending;
  bool refused = false;
  double httpMs = 0.0;
  long gets = 0;
  const auto entered = std::chrono::steady_clock::now();
  for (int attempt = 0; attempt < kFetchAttempts && reply == Reply::Pending && !refused; attempt++) {
    out->clear();
    const auto t0 = std::chrono::steady_clock::now();
    const int status = HttpGet(url.c_str(), out);
    httpMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    gets++;
    switch (Classify(status, out->size())) {
      case Status::Bytes:
        Remember(path, out->data(), out->size(), false);
        reply = Reply::Ready;
        break;
      case Status::Hole:
        Remember(path, nullptr, 0, true);
        reply = Reply::Absent;
        break;
      case Status::Refused:
        /* NOT remembered and NOT Absent. Repeating the request unchanged cannot help, so the loop
         * ends — but the answer the caller gets is Pending, because the one thing that must not be
         * cached under a wrong request is a decision about the world. */
        Log::Error("world", "tile_refused", {{"path", std::string(path)}, {"status", status}});
        refused = true;
        break;
      case Status::Again:
        usleep((useconds_t)kRetryMs * 1000);
        break;
    }
  }
  if (refused) tMiss = Worse(tMiss, Miss::Refused);
  const double blockedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entered).count();
  tFetchBlockedMs += blockedMs;
  {
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.Fetches += gets;
    Ledger_.FetchMs += httpMs;
    Ledger_.FetchBlockedMs += blockedMs;
    if (reply == Reply::Ready) Ledger_.FetchedMB += (double)out->size() / 1048576.0;
    if (reply == Reply::Absent) Ledger_.FetchAbsent++;
    if (refused) Ledger_.FetchRefused++;
    else if (reply == Reply::Pending) Ledger_.FetchGaveUp++;
  }
  if (reply == Reply::Pending) out->clear();   /* NOT remembered: the next ask starts the clock again */
  return reply;
}

TilePool::Reply TilePool::Bytes(const char *path, std::vector<uint8_t> *out) {
  const Reply resident = Lookup(path, out);
  if (resident != Reply::Pending) return resident;
  Job job;
  job.Kind = Rank::Fetch;
  job.Key = PathKey(path);
  job.Path = path;
  Result result;
  const Reply posted = Poll(std::move(job), &result);
  if (posted == Reply::Pending) return Reply::Pending;
  return Lookup(path, out);   /* the fetch's product is the cache entry, not the job's result */
}

TilePool::Reply TilePool::BytesBlocking(const char *path, std::vector<uint8_t> *out) {
  const Reply resident = Lookup(path, out);
  if (resident != Reply::Pending) return resident;
  return FetchInto(path, out);
}

namespace {

int Provider(void *user, osmmesh_tile_kind kind, uint32_t z, uint32_t x, uint32_t y,
             uint8_t **out, size_t *len) {
  *out = nullptr;
  *len = 0;
  if ((int)kind < 0 || (int)kind >= 3) return 0;
  char path[128];
  std::snprintf(path, sizeof path, "/t/%s/%u/%u/%u", kKindPath[(int)kind], z, x, y);
  std::vector<uint8_t> bytes;
  const TilePool::Reply reply = ((TilePool *)user)->BytesBlocking(path, &bytes);
  if (reply == TilePool::Reply::Pending) tMiss = Worse(tMiss, Miss::Wait);
  if (reply != TilePool::Reply::Ready || bytes.empty()) return 0;
  /* osmmesh frees what a provider hands over, so this crosses into the C island as a plain block. */
  uint8_t *block = (uint8_t *)Heap::Take("tile bytes", bytes.size());
  std::memcpy(block, bytes.data(), bytes.size());
  *out = block;
  *len = bytes.size();
  return 1;
}

}  // namespace

void TilePool::RunMesh(::osmmesh_ctx *ctx, const Job &job, Result *out) {
  osmmesh_tile tile = {};
  tMiss = Miss::None;
  const int fetched = osmmesh_fetch_tile(ctx, (uint8_t)job.Z, job.X, job.Y, &tile);
  /* A REFUSED ALLOCATION IS NOT A GAP. The C island answers both with a negative int, and under a
   * fixed linear memory that is the difference between "this tile has no DEM" and "the run is
   * over" — so it is separated here and nowhere else. */
  if (fetched == OSMMESH_ERR_OOM) Heap::Exhausted("tile dem grid");
  Chunk chunk = {};
  double origin[3] = {0.0, 0.0, 0.0};
  const char *stage = "fetch";
  Miss miss = Miss::None;
  if (fetched != OSMMESH_OK) miss = Miss::Refused;
  else if (!tile.terrain) miss = Miss::Hole;
  else if (!ChunkBuildEcef(tile.terrain, job.Z, job.X, job.Y, job.Grid, &chunk, origin) ||
           chunk.nverts <= 0) {
    miss = Miss::Refused;
    stage = "grid";
  }
  osmmesh_free_tile(&tile);
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
  /* THE STITCH READS FOUR NEIGHBOURS, so this tile's own answer is not the only one that decides:
   * a mesh built while any of them was still coming is a seam frozen into a node that is never asked
   * again, and a neighbour the wire refused is not a statement about this tile's ground either. */
  if (tMiss > miss) {
    miss = tMiss;
    stage = "provider";
  }
  if (miss == Miss::None) {
    out->State = Reply::Ready;
    return;
  }
  if (miss == Miss::Refused) {
    Log::Warn("world", "tile_mesh_refused", {{"z", job.Z}, {"x", (int)job.X}, {"y", (int)job.Y},
                                             {"stage", stage}, {"rc", fetched}});
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.MeshRefused++;
  }
  /* ONE DOOR TO A TERMINAL ANSWER, and only a Hole goes through it. */
  out->State = miss == Miss::Hole ? Reply::Absent : Reply::Pending;
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

/* EACH THREAD OWNS ITS OWN osmmesh_ctx: the context carries a decoded-DEM cache that the tile being
 * built writes, so sharing one would need a lock around the whole build and the pool would be a
 * queue. The byte cache below it IS shared and hands out copies only. */
void TilePool::Work(int slot) {
  StackProbe::Enter(StackProbe::Purpose::Tile);
  osmmesh_config cfg = {};
  cfg.origin_lat = OriginLatDeg_;
  cfg.origin_lon = OriginLonDeg_;
  cfg.tile_provider = Provider;
  cfg.tile_provider_user = this;
  cfg.provider_terrain_max_zoom = kProviderTerrainMaxZ;
  cfg.dem_cache_tiles = DemCacheTiles_;
  cfg.enable_terrain = 1;
  osmmesh_ctx *ctx = nullptr;
  const int created = osmmesh_create(&cfg, &ctx);
  /* A THREAD WITHOUT A CONTEXT IS WORSE THAN NO THREAD: it still takes mesh jobs off the queue and
   * leaves them Pending, so the caller re-posts for ever and the terrain never arrives at full CPU. */
  if (created == OSMMESH_ERR_OOM) Heap::Exhausted("tile mesh context");
  if (created != OSMMESH_OK) {
    Log::Error("world", "tilepool_context_unopenable", {{"rc", created}});
    std::abort();
  }

  ContextBytes_[(size_t)slot].store(osmmesh_ctx_heap_bytes(ctx), std::memory_order_relaxed);

  std::vector<uint8_t> scratch;
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
        RunMesh(ctx, job, &result);
        break;
      case Rank::Dag:
        RunDag(job, &result);
        break;
      case Rank::Fetch:
        result.State = FetchInto(job.Path.c_str(), &scratch);
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
    ContextBytes_[(size_t)slot].store(osmmesh_ctx_heap_bytes(ctx), std::memory_order_relaxed);
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
        if (result.State == Reply::Absent) result.Build = TileBuild{};
        Done_[job.Key] = std::move(result);
      }
    }
  }
  osmmesh_destroy(ctx);
  ContextBytes_[(size_t)slot].store(0, std::memory_order_relaxed);
}

TilePool::Reply TilePool::Poll(Job &&job, Result *out) {
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const auto done = Done_.find(job.Key);
  if (done != Done_.end()) {
    /* An Absent is KEPT, posted and done: the answer is final, so every later ask gets the same one
     * instead of a thread spun on a tile that does not exist. A build is handed over once — the
     * caller owns it after that, and the key leaves both tables with it. */
    if (done->second.State == Reply::Absent) {
      out->State = Reply::Absent;
      return Reply::Absent;
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
