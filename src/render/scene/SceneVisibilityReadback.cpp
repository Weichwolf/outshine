#include "SceneRenderer.h"
#include "ReadbackTransfer.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
using S = GeometryResidency::Stream;

bool ReadWords(Native::Device *device,
               const GeometryResidency &resident,
               S stream,
               uint64_t words,
               std::vector<uint32_t> &into,
               KeptDraws &cost) {
  const auto *buffer = resident.Buffer(stream).get();
  if (words > std::numeric_limits<uint32_t>::max() / sizeof(uint32_t) || buffer == nullptr) {
    return false;
  }
  const auto bytes = static_cast<uint32_t>(words * sizeof(uint32_t));
  if (bytes == 0) {
    into.clear();
    return true;
  }
  Readback read;
  if (bytes > buffer->Description().Bytes ||
      read.FromBuffer(device, buffer, bytes) != ReadState::Ready) {
    return false;
  }
  const auto *values = reinterpret_cast<const uint32_t *>(read.Rows());
  into.assign(values, values + words);
  cost.ReadbackBytes += bytes;
  return true;
}

bool CombineVisibility(const GeometryRenderer &geometry,
                       std::span<const uint32_t> first,
                       std::span<const uint32_t> last,
                       std::span<const uint32_t> roots,
                       KeptDraws &into) {
  const auto templates = geometry.SelectionTemplates();
  const auto sources = geometry.SelectionRoots();
  if (roots.size() != sources.size() || first.size() != last.size()) { return false; }
  into.Visibility.assign(first.size(), 0u);
  for (const auto &root : sources) {
    if (root.BatchRow >= into.Arguments.size() / 5u) { return false; }
    into.Arguments[size_t{root.BatchRow} * 5u] = 0;
  }
  for (size_t at = 0; at < sources.size(); ++at) {
    if (roots[at] == 0) { continue; }
    const auto &root = sources[at];
    if (uint64_t{root.LogicalFirst} + root.TemplateCount > first.size() ||
        uint64_t{root.TemplateFirst} + root.TemplateCount > templates.size() ||
        root.BatchRow >= into.Arguments.size() / 5u) {
      return false;
    }
    uint64_t count = into.Arguments[size_t{root.BatchRow} * 5u];
    for (uint32_t one = 0; one < root.TemplateCount; ++one) {
      const size_t logical = size_t{root.LogicalFirst} + one;
      if ((first[logical] != 0 && last[logical] != 0) ||
          uint64_t{first[logical]} + last[logical] >
              templates[root.TemplateFirst + one].IndexCount) {
        return false;
      }
      into.Visibility[logical] = first[logical] + last[logical];
      into.PredictedIndices += first[logical];
      into.DeferredIndices += last[logical];
      count += into.Visibility[logical];
    }
    if (count > std::numeric_limits<uint32_t>::max()) { return false; }
    into.Arguments[size_t{root.BatchRow} * 5u] = static_cast<uint32_t>(count);
  }
  uint64_t count = 0;
  for (size_t row = 0; row < into.Arguments.size(); row += 5) {
    count += into.Arguments[row];
    into.Batches += into.Arguments[row] != 0 ? 1u : 0u;
  }
  if (count > std::numeric_limits<uint32_t>::max()) { return false; }
  into.Indices = static_cast<uint32_t>(count);
  return true;
}

bool CombinedIndices(Native::Device *device, const GeometryRenderer &geometry, KeptDraws &into) {
  const auto sources = geometry.SelectionRoots();
  const auto templates = geometry.SelectionTemplates();
  uint64_t end = 0;
  for (const auto &batch : geometry.Drawn()) {
    end = std::max(end, uint64_t{batch.FirstIndex} + batch.IndexCount);
  }
  for (const auto &root : sources) {
    for (uint32_t one = 0; one < root.TemplateCount; ++one) {
      const auto &item = templates[root.TemplateFirst + one];
      end = std::max(end, uint64_t{root.IndexBase} + item.RelativeFirstIndex + item.IndexCount);
    }
  }
  std::vector<uint32_t> indices;
  if (!ReadWords(device, geometry.Resident(), S::Index, end, indices, into)) { return false; }
  into.DrawIndex.reserve(into.Indices);
  size_t rootAt = 0;
  for (size_t row = 0; row < into.Arguments.size() / 5u; ++row) {
    const auto first = into.Arguments[row * 5u + 2u];
    const auto count = into.Arguments[row * 5u];
    into.Arguments[row * 5u + 2u] = static_cast<uint32_t>(into.DrawIndex.size());
    bool clustered = false;
    while (rootAt < sources.size() && sources[rootAt].BatchRow == row) {
      const auto &root = sources[rootAt++];
      clustered = true;
      for (uint32_t one = 0; one < root.TemplateCount; ++one) {
        const auto &item = templates[root.TemplateFirst + one];
        const size_t offset = size_t{root.IndexBase} + item.RelativeFirstIndex;
        const auto keep = into.Visibility[root.LogicalFirst + one];
        if (offset > indices.size() || keep > indices.size() - offset) { return false; }
        const auto selected = std::span(indices).subspan(offset, keep);
        into.DrawIndex.insert(into.DrawIndex.end(), selected.begin(), selected.end());
      }
    }
    if (!clustered && count != 0) {
      if (first > indices.size() || count > indices.size() - first) { return false; }
      const auto selected = std::span(indices).subspan(first, count);
      into.DrawIndex.insert(into.DrawIndex.end(), selected.begin(), selected.end());
    }
  }
  return rootAt == sources.size() && into.DrawIndex.size() == into.Indices;
}
}

ReadState SceneRenderer::ReadCombinedGeometrySelection(KeptDraws &into, GeometryReadback detail) {
  const auto &geometry = ActiveContent().Geometry();
  const auto &resident = geometry.Resident();
  std::vector<uint32_t> first;
  std::vector<uint32_t> last;
  std::vector<uint32_t> roots;
  std::vector<uint32_t> queue;
  into.TwoPhase = true;
  if (!ReadWords(
          Device_.get(), resident, S::ClusterFirstKept, geometry.ClusterJobs(), first, into) ||
      !ReadWords(Device_.get(), resident, S::ClusterKept, geometry.ClusterJobs(), last, into) ||
      !ReadWords(Device_.get(),
                 resident,
                 S::ClusterVisible,
                 geometry.SelectionRoots().size(),
                 roots,
                 into) ||
      !ReadWords(Device_.get(),
                 resident,
                 S::DrawArguments,
                 uint64_t{geometry.ClusterBatchRows()} * 5u,
                 into.Arguments,
                 into)) {
    into = {};
    return ReadState::Failed;
  }
  for (const auto &[stream, state] :
       std::array{std::pair{S::ClusterChunkState, &into.RootWork},
                  std::pair{S::ClusterSelectedState, &into.ClusterWork}}) {
    if (!ReadWords(
            Device_.get(), resident, stream, sizeof(*state) / sizeof(uint32_t), queue, into)) {
      into = {};
      return ReadState::Failed;
    }
    std::memcpy(state, queue.data(), sizeof(*state));
  }
  if (!CombineVisibility(geometry, first, last, roots, into) ||
      (detail == GeometryReadback::Indices && !CombinedIndices(Device_.get(), geometry, into))) {
    into = {};
    return ReadState::Failed;
  }
  if (detail == GeometryReadback::Counters) {
    into.Arguments.clear();
    into.Visibility.clear();
  }
  return ReadState::Ready;
}
}
