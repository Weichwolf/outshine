#ifndef TILEPOOL_H
#define TILEPOOL_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ClusterDag.h"
#include "Request.h"

namespace outshine::Data {
class SourceSet;
class Transport;
}

namespace outshine::Ground {

class TerrainTiles;

struct TileBuild {
  std::vector<float> Verts;
  std::vector<uint32_t> Idx;
  std::vector<DagCluster> Clusters;
  double OriginEcef[3] = {0.0, 0.0, 0.0};
  float ErrM = 0.0f;
};

class TilePool {
public:

  enum class Reply { Ready, Pending, Absent, Refused, Undeclared };

  struct Ledger {

    long long MeshTiles = 0, MeshAbsent = 0, Dags = 0, Fetches = 0, FetchAbsent = 0, FetchGaveUp = 0;
    long long Evictions = 0;

    long long FetchRefused = 0, MeshRefused = 0;

    long long Posts = 0, Repeats = 0, QueueDepth = 0;

    double FetchMs = 0.0, FetchBlockedMs = 0.0, MeshCpuMs = 0.0, DagMs = 0.0;
    double FetchedMB = 0.0;
  };
  Ledger Counters() const;

  struct Config {
    double OriginLatDeg = 0.0;
    double OriginLonDeg = 0.0;
    int Threads = 1;
    size_t ByteBudget = 0;

    int DemCacheTiles = 0;
  };

  TilePool(const Config &config, Data::SourceSet &sources, Data::Transport &transport);
  ~TilePool();
  TilePool(const TilePool &) = delete;
  TilePool &operator=(const TilePool &) = delete;

  void Focus(double latDeg, double lonDeg);

  [[nodiscard]] Reply Mesh(int z, uint32_t x, uint32_t y, int grid, TileBuild *out);

  [[nodiscard]] Reply Dag(int id, const float *soup, int nverts, int seamAttr, TileBuild *out);

  void ForgetMesh(int z, uint32_t x, uint32_t y);

  struct Landing {
    std::vector<uint8_t> Bytes;
    Data::Address At = Data::Address::Whole(0);
  };

  [[nodiscard]] Reply Bytes(const Data::Request &request, Landing *out);

  [[nodiscard]] Reply BytesBlocking(const Data::Request &request, Landing *out);

  size_t ByteCacheBytes() const;

  size_t DemCacheBytes() const;

  size_t SchedulerBytes() const;
  int ThreadCount() const { return (int)Threads_.size(); }

  int InFlightCap() const { return (int)Threads_.size(); }

private:

  enum class Rank { Dag = 0, Fetch = 1, Mesh = 2 };

  struct Job {
    Rank Kind = Rank::Mesh;
    int Z = 0, Grid = 0, SeamAttr = -1;
    uint32_t X = 0, Y = 0;
    uint64_t Key = 0;
    double TileDist = 0.0;

    std::optional<Data::Request> Ask;
    std::vector<float> Soup;
  };

  struct Result {
    Reply State = Reply::Pending;
    TileBuild Build;
  };

  struct CacheEntry {
    std::string Key;
    std::vector<uint8_t> Data;

    Data::Address At = Data::Address::Whole(0);
    bool Absent = false;
    uint64_t Used = 0;
  };

  void Work(int slot);
  void RunMesh(TerrainTiles &tiles, const Job &job, Result *out);
  void RunDag(const Job &job, Result *out);
  [[nodiscard]] Reply Poll(Job &&job, Result *out);
  [[nodiscard]] bool Known(uint64_t key);
  double TileDistance(int z, uint32_t x, uint32_t y) const;

  [[nodiscard]] Reply Lookup(const std::string &key, Landing *out);
  void Remember(const std::string &key, const uint8_t *data, size_t len, const Data::Address &at,
                bool absent);
  [[nodiscard]] Reply FetchInto(const Data::Request &request, Landing *out);

  Data::SourceSet &Sources_;
  Data::Transport &Wire_;
  const double OriginLatDeg_, OriginLonDeg_;
  const size_t ByteBudget_;
  const int DemCacheTiles_;

  mutable std::mutex CacheMutex_;
  std::vector<CacheEntry> Cache_;
  size_t CacheBytes_ = 0;
  uint64_t CacheClock_ = 0;

  mutable std::mutex LedgerMutex_;
  Ledger Ledger_;

  std::vector<std::atomic<size_t>> ContextBytes_;

  mutable std::mutex QueueMutex_;
  std::condition_variable Wake_;
  std::vector<Job> Queue_;
  std::map<uint64_t, Result> Done_;
  std::set<uint64_t> Posted_;

  long long Posts_ = 0, Repeats_ = 0;
  double FocusLatDeg_ = 0.0, FocusLonDeg_ = 0.0;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
};

}
#endif
