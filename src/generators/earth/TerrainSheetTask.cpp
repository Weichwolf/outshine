#include "TerrainSheetTask.h"
#include "Tasks.h"
#include "TerrainInputPreparation.h"
#include "TerrainRefinement.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <utility>

namespace outshine::Generators {
struct TerrainSheetTask::State {
  enum class Phase : uint8_t { Refinement, Halos, Survey, Ready, Rejected, Consumed };
  static constexpr size_t kRefinementSourcesPerSlice = 8;
  static constexpr size_t kHaloNodesPerSlice = 32768;
  static constexpr size_t kSheetsPerSurveySlice = 48;

  State(TerrainInputPreparation &inputs,
        Patchwork &pages,
        Settings settings,
        TerrainRefinementJob refinement)
      : Inputs(std::exchange(inputs, {})),
        Pages{.Sheets = std::exchange(pages.Sheets, {})},
        Config(std::move(settings)),
        Refinement(std::move(refinement)),
        Halos(Inputs, Pages, Config.FinestZoom, Config.HaloCache.get(), Config.Generation),
        FieldBytes(Inputs.HeapBytes()),
        PageBytes(Pages.HeapBytes()) {}

  static void Record(Timing &timing, std::chrono::steady_clock::time_point began) {
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    timing.TotalMs += ms;
    timing.LongestSliceMs = std::max(timing.LongestSliceMs, ms);
  }

  Tasks::StepResult AdvanceRefinement() {
    assert(Refinement.has_value());
    auto &refinement = *Refinement;
    const auto began = std::chrono::steady_clock::now();
    auto ready = refinement.Advance(kRefinementSourcesPerSlice);
    Record(Product.Refinement, began);
    if (!ready) {
      Failure = std::move(ready.error());
      Where.store(Phase::Rejected, std::memory_order_release);
      return Tasks::StepResult::Complete;
    }
    if (*ready) {
      Product.LongestRefinementSourceMs = refinement.LongestSourceMs();
      Product.RefinementDeduplicationMs = refinement.DeduplicationMs();
      Pages.Sheets = std::move(refinement).Take();
      Refinement.reset();
      if (Config.HaloCache) { Config.HaloCache->SelectWindow(Config.Generation, Pages.Sheets); }
      Product.VirtualSheets = static_cast<size_t>(
          std::ranges::count_if(Pages.Sheets, [](const Sheet &sheet) { return sheet.Virtual; }));
      PageBytes.store(Pages.HeapBytes(), std::memory_order_relaxed);
      Where.store(Phase::Halos, std::memory_order_release);
    }
    return Tasks::StepResult::Yield;
  }

  void AdvanceHalos() {
    const auto began = std::chrono::steady_clock::now();
    const bool ready = Halos.Advance(kHaloNodesPerSlice);
    Record(Product.Halos, began);
    if (ready) {
      Product.Haloed = Halos.Haloed();
      PageBytes.store(Pages.HeapBytes(), std::memory_order_relaxed);
      Where.store(Phase::Survey, std::memory_order_release);
    }
  }

  bool AdvanceSurvey() {
    const auto began = std::chrono::steady_clock::now();
    auto &survey = Product.Survey;
    if (NextSheet == 0) {
      const auto side = static_cast<size_t>(Config.Layout.Side);
      const size_t nodes = Pages.Sheets.size() * side * side;
      survey.ProbePositionsM.reserve((nodes + Config.ProbeStride - 1u) / Config.ProbeStride);
      ProbeBytes.store(survey.ProbePositionsM.capacity() * sizeof(Vec3f),
                       std::memory_order_relaxed);
    }
    const size_t end = std::min(NextSheet + kSheetsPerSurveySlice, Pages.Sheets.size());
    for (; NextSheet < end; ++NextSheet) {
      SurveyTerrainSheet(
          survey, Pages.Sheets[NextSheet], Config.Frame, Config.Layout, Config.ProbeStride);
    }
    Record(Product.Surveying, began);
    return NextSheet == Pages.Sheets.size();
  }

