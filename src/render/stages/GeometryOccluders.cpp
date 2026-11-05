#include "GeometryRenderer.h"
#include "ClusterDag.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Render {
namespace {
struct alignas(16) OcclusionView {
  Mat4f Clip;
  std::array<float, 4> Shift{};
};

static_assert(sizeof(OcclusionView) == 80 && alignof(OcclusionView) == 16);
static_assert(offsetof(OcclusionView, Clip) == 0 && offsetof(OcclusionView, Shift) == 64);
}

void GeometryRenderer::PrepareOccluders() {
  Occluders_.assign(Batches.size(), 0u);
  for (size_t at = 0; at < Batches.size(); ++at) {
    const auto &batch = Batches[at];
    const auto &surface = Slots[batch.MaterialSlot];
    if (batch.JobCount != 0 && batch.Motion == VertexMotion::Rigid &&
        batch.Passes != GeometryPasses::Shadow && surface.Domain == SurfaceDomain::Geometry &&
        surface.Kind == SurfaceKind::Opaque && CarriesNormal(batch.Layout)) {
      Occluders_[at] = static_cast<uint8_t>(1u + uint8_t{surface.CullsBack});
    }
  }
  for (const auto &root : TableRoots_) {
    bool eligible = (root.Flags & kGeometryUnboundedDeformation) == 0;
    for (uint32_t at = 0; eligible && at < root.TemplateCount; ++at) {
      const size_t sphere = size_t{TableTemplates_[root.TemplateFirst + at].SphereIndex} * 12;
      eligible = TableSpheres_[sphere + 8] == 0.0f && TableSpheres_[sphere + 9] >= kDagRootErr;
    }
    if (!eligible) { Occluders_[root.BatchRow] = 0; }
  }
}

Native::Result<void> GeometryRenderer::EncodeOccluders(
    const FrameContext &ctx,
    const PassRecording &into,
    const std::array<std::unique_ptr<Native::GraphicsPipeline>, 2> &pipelines,
    bool hasSelection) {
  OccluderWork_ = {};
  if (!hasSelection) { return {}; }
  using S = GeometryResidency::Stream;
  const auto *const index = Bound().Buffer(S::DrawIndex).get();
  const auto *const arguments = Bound().Buffer(S::DrawArguments).get();
  if (index == nullptr || arguments == nullptr || Occluders_.empty()) { return {}; }
  if (into.Commands == nullptr || !pipelines[0] || !pipelines[1]) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "geometry occlusion is not configured"});
  }
  OcclusionView view{.Clip = ctx.Mvp};
  for (size_t axis = 0; axis < 3; ++axis) {
    view.Shift[axis] = static_cast<float>(Anchor[axis] + ctx.PreViewTranslation[axis]);
  }
  for (size_t first = 0; first < Occluders_.size();) {
    const uint8_t kind = Occluders_[first];
    if (kind == 0) {
      ++first;
      continue;
    }
    size_t end = first + 1;
    while (end < Occluders_.size() && Occluders_[end] == kind) { ++end; }
    DrawResources resources;
    BindShadowResources(resources, Batches[first], std::as_bytes(std::span(&view, 1)));
    const auto drawn = into.Commands->DrawIndexedIndirect(
        {.State = {.Pipeline = pipelines[kind - 1].get(),
                   .Bindings = std::span(resources.Bindings).first(resources.BindingCount),
                   .Vertices = std::span(resources.Vertices).first(resources.VertexCount),
                   .Area = into.Area,
                   .Clip = into.Clip},
         .Indices = {.Data = index, .Range = {.Bytes = index->Description().Bytes}},
         .Range = {.Data = arguments,
                   .Offset = first * Native::kIndexedIndirectBytes,
                   .Count = static_cast<uint32_t>(end - first)}});
    if (!drawn) { return drawn; }
    ++OccluderWork_.DrawRequests;
    OccluderWork_.IndirectRecords += static_cast<uint32_t>(end - first);
    for (size_t at = first; at < end; ++at) {
      const auto &batch = Batches[at];
      OccluderWork_.IndirectInputTriangles += uint64_t{batch.IndexCount / 3} * batch.Instances;
      OccluderWork_.PlacementSlotLimit =
          std::max(OccluderWork_.PlacementSlotLimit, batch.ModelSlot + batch.Instances);
    }
    first = end;
  }
  return {};
}
}
