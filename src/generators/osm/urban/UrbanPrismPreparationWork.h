#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMPREPARATIONWORK_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANPRISMPREPARATIONWORK_H

#include "UrbanPrismPreparation.h"
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace outshine::Generators::Osm {
struct UrbanPrismPreparation::Work {
  Result Value;
  Costs Measured;
  std::optional<PreparedAssetWrite> Package;
  AssetFactory Factory;
  Operation Next = Operation::Read;
  bool Busy = false;
  std::atomic<bool> Complete{false};
  std::mutex Mutex;
  std::condition_variable Landed;

  void Run(const std::string &database,
           size_t bytesMost,
           const std::string &request,
           const std::stop_token &stop);
  void Finish();

private:
  void CacheFailure(AssetCacheError error, const char *message);
  void Read(const AssetCache &cache, const std::string &request, size_t bytesMost);
  void Build(AssetCache &cache,
             const std::string &request,
             size_t bytesMost,
             const std::stop_token &stop);
  void Publish(AssetCache &cache);
  void Restore(const AssetCache &cache, const std::string &request, size_t bytesMost);
};
}
#endif