  Tasks::StepResult Step() {
    if (Stopping.load(std::memory_order_relaxed)) { return Tasks::StepResult::Complete; }
    switch (Where.load(std::memory_order_relaxed)) {
      case Phase::Refinement: return AdvanceRefinement();
      case Phase::Halos: AdvanceHalos(); return Tasks::StepResult::Yield;
      case Phase::Survey:
        if (!AdvanceSurvey()) { return Tasks::StepResult::Yield; }
        Where.store(Phase::Ready, std::memory_order_release);
        break;
      case Phase::Ready:
      case Phase::Rejected:
      case Phase::Consumed: break;
    }
    return Tasks::StepResult::Complete;
  }

  TerrainInputPreparation Inputs;
  Patchwork Pages;
  Settings Config;
  std::optional<TerrainRefinementJob> Refinement;
  TerrainInputPreparation::HaloBuildJob Halos;
  size_t FieldBytes;
  std::atomic_size_t PageBytes;
  std::atomic_size_t ProbeBytes{0};
  std::atomic_bool Stopping{false};
  std::atomic<Phase> Where{Phase::Refinement};
  size_t NextSheet = 0;
  Completed Product;
  std::string Failure;
};

TerrainSheetTask::TerrainSheetTask(Tasks &pool,
                                   TerrainInputPreparation &inputs,
                                   Patchwork &pages,
                                   Settings settings,
                                   TerrainRefinementJob refinement)
    : Pool_(pool),
      State_(std::make_shared<State>(inputs, pages, std::move(settings), std::move(refinement))) {
  if (!State_->Config.Layout.Valid() || State_->Config.ProbeStride == 0 ||
      !pool.PostDetachedSteps([state = State_] { return state->Step(); })) {
    State_->Failure = "terrain sheet preparation could not start";
    State_->Where.store(State::Phase::Rejected, std::memory_order_release);
  }
}

TerrainSheetTask::~TerrainSheetTask() {
  State_->Stopping.store(true, std::memory_order_relaxed);
  (void)Pool_.PostDetached([state = std::move(State_)] { (void)state; });
}

bool TerrainSheetTask::RefinementReady() const noexcept {
  return State_->Where.load(std::memory_order_acquire) >= State::Phase::Halos;
}

bool TerrainSheetTask::HalosReady() const noexcept {
  return State_->Where.load(std::memory_order_acquire) >= State::Phase::Survey;
}

bool TerrainSheetTask::AwaitSlice(double seconds) const {
  const auto epoch = Pool_.CompletionEpoch();
  const auto phase = State_->Where.load(std::memory_order_acquire);
  return phase >= State::Phase::Ready || Pool_.AwaitProgress(epoch, seconds);
}

std::optional<TerrainSheetTask::Result> TerrainSheetTask::Take(TerrainInputPreparation &inputs,
                                                               Patchwork &pages) {
  const auto phase = State_->Where.load(std::memory_order_acquire);
  if (phase == State::Phase::Rejected) {
    State_->Where.store(State::Phase::Consumed, std::memory_order_release);
    return Result{std::unexpected(std::move(State_->Failure))};
  }
  if (phase != State::Phase::Ready) { return std::nullopt; }
  inputs = std::move(State_->Inputs);
  pages.Sheets = std::move(State_->Pages.Sheets);
  State_->Where.store(State::Phase::Consumed, std::memory_order_release);
  return Result{std::move(State_->Product)};
}

TerrainFieldSet TerrainSheetTask::SnapshotSourceFields() const {
  return State_->Inputs.SnapshotSourceFields();
}

TerrainSheetTask::Memory TerrainSheetTask::PayloadMemory() const noexcept {
  return {.Fields = State_->FieldBytes,
          .Pages = State_->PageBytes.load(std::memory_order_relaxed),
          .Probes = State_->ProbeBytes.load(std::memory_order_relaxed)};
}
}
