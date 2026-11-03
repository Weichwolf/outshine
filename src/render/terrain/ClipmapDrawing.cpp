#include "ClipmapRenderer.h"
#include "Gpu.h"
#include "MaterialBinding.h"
#include "PipelineShape.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
Native::Result<void> ClipmapRenderer::Validate(const ClipmapFields &fields,
                                               ClipmapPass pass) const {
  if (!Surface_ || !Depth_ || !Nearest_ || !Linear_ || fields.Rings.empty() ||
      fields.Heights == nullptr || fields.RingBuffer == nullptr ||
      fields.RingBuffer->Description().Bytes < fields.Rings.size_bytes()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "clipmap draw is missing prepared resources"});
  }
  const auto &height = fields.Heights->Description();
  if (height.Texels != Native::Format::R32Float ||
      height.Type != Native::TextureType::Image2DArray || height.Width != height.Height ||
      height.Layers < fields.Rings.size()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "clipmap height image has the wrong layout"});
  }
  if (pass == ClipmapPass::Surface) {
    if (fields.Normals == nullptr) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "clipmap surface has no fine normal field"});
    }
    const auto &normal = fields.Normals->Description();
    if ((normal.Texels != Native::Format::Rgba16Float &&
         normal.Texels != Native::Format::Rgba32Float) ||
        normal.Type != Native::TextureType::Image2DArray || normal.Width != normal.Height ||
        normal.Layers < fields.Rings.size() || normal.Width != 2 * height.Width - 3) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "clipmap normal image has the wrong layout"});
    }
  }
  for (size_t i = 0; i < fields.Rings.size(); ++i) {
    const auto &ring = fields.Rings[i];
    if (!std::cmp_equal(ring.HeightAddress[2], height.Width) ||
        !std::cmp_equal(ring.HeightAddress[3], i) ||
        ring.Morph[2] != static_cast<float>(HalfCells_ - 2) ||
        ring.CentreStep[3] != (i + 1 < fields.Rings.size() ? 1.0f : 0.0f) || ring.Offsets[0] < 0 ||
        ring.Offsets[0] > 1 || ring.Offsets[1] < 0 || ring.Offsets[1] > 1) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid,
                        .Message = "clipmap ring records do not match their topology"});
    }
  }
  return {};
}

Native::Result<void> ClipmapRenderer::Encode(const PassRecording &into,
                                             const ClipmapFields &fields,
                                             std::span<const Native::Binding> bindings,
                                             ClipmapPass pass) const {
  const auto valid = Validate(fields, pass);
  if (!valid) { return valid; }
  constexpr DrawShape shape{.VertexSamplers = 1,
                            .VertexUniformBuffers = 1,
                            .VertexStorageBuffers = 1,
                            .FragmentSamplers = 3,
                            .FragmentUniformBuffers = kGeometryFragmentUniformCount + 1,
                            .FragmentStorageBuffers = 3};
  constexpr size_t count = shape.VertexSamplers + shape.VertexUniformBuffers +
                           shape.VertexStorageBuffers + shape.FragmentSamplers +
                           shape.FragmentUniformBuffers + shape.FragmentStorageBuffers;
  std::array<Native::Binding, count> resources;
  const size_t added = pass == ClipmapPass::Surface ? 3 : 2;
  if (into.Commands == nullptr || bindings.size() + added > resources.size()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "clipmap frame bindings are incomplete or too large"});
  }
  std::ranges::copy(bindings, resources.begin());
  resources[bindings.size()] = {
      .Set = 0,
      .Slot = 0,
      .Value = Native::SampledTexture{
          .Image = {.Image = fields.Heights, .Layers = fields.Heights->Description().Layers},
          .Sampling = Nearest_.get()}};
  resources[bindings.size() + 1] = {
      .Set = 0,
      .Slot = 1,
      .Value = Native::StorageBuffer{.Data = fields.RingBuffer,
                                     .Range = {.Bytes = fields.Rings.size_bytes()}}};
  if (pass == ClipmapPass::Surface) {
    resources[bindings.size() + 2] = {
        .Set = 2,
        .Slot = 4,
        .Value = Native::SampledTexture{
            .Image = {.Image = fields.Normals, .Layers = fields.Normals->Description().Layers},
            .Sampling = Linear_.get()}};
  }
  for (size_t level = 0; level < fields.Rings.size(); ++level) {
    const size_t meshIndex = level == 0
                                 ? 0
                                 : 1u + static_cast<size_t>(fields.Rings[level - 1].Offsets[0] +
                                                            2 * fields.Rings[level - 1].Offsets[1]);
    const auto &mesh = Meshes_[meshIndex];
    const std::array vertices{
        Native::VertexBuffer{.Stream = 0,
                             .Data = mesh.Vertices.get(),
                             .Range = {.Bytes = mesh.Vertices->Description().Bytes}}};
    const auto drawn = into.Commands->DrawIndexed(
        {.State = {.Pipeline = pass == ClipmapPass::Surface ? Surface_.get() : Depth_.get(),
                   .Bindings = std::span(resources).first(bindings.size() + added),
                   .Vertices = vertices,
                   .Area = into.Area,
                   .Clip = into.Clip},
         .Indices = {.Data = mesh.Indices.get(),
                     .Range = {.Bytes = mesh.Indices->Description().Bytes}},
         .Range = {
             .Count = mesh.Count, .Instances = 1, .FirstInstance = static_cast<uint32_t>(level)}});
    if (!drawn) { return drawn; }
  }
  return {};
}
}
