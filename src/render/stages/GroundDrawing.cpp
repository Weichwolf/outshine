#include "GroundLattice.h"
#include <array>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <algorithm>

namespace outshine::Render {
Native::Result<void> GroundLattice::Draw(const PassRecording &into,
                                         Native::GraphicsPipeline *pipeline,
                                         const std::unique_ptr<Native::Buffer> &instances,
                                         uint32_t real,
                                         uint32_t virtual_,
                                         std::span<const Native::Binding> bindings,
                                         uint32_t first) const {
  if (real + virtual_ == 0) { return {}; }
  if (pipeline == nullptr || into.Commands == nullptr || !Grid_ || !UniformGrid_ || !Index_ ||
      !instances || !Pages_ || !Nearest_) {
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
      .Value = Native::SampledTexture{.Image = {.Image = Pages_.get(), .Layers = PageLayers_},
                                      .Sampling = Nearest_.get()}};
  const auto draw = [&](Native::Buffer &grid, uint32_t start, uint32_t count) {
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
         .Indices = {.Data = Index_.get(), .Range = {.Bytes = Index_->Description().Bytes}},
         .Range = {.Count = kIndices, .Instances = count, .FirstInstance = first + start}});
  };
  const auto accepted = draw(*Grid_, 0, real);
  if (!accepted) { return accepted; }
  return draw(*UniformGrid_, real, virtual_);
}

Native::Result<void> GroundLattice::Encode(const PassRecording &into,
                                           std::span<const Native::Binding> bindings) const {
  if (!DrawEnabled_) { return {}; }
  return Draw(into, Pipelines_->Lit(), Visible_, VisibleReal_, VisibleVirtual_, bindings);
}

}
