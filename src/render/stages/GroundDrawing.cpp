#include "TerrainLattice.h"
#include "TerrainRenderer.h"
#include <array>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <algorithm>

namespace outshine::Render {
Native::Result<void> TerrainRenderer::Draw(const PassRecording &into,
                                           Native::GraphicsPipeline *pipeline,
                                           const std::unique_ptr<Native::Buffer> &instances,
                                           uint32_t real,
                                           uint32_t virtual_,
                                           std::span<const Native::Binding> bindings,
                                           uint32_t first,
                                           bool skirtsOnly) const {
  if (real + virtual_ == 0) { return {}; }
  if (pipeline == nullptr || into.Commands == nullptr || !Resident_->Grid_ ||
      !Resident_->UniformGrid_ || !Resident_->Index_ || !instances || !Resident_->Pages_ ||
      !Resident_->Nearest_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground draw is missing native resources"});
  }
  constexpr size_t maximumBindings = LitShape.VertexSamplers + LitShape.VertexUniformBuffers +
                                     LitShape.FragmentSamplers + LitShape.FragmentUniformBuffers +
                                     LitShape.FragmentStorageBuffers;
  std::array<Native::Binding, maximumBindings> resources;
  if (bindings.size() >= resources.size()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground draw has too many shader bindings"});
  }
  std::ranges::copy(bindings, resources.begin());
  resources[bindings.size()] = {
      .Set = 0,
      .Slot = 0,
      .Value = Native::SampledTexture{
          .Image = {.Image = Resident_->Pages_.get(), .Layers = Resident_->PageLayers_},
          .Sampling = Resident_->Nearest_.get()}};
  const auto draw = [&](const Native::Buffer &grid, uint32_t start, uint32_t count) {
    if (count == 0) { return Native::Result<void>{}; }
    const std::array vertices{
        Native::VertexBuffer{
            .Stream = 0, .Data = &grid, .Range = {.Bytes = grid.Description().Bytes}},
        Native::VertexBuffer{.Stream = 1,
                             .Data = instances.get(),
                             .Range = {.Bytes = instances->Description().Bytes}}};
    return into.Commands->DrawIndexed(
        {.State = {.Pipeline = pipeline,
                   .Bindings = std::span(resources).first(bindings.size() + 1),
                   .Vertices = vertices,
                   .Area = into.Area,
                   .Clip = into.Clip},
         .Indices = {.Data = Resident_->Index_.get(),
                     .Range = {.Bytes = Resident_->Index_->Description().Bytes}},
         .Range = {.First = skirtsOnly ? TerrainLattice::kQuads * 6u : 0u,
                   .Count =
                       skirtsOnly ? TerrainLattice::kSkirtQuads * 6u : TerrainLattice::kIndices,
                   .Instances = count,
                   .FirstInstance = first + start}});
  };
  const auto accepted = draw(*Resident_->Grid_, 0, real);
  if (!accepted) { return accepted; }
  return draw(*Resident_->UniformGrid_, real, virtual_);
}

Native::Result<void> TerrainRenderer::Encode(const PassRecording &into,
                                             std::span<const Native::Binding> bindings) const {
  if (!DrawEnabled_) { return {}; }
  if (!Current() && Resident_->Instances() != 0) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Busy,
                      .Message = "ground inputs changed before visibility selection"});
  }
  const auto patched = DrawPatches(into, bindings);
  if (!patched) { return patched; }
  return Draw(into, Pipelines_->Lit(), Visible_, VisibleReal_, VisibleVirtual_, bindings, 0, true);
}

}
