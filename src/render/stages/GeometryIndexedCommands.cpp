#include "GeometryRenderer.h"
#include <algorithm>
#include <utility>

namespace outshine::Render {
Native::Result<void>
GeometryRenderer::PrepareIndexedCommands(const PassRecording &into,
                                         GeometryResidency::SelectionPhase phase) {
  auto *device = Binding().Device;
  if (device == nullptr || !device->Limits().PreparedIndexedCommands || Jobs_ == 0 ||
      Batches.empty()) {
    return {};
  }
  const auto &arguments = Bound().Buffer(
      GeometryResidency::SelectionStream(GeometryResidency::Stream::DrawArguments, phase));
  const auto &indices = Bound().Buffer(
      GeometryResidency::SelectionStream(GeometryResidency::Stream::DrawIndex, phase));
  const auto at = static_cast<size_t>(phase);
  if (into.Commands == nullptr || !arguments || !indices) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "indexed preparation needs selected geometry"});
  }
  if (IndexedCommandCapacity_[at] < Batches.size()) {
    auto made = device->CreateIndexedCommands(static_cast<uint32_t>(Batches.size()));
    if (!made) { return std::unexpected(made.error()); }
    IndexedCommands_[at] = std::move(*made);
    IndexedCommandCapacity_[at] = static_cast<uint32_t>(Batches.size());
  }
  std::vector<Native::IndexedCommandSpan> spans;
  for (size_t first = 0; first < Batches.size();) {
    if (Batches[first].JobCount == 0) {
      ++first;
      continue;
    }
    const size_t end = IndirectRunEnd(first);
    while (first < end) {
      const auto count = static_cast<uint32_t>(std::min<size_t>(64, end - first));
      spans.push_back({.First = static_cast<uint32_t>(first), .Count = count});
      first += count;
    }
  }
  if (spans.empty()) { return {}; }
  return into.Commands->PrepareIndexedCommands(
      *IndexedCommands_[at],
      {.Data = indices.get(), .Range = {.Bytes = indices->Description().Bytes}},
      *arguments,
      spans);
}
}
