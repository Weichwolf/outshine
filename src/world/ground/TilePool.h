#ifndef OUTSHINE_WORLD_GROUND_TILEPOOL_H
#define OUTSHINE_WORLD_GROUND_TILEPOOL_H

#include <atomic>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "Earth.h"
#include "ClusterDag.h"
#include "FlatMap.h"
#include "TerrainGrid.h"
#include "TerrainTiles.h"
#include "TileMeshes.h"
#include "Fetch.h"

namespace outshine {
class LogSink;
}

namespace outshine::Data {
class SourceSet;
class Transport;
}

namespace outshine::Ground {

class TerrainTiles;

struct ShapedGround {
  std::string Kind;
  double AmplitudeM = 0.0;
  double WavelengthM = 0.0;
  double Gradient = 0.0;
  double BearingDeg = 0.0;
  double FocusLatDeg = 0.0;
  double FocusLonDeg = 0.0;
  uint64_t Seed = 0;
};

using outshine::TileBuild;

class TilePool : public TileMeshes {
public:
  using TileMeshes::Reply;

  void Shapes(const ShapedGround &how);

  [[nodiscard]] ShapedGround Shaped() const;

  struct Ledger {
    long long MeshTiles = 0, MeshAbsent = 0, Fetches = 0, FetchAbsent = 0, FetchGaveUp = 0;
    long long Evictions = 0;

    long long FetchRefused = 0, MeshRefused = 0;

    long long Posts = 0, Repeats = 0, AdmissionDeferred = 0, QueueDepth = 0;
    long long Outstanding = 0, Parked = 0, ParkedJobs = 0, Held = 0;
    long long MeshDeferred = 0, MeshDropped = 0;

    double FetchMs = 0.0, FetchBlockedMs = 0.0, MeshCpuMs = 0.0, FieldCpuMs = 0.0;
    long long FieldTiles = 0, FieldDropped = 0;
    long FetchOnCompute = 0;
    double FetchedMB = 0.0;
  };

  Ledger Counters() const;

  struct Config {
    double OriginLatDeg = 0.0;
    double OriginLonDeg = 0.0;
    int Threads = 1;
    size_t ByteBudget = 0;

    size_t DecodedBytes = 0;

    int PollAttempts = 0;

    int Carriers = 0;

    size_t OutstandingMost = 2048;

    LogSink *Diagnostics = nullptr;
  };

  TilePool(const Config &config, Data::SourceSet &sources, Data::Transport &transport);
  ~TilePool() override;
  TilePool(const TilePool &) = delete;
  TilePool &operator=(const TilePool &) = delete;

  void Focus(LongitudeLatitude at);

  [[nodiscard]] Reply Mesh(Data::TileId of, int grid, TileBuild *out) override;
  [[nodiscard]] Reply Wants(Data::TileId of, int grid) override;

  [[nodiscard]] Reply MeshAwaited(Data::TileId of, int grid, TileBuild *out) override;

  [[nodiscard]] Reply Field(Data::TileId of, std::shared_ptr<const TerrainField> *out);

  void ForgetMesh(int z, uint32_t x, uint32_t y);

  struct Landing {
    std::vector<uint8_t> Bytes;
    std::string SourceId;
    std::string SourceRevision;
    Data::Address At = Data::Address::Whole(0);
  };

  [[nodiscard]] Reply Bytes(const Data::Fetch &request, Landing *out);

  [[nodiscard]] Reply BytesBlocking(const Data::Fetch &request, Landing *out);

  [[nodiscard]] bool Carries() const { return !Carriers_.empty(); }

  size_t ByteCacheBytes() const;

  size_t DemCacheBytes() const;

  size_t SchedulerBytes() const;

  [[nodiscard]] size_t ResidentBytes() const;

  int ThreadCount() const { return static_cast<int>(Threads_.size()); }

  int InFlightCap() const { return static_cast<int>(Threads_.size()); }

  [[nodiscard]] bool AwaitLanding(double seconds);

private:
  enum class Rank { Fetch = 0, Mesh = 1, Field = 2 };

  struct Job {
    Rank Kind = Rank::Mesh;
    int Z = 0, Grid = 0;
    uint32_t X = 0, Y = 0;
    uint64_t Key = 0;
    double TileDist = 0.0;

    std::optional<Data::Fetch> Ask;
  };

