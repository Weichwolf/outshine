#include "GeometryCullStage.h"
#include "GpuPlacement.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Render {
static_assert(Row(Resource::ClusterRoots).Stride == sizeof(GeometryRoot));
static_assert(Row(Resource::ClusterTemplates).Stride == sizeof(GeometryClusterTemplate));
static_assert(Row(Resource::ClusterChunkState).Stride == sizeof(GeometryQueueState));
static_assert(Row(Resource::GeometryPlacements).Stride == sizeof(GpuPlacement));

namespace {
using S = GeometryResidency::Stream;

Native::Binding Storage(uint32_t set, uint32_t slot, const Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = Native::StorageBuffer{.Data = &buffer,
                                         .Range = {.Bytes = buffer.Description().Bytes}}};
}

template <size_t N>
Native::Result<std::array<Native::Binding, N>>
Bindings(const GeometryResidency &resident, const std::array<S, N> &streams, size_t readonly) {
  std::array<Native::Binding, N> bindings;
  for (size_t at = 0; at < N; ++at) {
    const auto *buffer = resident.Buffer(streams[at]).get();
    if (buffer == nullptr) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "geometry work buffer is missing"});
    }
    const uint32_t set = at < readonly ? 0u : 1u;
    const auto slot = static_cast<uint32_t>(at < readonly ? at : at - readonly);
    bindings[at] = Storage(set, slot, *buffer);
  }
  return bindings;
}
}

Native::Result<void> GeometryCullStage::EncodeCull(const PassRecording &into) {
  if (!CullThisFrame_) { return {}; }
  if (!Cull_ || Source_.Resident == nullptr || Source_.Hierarchy == nullptr ||
      into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "geometry leaf culling is not configured"});
  }
  const auto &resident = *Source_.Resident;
  const auto *reset = resident.Buffer(S::ClusterReset).get();
  auto *state = resident.Buffer(S::ClusterSelectedState).get();
  if (reset == nullptr || state == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "geometry leaf reset buffer is missing"});
  }
  const auto cleared = into.Commands->Copy(*reset, *state, {.Bytes = sizeof(GeometryQueueState)});
  if (!cleared) { return cleared; }
  const auto buffers = Bindings(resident,
                                std::array{S::ClusterRoots,
                                           S::ClusterTemplates,
                                           S::ClusterSpheres,
                                           S::Placements,
                                           S::ClusterChunks,
                                           S::ClusterChunkState,
                                           S::ClusterPrefix,
                                           S::ClusterKept,
                                           S::ClusterSelected,
                                           S::ClusterSelectedState},
                                7);
  if (!buffers) { return std::unexpected(buffers.error()); }
  const auto *pyramid =
      PyramidBuffer_ != nullptr ? PyramidBuffer_ : resident.Buffer(S::ClusterSpheres).get();
  const auto &h = Source_.Hierarchy->View;
  const std::array bindings{
      (*buffers)[0],
      (*buffers)[1],
      (*buffers)[2],
      (*buffers)[3],
      (*buffers)[4],
      (*buffers)[5],
      (*buffers)[6],
      Storage(0, 7, *pyramid),
      (*buffers)[7],
      (*buffers)[8],
      (*buffers)[9],
      Native::Binding{
          .Set = 2, .Slot = 0, .Value = Native::Uniform{std::as_bytes(std::span(&View_, 1))}},
      Native::Binding{
          .Set = 2, .Slot = 1, .Value = Native::Uniform{std::as_bytes(std::span(&h, 1))}}};
  return into.Commands->DispatchIndirect(*Cull_,
                                         bindings,
                                         *resident.Buffer(S::ClusterChunkState),
                                         offsetof(GeometryQueueState, Dispatch));
}

Native::Result<void> GeometryCullStage::EncodeScan(const PassRecording &into) {
  if (!CullThisFrame_) { return {}; }
  if (!Scan_ || Source_.Resident == nullptr || Source_.Hierarchy == nullptr ||
      into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "geometry root scan is not configured"});
  }
  const auto buffers = Bindings(
      *Source_.Resident,
      std::array{
          S::ClusterRoots, S::ClusterVisible, S::ClusterKept, S::ClusterSlot, S::DrawArguments},
      3);
  if (!buffers) { return std::unexpected(buffers.error()); }
  const auto &h = Source_.Hierarchy->View;
  const std::array bindings{
      (*buffers)[0],
      (*buffers)[1],
      (*buffers)[2],
      (*buffers)[3],
      (*buffers)[4],
      Native::Binding{
          .Set = 2, .Slot = 0, .Value = Native::Uniform{std::as_bytes(std::span(&h, 1))}}};
  return into.Commands->Dispatch(*Scan_, bindings, Source_.Hierarchy->ScanGroups);
}

Native::Result<void> GeometryCullStage::EncodeCompact(const PassRecording &into) {
  if (!CullThisFrame_) { return {}; }
  if (!Compact_ || Source_.Resident == nullptr || Source_.Hierarchy == nullptr ||
      into.Commands == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "geometry selected compaction is not configured"});
  }
  const auto buffers = Bindings(*Source_.Resident,
                                std::array{S::ClusterRoots,
                                           S::ClusterTemplates,
                                           S::ClusterSelected,
                                           S::ClusterSelectedState,
                                           S::ClusterSlot,
                                           S::DrawArguments,
                                           S::Index,
                                           S::ClusterVisible,
                                           S::DrawIndex},
                                8);
  if (!buffers) { return std::unexpected(buffers.error()); }
  const auto &h = Source_.Hierarchy->View;
  const std::array bindings{
      (*buffers)[0],
      (*buffers)[1],
      (*buffers)[2],
      (*buffers)[3],
      (*buffers)[4],
      (*buffers)[5],
      (*buffers)[6],
      (*buffers)[7],
      (*buffers)[8],
      Native::Binding{
          .Set = 2, .Slot = 0, .Value = Native::Uniform{std::as_bytes(std::span(&View_, 1))}},
      Native::Binding{
          .Set = 2, .Slot = 1, .Value = Native::Uniform{std::as_bytes(std::span(&h, 1))}}};
  const auto encoded =
      into.Commands->DispatchIndirect(*Compact_,
                                      bindings,
                                      *Source_.Resident->Buffer(S::ClusterSelectedState),
                                      offsetof(GeometryQueueState, Dispatch));
  if (!encoded) { return encoded; }
  into.Submission.Record(Stage::GeometryCompact, Cache_);
  return {};
}
}
