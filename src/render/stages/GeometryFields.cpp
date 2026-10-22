#include "GeometryRenderer.h"
#include <algorithm>
#include <array>
#include <expected>
#include <span>

namespace outshine::Render {
Native::Result<uint64_t> GeometryRenderer::PlaceField(const FieldMaskGpu &parameters,
                                                      std::span<const float> heights,
                                                      PieceSurface surface,
                                                      const GpuPlacement &placement) {
  const auto &slots =
      surface.From == PieceSurface::Source::Registered ? RegisteredSlotOf_ : SlotOf_;
  const auto materialSlot = surface.Index < slots.size() ? slots[surface.Index] : kNoSlot;
  if (Bound().Device() == nullptr || materialSlot >= Slots.size() ||
      Slots[materialSlot].Kind != SurfaceKind::Opaque ||
      Slots[materialSlot].Domain != SurfaceDomain::Subject || !Slots[materialSlot].CullsBack ||
      Slots[materialSlot].Unlit) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "field requires a device and an opaque lit back-culled material"});
  }
  return Bound().Fields.Place(
      *Bound().Device(), Bound().Uploads(), parameters, heights, materialSlot, placement);
}

Native::Result<void> GeometryRenderer::EncodeFields(const PassRecording &into,
                                                    const FrameUniforms &uniforms) {
  if (Binding().Behind != nullptr || Bound().Fields.Drawn().empty()) { return {}; }
  if (into.Commands == nullptr || !Binding().FieldColour) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "field colour pass is unavailable"});
  }
  for (const auto &field : Bound().Fields.Drawn()) {
    if (field.IndexCount == 0) { continue; }
    if (field.MaterialSlot >= Slots.size()) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "field material is no longer resident"});
    }
    DrawResources resources;
    const auto bindings = field.Bindings();
    resources.Bindings[0] = bindings[0];
    resources.Bindings[1] = {.Set = 1,
                             .Slot = 0,
                             .Value =
                                 Native::Uniform{.Bytes = std::as_bytes(std::span(uniforms.View))}};
    BindSlot(resources, field.MaterialSlot, VertexLayout::PositionNormal, uniforms);
    resources.Bindings[resources.BindingCount++] = bindings[1];
    resources.Bindings[resources.BindingCount++] = bindings[2];
    auto recorded = field.Encode(*into.Commands,
                                 *Binding().FieldColour,
                                 std::span(resources.Bindings).first(resources.BindingCount),
                                 into.Area,
                                 into.Clip);
    if (!recorded) { return recorded; }
    ++EncodedWork_.DrawRequests;
    EncodedWork_.DirectTriangles += field.IndexCount / 3;
    EncodedWork_.SurfaceSlotLimit = std::max(EncodedWork_.SurfaceSlotLimit, field.MaterialSlot + 1);
  }
  return {};
}

Native::Result<uint32_t>
GeometryRenderer::EncodeFieldShadow(const PassRecording &into,
                                    const Native::GraphicsPipeline &pipeline,
                                    std::span<const std::byte> view) const {
  if (into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "field caster pass is unavailable"});
  }
  uint32_t draws = 0;
  for (const auto &field : Bound().Fields.Drawn()) {
    if (field.IndexCount == 0) { continue; }
    const auto resources = field.Bindings();
    const std::array bindings{
        resources[0],
        resources[1],
        resources[2],
        Native::Binding{.Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = view}}};
    auto recorded = field.Encode(*into.Commands, pipeline, bindings, into.Area, into.Clip);
    if (!recorded) { return std::unexpected(recorded.error()); }
    ++draws;
  }
  return draws;
}
}
