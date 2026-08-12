/* ONE TILE SCHEDULER, one byte cache, one in-flight cap — for both translations. Everything that
 * blocks (HTTP, PNG decode, mesh, cluster DAG) runs on this pool's threads; the frame thread only
 * posts and polls.
 *
 * The pool has two views of the same threads. The SIMULATION view hands out bytes — a Terrarium DEM
 * for the height oracle, an MVT for the classification — and the PICTURE view hands out geometry.
 * Both go through the same queue and the same cache, so a tile fetched for the mesh is the tile the
 * oracle reads.
 *
 * WHY THREADS AND NOT A SECOND WASM MODULE: the reason a Web Worker was needed died when the worker
 * began fetching for itself. A pthread IS a Web Worker, so a synchronous fetch on it blocks nothing
 * the browser draws with — and the scheduler stops existing twice in two languages. */
#ifndef TILEPOOL_H
#define TILEPOOL_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ClusterDag.h"

namespace outshine::World {

class TerrainTiles;

/* A tile of geometry as the renderer draws it: the cluster DAG, never the raw soup. */
struct TileBuild {
  std::vector<float> Verts;
  std::vector<uint32_t> Idx;
  std::vector<DagCluster> Clusters;
  double OriginEcef[3] = {0.0, 0.0, 0.0};
  float ErrM = 0.0f;
};

class TilePool {
public:
  /* THE THREE ANSWERS A STREAM CAN GIVE, and they are three because a fixed heap made two of them
   * indistinguishable: Absent is a decision the caller may cache, Pending is a promise, and running
   * out of memory is neither — it ends the run through core/io/Heap.h before it can reach here. */
  enum class Reply { Ready, Pending, Absent };

  /* WHAT THE POOL HAS DONE SO FAR, cumulative and never behind a switch: a bench is a declared run,
   * not a different code path, so this rides the ordinary telemetry row and the per-second delta is
   * the reader's subtraction. */
  struct Ledger {
    /* 64 bits, never `long`: the frame is wasm32, where a `long` counter wraps at 2^31 — and the
     * ask counters below reach that inside a day of streaming. */
    long long MeshTiles = 0, MeshAbsent = 0, Dags = 0, Fetches = 0, FetchAbsent = 0, FetchGaveUp = 0;
    long long Evictions = 0;
    /* THE TWO REFUSAL COUNTS, kept apart from the absences beside them because that is the whole
     * distinction: an absence is the world, a refusal is this tree or the wire. A run whose terrain
     * is coarse and whose MeshRefused is zero has a different cause from one where it is not. */
    long long FetchRefused = 0, MeshRefused = 0;
    /* WHAT THE ASKS DID TO THE QUEUE. Posts is a job that started; Repeats is an ask that found its
     * job already under way, which is the caller waiting on the threads rather than on nothing.
     * QueueDepth is the one GAUGE here — a queue length has no cumulative form, and it is what
     * tells a queue stacked behind the in-flight cap (InFlightCap()) from one thread being slow. */
    long long Posts = 0, Repeats = 0, QueueDepth = 0;
    /* FetchMs is the wall inside HttpGet; FetchBlockedMs is the whole span inside a fetch, so the
     * difference is what the flat 202 retry cost. MeshCpuMs has that span taken out of it: a build
     * does not do the transport it waits on, and a column that can silently carry one is a column
     * that will. */
    double FetchMs = 0.0, FetchBlockedMs = 0.0, MeshCpuMs = 0.0, DagMs = 0.0;
    double FetchedMB = 0.0;
  };
  Ledger Counters() const;

  struct Config {
    std::string TilesBase;
    double OriginLatDeg = 0.0;
    double OriginLonDeg = 0.0;
    int Threads = 1;
    size_t ByteBudget = 0;
    /* Decoded source grids each thread holds against its next stitch. */
    int DemCacheTiles = 0;
  };

  /* The threads run when this returns: a pool that starts lazily starts on the frame thread. */
  explicit TilePool(const Config &config);
  ~TilePool();
  TilePool(const TilePool &) = delete;
  TilePool &operator=(const TilePool &) = delete;

  /* The order the queue is drained in: nearest tile first, at the camera's current position. */
  void Camera(double latDeg, double lonDeg);

