#include "TerrainDeformationTask.h"
#include "PreparedTerrainAssets.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <expected>

namespace outshine::Generators {
struct TerrainDeformationTask::State {
  enum class Phase : uint8_t { Lookup, Generate, Store, Complete };

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
    size_t bytes = sizeof(State) + Input.HeapBytes() + Product.Pages.capacity() * sizeof(Sheet);
    for (const Sheet &page : Product.Pages) { bytes += page.Nodes.capacity() * sizeof(float); }
    if (Press) {
      bytes += Press->HeapBytes();
    } else {
      bytes += Stamps.capacity() * sizeof(EarthworkStamp);
      for (const EarthworkStamp &stamp : Stamps) { bytes += stamp.HeapBytes(); }
    }
    Heap.store(bytes, std::memory_order_relaxed);
  }

  Tasks::StepResult Lookup() {
    if (Cache) {
      auto key = TerrainDeformationKey(Input, Stamps, Frame, Layout, MostEarthworkM);
      if (!key) { return Fail(std::move(key.error())); }
      Key = std::move(*key);
      auto loaded = Cache->LoadDeformation(Key, LoadBytesMost);
      if (!loaded) { return Fail(std::move(loaded.error())); }
      if (*loaded) {
        Product = std::move(**loaded);
        Input.Sheets.clear();
        Current = Phase::Complete;
        return Tasks::StepResult::Complete;
      }
    }
    Press =
        std::make_unique<TerrainPressJob>(std::move(Stamps), Input, Frame, Layout, MostEarthworkM);
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
    auto stored = Cache->StoreDeformation(Key, Product);
    if (!stored) { return Fail(std::move(stored.error())); }
    auto loaded = Cache->LoadDeformation(Key, LoadBytesMost);
    if (!loaded || !*loaded) {
      return Fail(loaded ? "published terrain deformation asset is missing"
                         : std::move(loaded.error()));
    }
    const PressedTerrain effects = Product.Effects;
    Product = std::move(**loaded);
    Product.Effects = effects;
    Current = Phase::Complete;
    return Tasks::StepResult::Complete;
  }

  Tasks::StepResult Fail(std::string message) {
    Outcome = std::unexpected(std::move(message));
    Current = Phase::Complete;
    return Tasks::StepResult::Complete;
  }

  void Finish() {
    const std::scoped_lock lock(CompletionLock);
    Completed = true;
    Ready.notify_all();
  }

  bool Finished() const {
    const std::scoped_lock lock(CompletionLock);
    return Completed;
  }

  Tasks::StepResult Step() {
    if (Stopping.load(std::memory_order_relaxed)) {
      const auto result = Fail("terrain deformation cancelled");
      Finish();
      return result;
    }
    Tasks::StepResult result = Tasks::StepResult::Complete;
    switch (Current) {
      case Phase::Lookup: result = Lookup(); break;
      case Phase::Generate: result = Generate(); break;
      case Phase::Store: result = Store(); break;
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
  std::unique_ptr<TerrainPressJob> Press;
  std::expected<void, std::string> Outcome;
  Phase Current = Phase::Lookup;
  std::atomic_bool Stopping{false};
  std::atomic_size_t Heap{0};
  mutable std::mutex CompletionLock;
  mutable std::condition_variable Ready;
  bool Completed = false;
};

TerrainDeformationTask::TerrainDeformationTask(Tasks &pool,
                                               std::shared_ptr<PreparedTerrainAssets> cache,
                                               std::vector<EarthworkStamp> stamps,
                                               Patchwork &candidate,
                                               TangentFrame frame,
                                               TerrainPageLayout layout,
                                               double mostEarthworkM)
    : State_(std::make_shared<State>(std::move(cache),
                                     std::move(stamps),
                                     std::move(candidate.Sheets),
                                     frame,
                                     layout,
                                     mostEarthworkM)) {
  if (!pool.PostDetachedSteps([state = State_] { return state->Step(); })) {
    State_->Fail("compute worker rejected terrain deformation");
    State_->Finish();
  }
}

TerrainDeformationTask::~TerrainDeformationTask() {
  State_->Stopping.store(true, std::memory_order_relaxed);
}

bool TerrainDeformationTask::Advance() const {
  return State_->Finished();
}

bool TerrainDeformationTask::AwaitSlice(double seconds) const {
  if (!(seconds > 0.0)) { return State_->Finished(); }
  std::unique_lock lock(State_->CompletionLock);
  return State_->Ready.wait_for(
      lock, std::chrono::duration<double>(seconds), [state = State_] { return state->Completed; });
}

std::expected<PressedTerrain, std::string> TerrainDeformationTask::Take(Patchwork &candidate) {
  assert(State_->Finished());
  if (!State_->Outcome) { return std::unexpected(std::move(State_->Outcome.error())); }
  candidate.Sheets = std::move(State_->Product.Pages);
  return State_->Product.Effects;
}

size_t TerrainDeformationTask::HeapBytes() const noexcept {
  return State_->Heap.load(std::memory_order_relaxed);
}
}
