#include "GeometryRenderer.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
Native::Binding PrismStorage(uint32_t set, uint32_t slot, const Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = Native::StorageBuffer{.Data = &buffer,
                                         .Range = {.Bytes = buffer.Description().Bytes}}};
}
}

Native::Result<uint64_t> GeometryRenderer::PlacePrismGrid(std::shared_ptr<const PrismGrid> source,
                                                          std::span<const PrismInstance> instances,
                                                          uint32_t material) {
  if (material >= RegisteredSlotOf_.size() || RegisteredSlotOf_[material] >= Slots.size()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism material is not registered"});
  }
  const uint32_t slot = RegisteredSlotOf_[material];
  const auto &surface = Slots[slot];
  if (surface.Kind != SurfaceKind::Opaque || surface.Domain != SurfaceDomain::Subject ||
      surface.Unlit.has_value()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Unsupported,
                      .Message = "prism grids require an opaque metallic-roughness material"});
  }
  return Prisms_.Place(std::move(source), instances, slot);
}

Native::Result<void>
GeometryRenderer::SetPrismGridInstances(uint64_t id, std::span<const PrismInstance> instances) {
  return Prisms_.SetInstances(id, instances);
}

void GeometryRenderer::ReleasePrismGrid(uint64_t id) {
  (void)Prisms_.Release(id);
}

Native::Result<void> GeometryRenderer::EncodePrisms(const PassRecording &into,
                                                    const FrameUniforms &uniforms) {
  if (Binding().Behind != nullptr) { return {}; }
  for (const auto &product : Prisms_.Products()) {
    if (product.Records.empty()) { continue; }
    if (into.Commands == nullptr || Binding().Prisms.Colour() == nullptr ||
        product.Material >= Slots.size()) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "prism colour resources are unavailable"});
    }
    const auto &surface = Slots[product.Material];
    if (surface.Kind != SurfaceKind::Opaque || surface.Domain != SurfaceDomain::Subject ||
        surface.Unlit.has_value()) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Unsupported,
                        .Message = "prism material is no longer opaque metallic-roughness"});
    }
    const auto view = std::as_bytes(std::span(uniforms.View));
    const std::array area{into.Area.X, into.Area.Y, into.Area.Width, into.Area.Height};
    DrawResources resources;
    resources.Bindings[0] = PrismStorage(0, 0, *product.Buffers[1]);
    resources.Bindings[1] = {.Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = view}};
    resources.Bindings[2] = {.Set = 3, .Slot = 2, .Value = Native::Uniform{.Bytes = view}};
    resources.Bindings[3] = {
        .Set = 3, .Slot = 3, .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(area))}};
    BindSlot(resources, product.Material, VertexLayout::PositionNormalUvTangent, uniforms, 4);
    resources.Bindings[resources.BindingCount++] = PrismStorage(4, 3, *product.Buffers[0]);
    resources.Bindings[resources.BindingCount++] = PrismStorage(4, 4, *product.Buffers[1]);
    auto drawn = PrismRenderer::Encode(product,
                                       *into.Commands,
                                       *Binding().Prisms.Colour(),
                                       std::span(resources.Bindings).first(resources.BindingCount),
                                       into.Area,
                                       into.Clip);
    if (!drawn) { return drawn; }
    ++EncodedWork_.DrawRequests;
    constexpr uint32_t proxyTriangles = 12;
    EncodedWork_.DirectTriangles += product.Records.size() * proxyTriangles;
    EncodedWork_.SurfaceSlotLimit = std::max(EncodedWork_.SurfaceSlotLimit, product.Material + 1);
  }
  return {};
}

Native::Result<void> GeometryRenderer::EncodePrismShadow(const PassRecording &into,
                                                         const PrismDepthView &view) const {
  if (Prisms_.Products().empty()) { return {}; }
  if (into.Commands == nullptr || Binding().Prisms.Depth() == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism shadow resources are unavailable"});
  }
  return Prisms_.EncodeDepth(*into.Commands, *Binding().Prisms.Depth(), view, into.Area, into.Clip);
}
}
