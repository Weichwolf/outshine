#include "StructureSourcePreparation.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace outshine {

StructureSourcePreparation::StructureSourcePreparation(Tasks &pool,
                                                       const Ground::HeightField::Request &request,
                                                       SourcedTerrainFields sources,
                                                       size_t bytesMost)
    : Pool_(&pool), Work_(std::make_unique<Work>()) {
  if (request.Fallback || request.Tiles.empty() ||
      std::ranges::any_of(request.Tiles,
                          [&request](const auto tile) { return tile.Zoom != request.Zoom; })) {
    Work_->Outcome = State::Failed;
    return;
  }
  if (bytesMost < sizeof(Work) ||
      !sources.FitsPreparation(request.Tiles, bytesMost - sizeof(Work))) {
    Work_->Outcome = State::OverBudget;
    return;
  }
  Work_->Request = request;
  Work_->Sources = std::move(sources);
  Work_->Blocks.reserve(request.Tiles.size());
}

StructureSourcePreparation::~StructureSourcePreparation() {
  Cancel();
  Join();
}

StructureSourcePreparation::StructureSourcePreparation(StructureSourcePreparation &&other) noexcept
    : Pool_(std::exchange(other.Pool_, nullptr)),
      Handle_(std::exchange(other.Handle_, Tasks::kNoTask)),
      Work_(std::move(other.Work_)) {}

StructureSourcePreparation &
StructureSourcePreparation::operator=(StructureSourcePreparation &&other) noexcept {
  if (this == &other) { return *this; }
  Cancel();
  Join();
  Pool_ = std::exchange(other.Pool_, nullptr);
  Handle_ = std::exchange(other.Handle_, Tasks::kNoTask);
  Work_ = std::move(other.Work_);
  return *this;
}

void StructureSourcePreparation::Cancel() noexcept {
  if (Work_) { Work_->Stopping.store(true, std::memory_order_relaxed); }
}

void StructureSourcePreparation::Join() {
  if (!Running()) { return; }
  Pool_->Wait(Handle_);
  Handle_ = Tasks::kNoTask;
}

StructureSourcePreparation::State StructureSourcePreparation::Advance() {
  if (!Work_) { return State::Cancelled; }
  if (Running()) {
    if (!Pool_->Done(Handle_)) { return State::Preparing; }
    Handle_ = Tasks::kNoTask;
  }
  if (Work_->Stopping.load(std::memory_order_relaxed)) {
    Work_->Result.reset();
    Work_->Outcome = State::Cancelled;
  }
  if (Work_->Outcome != State::Preparing) { return Work_->Outcome; }
  Work *const work = Work_.get();
  Handle_ = Pool_->Post([work] {
    const size_t end = std::min(work->Next + 4u, work->Request.Tiles.size());
    while (work->Next < end) {
      if (work->Stopping.load(std::memory_order_relaxed)) {
        work->Outcome = State::Cancelled;
        return;
      }
      const auto at = work->Request.Tiles[work->Next];
      Ground::HeightField::Block block;
      if (!work->Sources.ShareSourcedField(
              {.Zoom = at.Zoom, .X = static_cast<uint32_t>(at.X), .Y = static_cast<uint32_t>(at.Y)},
              block)) {
        work->Outcome = State::Failed;
        return;
      }
      work->Blocks.push_back(std::move(block));
      ++work->Next;
    }
    if (work->Next == work->Request.Tiles.size()) {
      work->Result = Ground::HeightField::Of(work->Request.Zoom, std::move(work->Blocks));
      work->Outcome = work->Result->Qualified() ? State::Ready : State::Failed;
      work->Sources = {};
    }
  });
  return State::Preparing;
}

std::shared_ptr<const Ground::HeightField> StructureSourcePreparation::Result() const noexcept {
  return !Running() && Work_ && Work_->Outcome == State::Ready ? Work_->Result : nullptr;
}

}
