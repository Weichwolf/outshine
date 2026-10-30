#ifndef OUTSHINE_GENERATORS_TERRAIN_INPUTS_TERRAINFIELDPREPARATION_H
#define OUTSHINE_GENERATORS_TERRAIN_INPUTS_TERRAINFIELDPREPARATION_H
#include "TerrainTiles.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
#include <utility>

namespace outshine::Ground {
struct TerrainTiles::FieldStep {
  TerrainGrid Grid;
  bool CacheDeferred = false;
};

class TerrainTiles::FieldRequest {
public:
  virtual ~FieldRequest() = default;
  // The factory and its captures are borrowed for this step only.
  [[nodiscard]] virtual FieldStep Resume(const FieldFactory &factory,
                                         const std::stop_token &stop = {}) = 0;
  [[nodiscard]] virtual size_t HeapBytes() const noexcept = 0;
  // Diagnostics retain only this small counter, never the native product's lifetime.
  [[nodiscard]] virtual std::shared_ptr<const std::atomic_size_t> Memory() const noexcept = 0;
};

class TerrainTiles::FieldWork {
public:
  // Only the executing worker accesses the native request; observers retain its counter.
  [[nodiscard]] const std::shared_ptr<FieldRequest> &Request() const noexcept { return Request_; }

  void SetRequest(std::shared_ptr<FieldRequest> request) {
    auto memory = request ? request->Memory() : nullptr;
    Request_ = std::move(request);
    CacheDeferred_ = false;
    const std::scoped_lock lock(MemoryLock_);
    Memory_ = std::move(memory);
  }

  [[nodiscard]] std::shared_ptr<const std::atomic_size_t> Memory() const {
    const std::scoped_lock lock(MemoryLock_);
    return Memory_;
  }

  [[nodiscard]] bool AwaitingCache() const noexcept { return CacheDeferred_; }

  void DeferredByCache(bool value) noexcept { CacheDeferred_ = value; }

  [[nodiscard]] std::stop_token Token() const noexcept { return Stop_.get_token(); }

  void Cancel() noexcept { Stop_.request_stop(); }

private:
  std::shared_ptr<FieldRequest> Request_;
  std::stop_source Stop_;
  bool CacheDeferred_ = false;
  mutable std::mutex MemoryLock_;
  std::shared_ptr<const std::atomic_size_t> Memory_;
};
}
#endif
