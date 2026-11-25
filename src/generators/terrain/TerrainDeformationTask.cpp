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
namespace {
struct DeformationArchive {
  std::shared_ptr<PreparedTerrainAssets> Cache;
  std::string Key;
  PreparedTerrainDeformation Product;
  std::optional<PreparedTerrainAssets::DeformationWrite> Write;

  std::expected<bool, PreparedTerrainAssets::Failure> Store(const std::stop_token &stop) {
    if (stop.stop_requested()) {
      return std::unexpected(std::string("terrain deformation archive cancelled"));
    }
    if (!Write) {
      auto prepared = PreparedTerrainAssets::PrepareDeformation(std::move(Key), std::move(Product));
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      Write = std::move(*prepared);
    }
    return Cache->StoreDeformation(*Write, stop);
  }
};
}

struct TerrainDeformationTask::State {
  enum class Phase : uint8_t { Prepare, Lookup, Generate, Complete };

  State(std::shared_ptr<PreparedTerrainAssets> cache,
        std::vector<EarthworkStamp> stamps,
        std::vector<Sheet> pages,
        TangentFrame frame,
        TerrainPageLayout layout,
        double mostEarthworkM,
        StampPreparation prepare)
      : Cache(std::move(cache)),
        Stamps(std::move(stamps)),
        PrepareStamps(std::move(prepare)),
        Frame(frame),
        Layout(layout),
        MostEarthworkM(mostEarthworkM) {
    Input.Sheets = std::move(pages);
    StampBytes = Stamps.capacity() * sizeof(EarthworkStamp);
    Heap.store(sizeof(State) + StampBytes + Input.Sheets.capacity() * sizeof(Sheet),
               std::memory_order_relaxed);
  }

  void RecordHeap() noexcept {
    size_t bytes = sizeof(State) + Key.capacity() + 1 + Input.HeapBytes() +
                   Product.Pages.capacity() * sizeof(Sheet);
    for (const Sheet &page : Product.Pages) {
      bytes += (page.SharedSamples ? page.SharedSamples->capacity() : page.Nodes.capacity()) *
               sizeof(float);
    }
    if (Persist) { bytes += sizeof(DeformationArchive) + Product.Pages.size() * sizeof(Sheet); }
    if (Press) {
      bytes += Press->HeapBytes();
    } else {
      bytes += StampBytes;
    }
    Heap.store(bytes, std::memory_order_relaxed);
  }

  Tasks::StepResult Prepare() {
    if (PrepareStamps) { std::exchange(PrepareStamps, {})(Stamps); }
    LoadBytesMost = std::max(kTerrainDeformationBytesMost, Input.HeapBytes());
    StampBytes = Stamps.capacity() * sizeof(EarthworkStamp);
    for (const EarthworkStamp &stamp : Stamps) { StampBytes += stamp.HeapBytes(); }
    Current = Phase::Lookup;
    return Tasks::StepResult::Yield;
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
    StampBytes = 0;
    Current = Phase::Generate;
    return Tasks::StepResult::Yield;
  }

  Tasks::StepResult Generate() {
    if (!Press->Advance(32, 8192)) { return Tasks::StepResult::Yield; }
    Product.Effects = Press->Take();
    Press.reset();
    Product.Pages = std::move(Input.Sheets);
    if (Cache) {
      for (Sheet &page : Product.Pages) {
        page.SharedSamples = std::make_shared<const std::vector<float>>(std::move(page.Nodes));
      }
      Persist = [archive = std::make_shared<DeformationArchive>(DeformationArchive{
                     .Cache = Cache, .Key = std::move(Key), .Product = Product, .Write = {}})](
                    const std::stop_token &stop) { return archive->Store(stop); };
    }
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
      case Phase::Prepare: result = Prepare(); break;
      case Phase::Lookup: result = Lookup(); break;
      case Phase::Generate: result = Generate(); break;
      case Phase::Complete: break;
    }
    RecordHeap();
    if (result == Tasks::StepResult::Complete) { Finish(); }
    return result;
  }

  std::shared_ptr<PreparedTerrainAssets> Cache;
  std::vector<EarthworkStamp> Stamps;
  StampPreparation PrepareStamps;
  Patchwork Input;
  PreparedTerrainDeformation Product;
  TangentFrame Frame;
  TerrainPageLayout Layout;
  double MostEarthworkM;
  size_t StampBytes = 0;
  size_t LoadBytesMost = 0;
  std::string Key;
  std::unique_ptr<TerrainDeformationJob> Press;
  Archive Persist;
  std::expected<void, std::string> Outcome;
  Phase Current = Phase::Prepare;
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
                                               double mostEarthworkM,
                                               StampPreparation prepare)
    : Pool_(pool),
      State_(std::make_shared<State>(std::move(cache),
                                     std::move(stamps),
                                     std::move(candidate.Sheets),
                                     frame,
                                     layout,
                                     mostEarthworkM,
                                     std::move(prepare))) {
  Post();
}

void TerrainDeformationTask::Post() const {
  if (!Pool_.PostDetachedSteps([state = State_] { return state->Step(); })) {
    State_->Fail("compute worker rejected terrain deformation");
    State_->Finish();
  }
}

TerrainDeformationTask::~TerrainDeformationTask() {
  (void)State_->Stop.request_stop();
  (void)Pool_.PostDetached([state = std::move(State_)] { (void)state; });
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

std::expected<TerrainDeformationTask::Result, std::string>
TerrainDeformationTask::Take(Patchwork &candidate) {
  assert(State_->Finished());
  if (!State_->Outcome) { return std::unexpected(std::move(State_->Outcome.error())); }
  candidate.Sheets = std::move(State_->Product.Pages);
  return Result{.Effects = State_->Product.Effects, .Persist = std::move(State_->Persist)};
}

size_t TerrainDeformationTask::HeapBytes() const noexcept {
  return State_->Heap.load(std::memory_order_relaxed);
}
}
