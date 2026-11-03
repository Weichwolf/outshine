#include "TerrainClipmapRequest.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
struct TerrainClipmapRequest::State {
  enum class Phase { Sampling, Ready, Failed, Consumed };

  State(std::shared_ptr<const TerrainSurfacePublication> source,
        Render::ClipmapHeightUpdate update,
        TerrainClipmapSampling sampling)
      : Source(std::move(source)),
        Update(std::move(update)),
        SpacingM(sampling.SpacingM),
        Slice(sampling.SamplesPerSlice) {}

  Tasks::StepResult Step() {
    if (!Source || Slice == 0) { return Fail("invalid terrain clipmap source or work slice"); }
    if (Stop.stop_requested()) { return Fail("terrain clipmap sampling cancelled"); }
    const size_t count = std::min(Slice, Update.Positions().size() - Next);
    const auto sampled = Source->SampleRenderHeights(Update.Positions().subspan(Next, count),
                                                     Update.Samples().subspan(Next, count),
                                                     SpacingM,
                                                     Stop.get_token());
    if (!sampled) {
      switch (sampled.error()) {
        case TerrainSamplingError::Cancelled: return Fail("terrain clipmap sampling cancelled");
        case TerrainSamplingError::Unpublished:
          return Fail("terrain clipmap source is unpublished");
        case TerrainSamplingError::InvalidInput:
          return Fail("invalid terrain clipmap sampling input");
      }
    }
    Next += count;
    if (Next != Update.Positions().size()) { return Tasks::StepResult::Yield; }
    Status.store(Phase::Ready, std::memory_order_release);
    return Tasks::StepResult::Complete;
  }

  Tasks::StepResult Fail(const char *error) {
    Failure = error;
    Status.store(Phase::Failed, std::memory_order_release);
    return Tasks::StepResult::Complete;
  }

  std::shared_ptr<const TerrainSurfacePublication> Source;
  Render::ClipmapHeightUpdate Update;
  double SpacingM;
  size_t Slice;
  size_t Next = 0;
  std::stop_source Stop;
  std::atomic<Phase> Status{Phase::Sampling};
  const char *Failure = nullptr;
};

TerrainClipmapRequest::TerrainClipmapRequest(
    Tasks &pool,
    std::shared_ptr<const TerrainSurfacePublication> source,
    Render::ClipmapHeightUpdate update,
    TerrainClipmapSampling sampling)
    : State_(std::make_shared<State>(std::move(source), std::move(update), sampling)) {
  if (!pool.PostDetachedSteps([state = State_] { return state->Step(); })) {
    (void)State_->Fail("compute worker rejected terrain clipmap sampling");
  }
}

TerrainClipmapRequest::~TerrainClipmapRequest() {
  (void)State_->Stop.request_stop();
}

bool TerrainClipmapRequest::Finished() const noexcept {
  return State_->Status.load(std::memory_order_acquire) != State::Phase::Sampling;
}

std::optional<TerrainClipmapRequest::Result> TerrainClipmapRequest::Take() {
  const auto status = State_->Status.load(std::memory_order_acquire);
  if (status == State::Phase::Sampling || status == State::Phase::Consumed) { return std::nullopt; }
  State_->Status.store(State::Phase::Consumed, std::memory_order_release);
  if (status == State::Phase::Failed) { return Result{std::unexpected(State_->Failure)}; }
  return Result{std::move(State_->Update)};
}

size_t TerrainClipmapRequest::HeapBytes() const noexcept {
  return sizeof(State) + State_->Update.HeapBytes();
}
}