  [[nodiscard]] Reply Mesh(int z, uint32_t x, uint32_t y, int grid, TileBuild *out);
  /* `id` names the job and is repeated until it is collected; the soup is copied on the first call.
   * `seamAttr` >= 0 names a float whose SIGN marks an attribute seam — two vertices at one position
   * that disagree in it weld as one point and stay two for drawing. -1 = no seam. */
  [[nodiscard]] Reply Dag(int id, const float *soup, int nverts, int seamAttr, TileBuild *out);
  /* THE CALLER LETTING GO. Poll hands a finished build over exactly once and to whoever asks, so a
   * mesh whose asker stopped asking — a retracted split takes a sibling out mid-build, an eviction
   * takes a leaf out of the cut — is held for the life of the pool with nothing left that could
   * collect it. Also cancels the in-flight job's product: the completion writes nothing once the key
   * is no longer posted. */
  void ForgetMesh(int z, uint32_t x, uint32_t y);

  /* A path under the tile server's root, e.g. "/t/terrain/14/8620/5403". Non-blocking: a miss posts
   * a fetch and answers Pending. `out` keeps its capacity across calls. */
  [[nodiscard]] Reply Bytes(const char *path, std::vector<uint8_t> *out);
  /* The same bytes, fetched on the CALLING thread when they are not resident. Legal only where the
   * caller may block: the pool's own threads, and natively the frame thread. */
  [[nodiscard]] Reply BytesBlocking(const char *path, std::vector<uint8_t> *out);

  /* The whole table, not just the budgeted payload: the paths and the row vector are held for as
   * long as the bytes are. */
  size_t ByteCacheBytes() const;
  /* Decoded DEM grids the pool's threads hold. Each thread publishes its own after every job —
   * reading another thread's context directly would race the build that is writing it. */
  size_t DemCacheBytes() const;
  /* The queue, the posted set and the finished results waiting to be collected. A completed mesh
   * sits here holding its vertices until the frame thread comes for it. */
  size_t SchedulerBytes() const;
  int ThreadCount() const { return (int)Threads_.size(); }
  /* HTTP requests that can be outstanding at once. It is the thread count because every request is
   * made by a thread that is waiting on it. */
  int InFlightCap() const { return (int)Threads_.size(); }

private:
  /* Ascending: the queue drains by (Rank, tile distance), and this enumeration IS the priority. A
   * DAG's soup is already decoded and a frame is waiting on it; a byte fetch blocks whatever asked
   * for it; a mesh is what the skirts cover in the meantime. */
  enum class Rank { Dag = 0, Fetch = 1, Mesh = 2 };

  struct Job {
    Rank Kind = Rank::Mesh;
    int Z = 0, Grid = 0, SeamAttr = -1;
    uint32_t X = 0, Y = 0;
    uint64_t Key = 0;
    double TileDist = 0.0;
    std::string Path;
    std::vector<float> Soup;
  };

  struct Result {
    Reply State = Reply::Pending;
    TileBuild Build;
  };

  struct CacheEntry {
    std::string Path;
    std::vector<uint8_t> Data;
    bool Absent = false;
    uint64_t Used = 0;
  };

  void Work(int slot);
  void RunMesh(TerrainTiles &tiles, const Job &job, Result *out);
  void RunDag(const Job &job, Result *out);
  [[nodiscard]] Reply Poll(Job &&job, Result *out);
  [[nodiscard]] bool Known(uint64_t key);
  double TileDistance(int z, uint32_t x, uint32_t y) const;

  /* Ready + a copy, or the cache's verdict. Never blocks. */
  [[nodiscard]] Reply Lookup(const char *path, std::vector<uint8_t> *out);
  void Remember(const std::string &path, const uint8_t *data, size_t len, bool absent);
  [[nodiscard]] Reply FetchInto(const char *path, std::vector<uint8_t> *out);

  const std::string Base_;
  const double OriginLatDeg_, OriginLonDeg_;
  const size_t ByteBudget_;
  const int DemCacheTiles_;

  mutable std::mutex CacheMutex_;
  std::vector<CacheEntry> Cache_;
  size_t CacheBytes_ = 0;
  uint64_t CacheClock_ = 0;

  mutable std::mutex LedgerMutex_;
  Ledger Ledger_;

  /* One slot per thread, written only by that thread. */
  std::vector<std::atomic<size_t>> ContextBytes_;

  mutable std::mutex QueueMutex_;
  std::condition_variable Wake_;
  std::vector<Job> Queue_;
  std::map<uint64_t, Result> Done_;
  std::set<uint64_t> Posted_;
  /* 64 bits for the same reason the Ledger's are, and these are the ones that reach it: Repeats_
   * rises on every ask that finds its job already under way, measured at ~190 kHz. */
  long long Posts_ = 0, Repeats_ = 0;   /* written only under QueueMutex_, beside the set they count */
  double CameraLatDeg_ = 0.0, CameraLonDeg_ = 0.0;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
};

} // namespace outshine::World
#endif
