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

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ClusterDag.h"

struct osmmesh_ctx;   /* one per thread, opaque here: the C island is a build detail */

namespace outshine::World {

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
   * out of memory is neither — it ends the run through core/Heap.h before it can reach here. */
  enum class Reply { Ready, Pending, Absent };

  /* WHAT THE POOL HAS DONE SO FAR, cumulative and never behind a switch: a bench is a declared run,
   * not a different code path, so this rides the ordinary telemetry row and the per-second delta is
   * the reader's subtraction. */
  struct Ledger {
    long MeshTiles = 0, MeshAbsent = 0, Dags = 0, Fetches = 0, FetchAbsent = 0, FetchGaveUp = 0;
    long Evictions = 0;
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

  Reply Mesh(int z, uint32_t x, uint32_t y, int grid, TileBuild *out);
  /* `id` names the job and is repeated until it is collected; the soup is copied on the first call.
   * `seamAttr` >= 0 names a float whose SIGN marks an attribute seam — two vertices at one position
   * that disagree in it weld as one point and stay two for drawing. -1 = no seam. */
  Reply Dag(int id, const float *soup, int nverts, int seamAttr, TileBuild *out);

  /* A path under the tile server's root, e.g. "/t/terrain/14/8620/5403". Non-blocking: a miss posts
   * a fetch and answers Pending. `out` keeps its capacity across calls. */
  Reply Bytes(const char *path, std::vector<uint8_t> *out);
  /* The same bytes, fetched on the CALLING thread when they are not resident. Legal only where the
   * caller may block: the pool's own threads, and natively the frame thread. */
  Reply BytesBlocking(const char *path, std::vector<uint8_t> *out);

  size_t ByteCacheBytes() const;
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

  void Work();
  void RunMesh(::osmmesh_ctx *ctx, const Job &job, Result *out);
  void RunDag(const Job &job, Result *out);
  Reply Poll(Job &&job, Result *out);
  bool Known(uint64_t key);
  double TileDistance(int z, uint32_t x, uint32_t y) const;

  /* Ready + a copy, or the cache's verdict. Never blocks. */
  Reply Lookup(const char *path, std::vector<uint8_t> *out);
  void Remember(const std::string &path, const uint8_t *data, size_t len, bool absent);
  Reply FetchInto(const char *path, std::vector<uint8_t> *out);

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

  std::mutex QueueMutex_;
  std::condition_variable Wake_;
  std::vector<Job> Queue_;
  std::map<uint64_t, Result> Done_;
  std::set<uint64_t> Posted_;
  double CameraLatDeg_ = 0.0, CameraLonDeg_ = 0.0;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
};

} // namespace outshine::World
#endif