  struct Result {
    Reply State = Reply::Pending;
    TileBuild Build;
    std::shared_ptr<const TerrainField> Field;
    Landing Landed;
    bool Holds = false;
  };

  struct CacheEntry {
    std::string Key;
    std::vector<uint8_t> Data;
    std::string SourceId;
    std::string SourceRevision;

    Data::Address At = Data::Address::Whole(0);
    bool Absent = false;
    double RefusedUntilMs = 0.0;
    uint64_t Used = 0;
  };

  struct CacheIndex {
    uint64_t Digest = 0;
    size_t Entry = 0;
  };

  struct CacheEntryMove {
    size_t From = 0;
    size_t To = 0;
  };

  template <size_t Capacity> struct RecentKeys {
    static_assert(Capacity > 0);

    [[nodiscard]] std::optional<uint64_t> Push(uint64_t key) noexcept {
      if (Count < Capacity) {
        Keys[(First + Count) % Capacity] = key;
        ++Count;
        return std::nullopt;
      }
      const uint64_t oldest = Keys[First];
      Keys[First] = key;
      First = (First + 1u) % Capacity;
      return oldest;
    }

    [[nodiscard]] bool Holds(uint64_t key) const noexcept {
      for (size_t at = 0; at < Count; ++at) {
        if (Keys[(First + at) % Capacity] == key) { return true; }
      }
      return false;
    }

    std::array<uint64_t, Capacity> Keys{};
    size_t First = 0;
    size_t Count = 0;
  };

  [[nodiscard]] std::optional<Job> NextJob();
  [[nodiscard]] Result RunJob(TerrainTiles &tiles, const Job &job);
  void PublishResult(const Job &job, Result result);
  void Work(int slot);
  void Carry();
  void RunMesh(TerrainTiles &tiles, const Job &job, Result *out);
  static void RunField(TerrainTiles &tiles, const Job &job, Result *out);

  ShapedGround Shape_;
  [[nodiscard]] Reply Poll(const Job &job, Result *out);
  void Lands(uint64_t key, bool holds);
  [[nodiscard]] bool Known(uint64_t key);
  double TileDistance(Data::TileId of) const;

  [[nodiscard]] Reply Lookup(const std::string &key, Landing *out);
  void RefuseUntil(const std::string &key, double untilMs);
  void Remember(const std::string &key,
                const uint8_t *data,
                size_t len,
                const Data::Address &at,
                std::string_view sourceId,
                std::string_view sourceRevision,
                bool absent);
  [[nodiscard]] Reply FetchInto(const Data::Fetch &request, Landing *out);
  [[nodiscard]] std::optional<size_t> CacheEntryOf(std::string_view key) const;
  void IndexCacheEntry(std::string_view key, size_t entry);
  void EraseCacheEntry(std::string_view key, size_t entry);
  void RepointCacheEntry(CacheEntryMove move) noexcept;
  [[nodiscard]] bool StoresDone(uint64_t key, Result result);
  [[nodiscard]] Reply PublishesCarried(const Job &job, Result result);
  void DeferredAdmission();

  Data::SourceSet &Sources_;
  Data::Transport &Wire_;
  const double OriginLatDeg_, OriginLonDeg_;
  const size_t ByteBudget_;
  std::shared_ptr<Ground::DecodedCache> Decoded_;
  const int PollAttempts_;
  const int CarrierCount_;
  const size_t OutstandingMost_;
  LogSink *const Diagnostics_;

  mutable std::mutex CacheMutex_;
  std::vector<CacheEntry> Cache_;
  std::vector<CacheIndex> CacheAt_;
  size_t CacheBytes_ = 0;
  uint64_t CacheClock_ = 0;

  mutable std::mutex LedgerMutex_;
  Ledger Ledger_;

  std::vector<std::atomic<size_t>> ContextBytes_;

  mutable std::mutex QueueMutex_;
  std::condition_variable Wake_;
  std::condition_variable Landed_;
  std::vector<Job> Queue_;
  std::vector<Job> Carrying_;
  FlatMap<Result> Done_;
  FlatMap<bool> Posted_;
  RecentKeys<1024> Kept_;
  RecentKeys<1024> Passing_;

  long long Posts_ = 0, Repeats_ = 0;
  double FocusLatDeg_ = 0.0, FocusLonDeg_ = 0.0;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
  std::vector<std::thread> Carriers_;
  FlatMap<std::vector<Job>> Awaiting_;
};

}
#endif
