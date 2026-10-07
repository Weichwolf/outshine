#include "BuildingSurfaceBlock.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

namespace outshine::Generators {

std::expected<void, StructureMeshError> BuildingSurfaceBlock::Require() {
  const auto state = State_.load(std::memory_order_acquire);
  if (state == State::Ready) { return {}; }
  if (state == State::Failed) {
    assert(Models_.has_value());
    return std::unexpected(Models_->error());
  }
  const std::scoped_lock lock(Lock_);
  if (!Models_) {
    Models_.emplace(Load_());
    Load_ = {};
    State_.store(*Models_ ? State::Ready : State::Failed, std::memory_order_release);
  }
  if (!*Models_) { return std::unexpected(Models_->error()); }
  return {};
}

const BuildingSurface &BuildingSurfaceBlock::At(uint32_t index) const noexcept {
  assert(Models_ && *Models_ && index < (**Models_).size());
  return (**Models_)[index];
}

void BuildingSurface::BindShapes(std::shared_ptr<BuildingSurfaceBlock> block, uint32_t index) {
  assert(Selection_ && block);
  Block_ = std::move(block);
  BlockIndex_ = index;
}

std::expected<void, StructureMeshError> BuildingSurface::RequireShapes() const {
  if (Selection_ && !Block_ && Shapes_.empty()) {
    return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
  }
  return Block_ ? Block_->Require() : std::expected<void, StructureMeshError>{};
}

const BuildingSurface &BuildingSurface::Resident() const noexcept {
  return Block_ ? Block_->At(BlockIndex_) : *this;
}

std::span<const BuildingShape> BuildingSurface::Shapes() const noexcept {
  return Resident().Shapes_;
}

size_t BuildingSurface::FaceCount() const noexcept {
  if (Selection_) { return Selection_->Faces; }
  return FaceOffsets_.empty() ? 0 : FaceOffsets_.back();
}

}
