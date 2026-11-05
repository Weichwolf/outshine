#include "GeometryPrefixScan.h"
#include "ComputeShaders.h"
#include "PipelineFactory.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr uint32_t kBlock = 256;

struct alignas(16) ScanView {
  uint32_t Count = 0;
  uint32_t GridWidth = 0;
  std::array<uint32_t, 2> Reserved{};
};

static_assert(sizeof(ScanView) == 16 && alignof(ScanView) == 16);
static_assert(offsetof(ScanView, Count) == 0 && offsetof(ScanView, GridWidth) == 4);
static_assert(offsetof(ScanView, Reserved) == 8);

uint32_t Blocks(uint32_t count) {
  return count / kBlock + uint32_t{count % kBlock != 0};
}

Native::Binding Storage(uint32_t set, uint32_t slot, const Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = Native::StorageBuffer{.Data = &buffer,
                                         .Range = {.Bytes = buffer.Description().Bytes}}};
}

Native::Binding Uniform(const ScanView &view) {
  return {.Set = 2, .Slot = 0, .Value = Native::Uniform{std::as_bytes(std::span(&view, 1))}};
}

Native::Workgroups Grid(const ScanView &view) {
  const uint32_t blocks = Blocks(view.Count);
  return {.X = std::min(blocks, view.GridWidth),
          .Y = blocks / view.GridWidth + uint32_t{blocks % view.GridWidth != 0}};
}

Native::Result<void>
Grow(Native::Device &device, std::unique_ptr<Native::Buffer> &buffer, uint64_t bytes) {
  if (buffer && buffer->Description().Bytes >= bytes) { return {}; }
  const uint64_t capacity = std::min(std::bit_ceil(bytes), device.Limits().MaxStorageBufferBytes);
  if (capacity < bytes) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::OutOfMemory,
                      .Message = "cluster scan exceeds native storage addressing"});
  }
  auto made = device.CreateBuffer({.Bytes = capacity, .Use = Native::BufferUse::Storage});
  if (!made) { return std::unexpected(made.error()); }
  buffer = std::move(*made);
  return {};
}
}

Native::Result<void> GeometryPrefixScan::Configure(Native::Device &device) {
  if (Device_ != &device) {
    Programs_ = {};
    Levels_.clear();
    Device_ = &device;
  }
  constexpr std::array shaders{ComputeShaderId::GeometryPrefix,
                               ComputeShaderId::GeometryPrefixBlocks,
                               ComputeShaderId::GeometryPrefixCarry,
                               ComputeShaderId::GeometryPrefixFinish};
  for (size_t at = 0; at < shaders.size(); ++at) {
    if (Programs_[at]) { continue; }
    auto made = CreateComputePipeline(device, shaders[at]);
    if (!made) { return std::unexpected(made.error()); }
    Programs_[at] = std::move(*made);
  }
  Device_ = &device;
  return {};
}

Native::Result<size_t> GeometryPrefixScan::Prepare(uint32_t count) {
  size_t depth = 0;
  while (true) {
    if (depth == Levels_.size()) { Levels_.emplace_back(); }
    auto &level = Levels_[depth];
    level.Count = count;
    const uint32_t blocks = Blocks(count);
    const auto prefix = Grow(*Device_, level.Prefix, uint64_t{count} * 8);
    if (!prefix) { return std::unexpected(prefix.error()); }
    const auto tail = Grow(*Device_, level.Tail, uint64_t{blocks} * 8);
    if (!tail) { return std::unexpected(tail.error()); }
    ++depth;
    if (blocks == 1) { return depth; }
    count = blocks;
  }
}

Native::Result<void> GeometryPrefixScan::Encode(Native::Commands &commands, Source inputs) {
  const auto &[addresses, kept, visible, slots, args, logicalCount] = inputs;
  if (logicalCount == 0) { return {}; }
  if (Device_ == nullptr || addresses == nullptr || kept == nullptr || visible == nullptr ||
      slots == nullptr || args == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "cluster prefix scan source is incomplete"});
  }
  const auto depth = Prepare(logicalCount);
  if (!depth) { return std::unexpected(depth.error()); }
  const uint32_t width = std::min(256u, Device_->Limits().MaxWorkgroupCount[0]);

  for (size_t at = 0; at < *depth; ++at) {
    const auto &level = Levels_[at];
    const auto *source = at == 0 ? addresses : Levels_[at - 1].Tail.get();
    const ScanView view{.Count = level.Count, .GridWidth = width};
    const std::array bindings{Storage(0, 0, *source),
                              Storage(1, 0, *level.Prefix),
                              Storage(1, 1, *level.Tail),
                              Uniform(view),
                              Storage(0, 1, *kept),
                              Storage(0, 2, *visible),
                              Storage(1, 2, *slots)};
    const auto selected = std::span(bindings).first(at == 0 ? 7 : 4);
    const auto result = commands.Dispatch(*Programs_[at == 0 ? 0 : 1], selected, Grid(view));
    if (!result) { return result; }
  }
  for (size_t at = *depth - 1; at > 1; --at) {
    const auto &level = Levels_[at - 1];
    const ScanView view{.Count = level.Count, .GridWidth = width};
    const std::array bindings{
        Storage(0, 0, *Levels_[at].Prefix), Storage(1, 0, *level.Prefix), Uniform(view)};
    const auto result = commands.Dispatch(*Programs_[2], bindings, Grid(view));
    if (!result) { return result; }
  }
  const auto &first = Levels_.front();
  const auto &parent = *depth > 1 ? Levels_[1].Prefix : first.Tail;
  const ScanView view{.Count = logicalCount, .GridWidth = width};
  const std::array bindings{Storage(0, 0, *first.Prefix),
                            Storage(0, 1, *parent),
                            Storage(1, 0, *slots),
                            Storage(1, 1, *args),
                            Uniform(view)};
  return commands.Dispatch(*Programs_[3], bindings, Grid(view));
}
}
