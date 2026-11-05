#include "GeometrySnapshot.h"
#include "SceneRenderer.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
template <class Value>
bool Read(Native::Device *device,
          const GeometryResidency &resident,
          GeometryResidency::Stream stream,
          size_t count,
          std::vector<Value> &into,
          uint64_t &bytesRead) {
  if (count == 0) { return true; }
  const uint64_t bytes = uint64_t{count} * sizeof(Value);
  const auto &buffer = resident.Buffer(stream);
  if (!buffer || bytes > buffer->Description().Bytes ||
      bytes > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  Readback read;
  if (read.FromBuffer(device, buffer.get(), static_cast<uint32_t>(bytes)) != ReadState::Ready) {
    return false;
  }
  into.resize(count);
  std::memcpy(into.data(), read.Rows(), static_cast<size_t>(bytes));
  bytesRead += bytes;
  return true;
}
}

ReadState SceneRenderer::ReadGeometrySnapshot(GeometrySnapshot &into) {
  into = {};
  if (!ActiveState().Ready) { return ReadState::Failed; }
  const auto &draw = ActiveContent().Geometry();
  const auto &resident = draw.Resident();
  const auto &view = draw.Hierarchy().View;
  GeometrySnapshot snapshot;
  snapshot.Generation = draw.Generation();
  snapshot.SelectionReady = ActiveFrame().Cull.HasAcceptedSelection();
  const auto read = [&](auto stream, size_t count, auto &values) {
    return Read(Device_.get(), resident, stream, count, values, snapshot.ReadbackBytes);
  };
  using S = GeometryResidency::Stream;
  if (!read(S::ClusterRoots, view.RootCount, snapshot.Roots) ||
      !read(S::ClusterTemplates, view.TemplateCount, snapshot.Templates)) {
    return ReadState::Failed;
  }
  uint64_t end = 0;
  std::vector<std::pair<uint64_t, uint32_t>> ranges;
  const auto &batches = draw.Drawn();
  for (size_t at = 0; at < batches.size(); ++at) {
    const auto &batch = batches[at];
    ranges.emplace_back(batch.FirstIndex, batch.IndexCount);
    snapshot.Batches.push_back({batch.MaterialSlot,
                                static_cast<uint32_t>(draw.BatchLayoutAt(at)),
                                draw.BatchPipelineAt(at),
                                std::to_underlying(batch.Motion),
                                std::to_underlying(batch.Passes),
                                batch.Instances,
                                batch.FirstIndex,
                                batch.IndexCount,
                                batch.ModelSlot,
                                batch.FirstJob,
                                batch.JobCount,
                                static_cast<uint32_t>(draw.BatchSurfaceKindAt(at))});
  }
  for (const auto &root : snapshot.Roots) {
    if (root.TemplateFirst > snapshot.Templates.size() ||
        root.TemplateCount > snapshot.Templates.size() - root.TemplateFirst) {
      return ReadState::Failed;
    }
    for (const auto &one :
         std::span(snapshot.Templates).subspan(root.TemplateFirst, root.TemplateCount)) {
      ranges.emplace_back(uint64_t{root.IndexBase} + one.RelativeFirstIndex, one.IndexCount);
    }
  }
  for (const auto &[first, count] : ranges) { end = std::max(end, first + count); }
  std::vector<uint32_t> source;
  if (end > std::numeric_limits<size_t>::max() ||
      !read(S::Index, static_cast<size_t>(end), source)) {
    return ReadState::Failed;
  }
  snapshot.SourceIndices.resize(source.size(), 0);
  for (const auto &[first, count] : ranges) {
    const auto offset = static_cast<size_t>(first);
    std::ranges::copy(std::span(source).subspan(offset, count),
                      std::span(snapshot.SourceIndices).subspan(offset, count).begin());
  }
  if (snapshot.SelectionReady &&
      (!read(S::ClusterKept, view.LogicalCount, snapshot.Kept) ||
       !read(S::ClusterVisible, view.RootCount, snapshot.Visible) ||
       !read(S::DrawArguments, batches.size() * 5, snapshot.Arguments))) {
    return ReadState::Failed;
  }
  into = std::move(snapshot);
  return ReadState::Ready;
}
}
