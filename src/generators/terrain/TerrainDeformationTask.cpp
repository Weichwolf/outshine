#include "TerrainDeformationTask.h"
#include "PreparedTerrainAssets.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <optional>
#include <stop_token>
#include <variant>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <expected>

namespace outshine::Generators {
struct TerrainDeformationTask::State {
  enum class Phase : uint8_t { Lookup, Generate, Store, Reload, Complete };

  State(std::shared_ptr<PreparedTerrainAssets> cache,
        std::vector<EarthworkStamp> stamps,
        std::vector<Sheet> pages,
        TangentFrame frame,
        TerrainPageLayout layout,
        double mostEarthworkM)
      : Cache(std::move(cache)),
        Stamps(std::move(stamps)),
        Frame(frame),
        Layout(layout),
        MostEarthworkM(mostEarthworkM) {
    Input.Sheets = std::move(pages);
    LoadBytesMost = std::max(kTerrainDeformationBytesMost, Input.HeapBytes());
    RecordHeap();
  }

  void RecordHeap() noexcept {
    size_t bytes = sizeof(State) + Key.capacity() + 1 + Input.HeapBytes() +
                   Product.Pages.capacity() * sizeof(Sheet);
    for (const Sheet &page : Product.Pages) { bytes += page.Nodes.capacity() * sizeof(float); }
    if (Press) {
      bytes += Press->HeapBytes();
    } else {
      bytes += Stamps.capacity() * sizeof(EarthworkStamp);
      for (const EarthworkStamp &stamp : Stamps) { bytes += stamp.HeapBytes(); }
    }
    if (Publication) { bytes += Publication->HeapBytes(); }
    Heap.store(bytes, std::memory_order_relaxed);
  }

  Tasks::StepResult Lookup() {
    if (Cache) {
      if (Key.empty()) {
        auto key = TerrainDeformationKey(Input, Stamps, Frame, Layout, MostEarthworkM);
        if (!key) { return Fail(std::move(key.error())); }
        Key = std::move(*key);
      }
      auto loaded = Cache->LoadDeformation(Key, LoadBytesMost);
      if (!loaded) { return CacheFailure(loaded.error()); }
      if (*loaded) {
        Product = std::move(**loaded);
        Input.Sheets.clear();
        Current = Phase::Complete;
        return Tasks::StepResult::Complete;
      }
    }
    Press = std::make_unique<TerrainDeformationJob>(
        std::move(Stamps), Input, Frame, Layout, MostEarthworkM);
    Current = Phase::Generate;
    return Tasks::StepResult::Yield;
  }

  Tasks::StepResult Generate() {
    if (!Press->Advance(32, 8192)) { return Tasks::StepResult::Yield; }
    Product.Effects = Press->Take();
    Press.reset();
    Product.Pages = std::move(Input.Sheets);
    Current = Cache ? Phase::Store : Phase::Complete;
    return Cache ? Tasks::StepResult::Yield : Tasks::StepResult::Complete;
  }

  Tasks::StepResult Store() {
    if (!Publication) {
      auto prepared = PreparedTerrainAssets::PrepareDeformation(Key, std::move(Product));
      if (!prepared) { return CacheFailure(prepared.error()); }
      Publication = std::move(*prepared);
    }
    auto stored = Cache->StoreDeformation(*Publication, Stop.get_token());
    if (!stored) { return CacheFailure(stored.error()); }
    if (*stored) { Current = Phase::Reload; }
    return Tasks::StepResult::Yield;
  }

  Tasks::StepResult Reload() {
    if (!Publication) { return Fail("terrain deformation publication state is missing"); }
    auto loaded = Cache->LoadDeformation(Key, LoadBytesMost);
    if (!loaded) { return CacheFailure(loaded.error()); }
    if (!*loaded) { return Fail("published terrain deformation asset is missing"); }
    Product = std::move(**loaded);
    Product.Effects = Publication->Effects();
    Publication.reset();
    Current = Phase::Complete;
    return Tasks::StepResult::Complete;
  }

  Tasks::StepResult CacheFailure(const PreparedTerrainAssets::Failure &failure) {
    const auto *cache = std::get_if<AssetCacheError>(&failure);
    if (cache == nullptr || *cache != AssetCacheError::Busy) {
      return Fail(PreparedTerrainAssets::DescribeFailure(failure));
    }
    RetryAt = std::chrono::steady_clock::now() + Delay;
    Delay = std::min(Delay * 2, std::chrono::milliseconds(16));
    return Tasks::StepResult::Complete;
  }

  Tasks::StepResult Fail(std::string message) {
    Outcome = std::unexpected(std::move(message));
    Publication.reset();
    Current = Phase::Complete;
    return Tasks::StepResult::Complete;
  }

  void Finish() {
    const std::scoped_lock lock(CompletionLock);
    Completed = Current == Phase::Complete;
    Deferred = !Completed;
    Ready.notify_all();
  }

  bool ResumeDue() {
    const std::scoped_lock lock(CompletionLock);
    if (!Deferred || std::chrono::steady_clock::now() < RetryAt) { return false; }
    Deferred = false;
    return true;
  }

  bool Finished() const {
    const std::scoped_lock lock(CompletionLock);
    return Completed;
  }

  Tasks::StepResult Step() {
    if (Stop.stop_requested()) {
      const auto result = Fail("terrain deformation cancelled");
      RecordHeap();
      Finish();
      return result;
    }
    Tasks::StepResult result = Tasks::StepResult::Complete;
    switch (Current) {
      case Phase::Lookup: result = Lookup(); break;
      case Phase::Generate: result = Generate(); break;
      case Phase::Store: result = Store(); break;
      case Phase::Reload: result = Reload(); break;
      case Phase::Complete: break;
    }
    RecordHeap();
    if (result == Tasks::StepResult::Complete) { Finish(); }
    return result;
  }

  std::shared_ptr<PreparedTerrainAssets> Cache;
  std::vector<EarthworkStamp> Stamps;
  Patchwork Input;
  PreparedTerrainDeformation Product;
  TangentFrame Frame;
  TerrainPageLayout Layout;
  double MostEarthworkM;
  size_t LoadBytesMost = 0;
  std::string Key;
  std::unique_ptr<TerrainDeformationJob> Press;
  std::optional<PreparedTerrainAssets::DeformationWrite> Publication;
  std::expected<void, std::string> Outcome;
  Phase Current = Phase::Lookup;
  std::stop_source Stop;
  std::chrono::steady_clock::time_point RetryAt;
  std::chrono::milliseconds Delay{1};
  std::atomic_size_t Heap{0};
  mutable std::mutex CompletionLock;
  mutable std::condition_variable Ready;
  bool Completed = false, Deferred = false;
};

TerrainDeformationTask::TerrainDeformationTask(Tasks &pool,
                                               std::shared_ptr<PreparedTerrainAssets> cache,
                                               std::vector<EarthworkStamp> stamps,
                                               Patchwork &candidate,
                                               TangentFrame frame,
                                               TerrainPageLayout layout,
                                               double mostEarthworkM)
    : Pool_(pool),
      State_(std::make_shared<State>(std::move(cache),
                                     std::move(stamps),
                                     std::move(candidate.Sheets),
                                     frame,
                                     layout,
                                     mostEarthworkM)) {
  Post();
}

void TerrainDeformationTask::Post() const {
  if (!Pool_.PostDetachedSteps([state = State_] { return state->Step(); })) {
    State_->Fail("compute worker rejected terrain deformation");
    State_->RecordHeap();
    State_->Finish();
  }
}

TerrainDeformationTask::~TerrainDeformationTask() {
  (void)State_->Stop.request_stop();
}

bool TerrainDeformationTask::Advance() const {
  if (State_->ResumeDue()) { Post(); }
  return State_->Finished();
}

bool TerrainDeformationTask::AwaitSlice(double seconds) const {
  if (Advance() || !(seconds > 0.0)) { return State_->Finished(); }
  std::unique_lock lock(State_->CompletionLock);
  const bool deferred = State_->Deferred;
  const auto wait = deferred ? std::min(std::chrono::duration<double>(seconds),
                                        std::chrono::duration<double>(
                                            State_->RetryAt - std::chrono::steady_clock::now()))
                             : std::chrono::duration<double>(seconds);
  (void)State_->Ready.wait_for(lock, wait, [state = State_, deferred] {
    return state->Completed || (!deferred && state->Deferred);
  });
  lock.unlock();
  return Advance();
}

std::expected<TerrainDeformationMetrics, std::string>
TerrainDeformationTask::Take(Patchwork &candidate) {
  assert(State_->Finished());
  if (!State_->Outcome) { return std::unexpected(std::move(State_->Outcome.error())); }
  candidate.Sheets = std::move(State_->Product.Pages);
  return State_->Product.Effects;
}

size_t TerrainDeformationTask::HeapBytes() const noexcept {
  return State_->Heap.load(std::memory_order_relaxed);
}
}
