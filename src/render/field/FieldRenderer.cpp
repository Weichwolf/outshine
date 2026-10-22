#include "FieldRenderer.h"
#include "UploadQueue.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
Native::Result<std::unique_ptr<Native::Buffer>>
Buffer(Native::Device &device, uint64_t bytes, Native::BufferUse use) {
  return device.CreateBuffer({.Bytes = bytes, .Use = use | Native::BufferUse::CopyDestination});
}

Native::Result<void>
Allocate(Native::Device &device, FieldDraw &draw, const FieldSurface &surface) {
  auto vertices =
      Buffer(device, surface.Vertices.size() * sizeof(FieldVertex), Native::BufferUse::Vertex);
  if (!vertices) { return std::unexpected(vertices.error()); }
  auto indices =
      Buffer(device, surface.Indices.size() * sizeof(uint32_t), Native::BufferUse::Index);
  if (!indices) { return std::unexpected(indices.error()); }
  auto placement = Buffer(device, sizeof(GpuPlacement), Native::BufferUse::Storage);
  if (!placement) { return std::unexpected(placement.error()); }
  auto heights = device.CreateTexture(
      {.Width = draw.Parameters.Shape[0],
       .Height = draw.Parameters.Shape[1],
       .Texels = Native::Format::R32Float,
       .Use = Native::TextureUse::Sampled | Native::TextureUse::CopyDestination});
  if (!heights) { return std::unexpected(heights.error()); }
  auto sampling = device.CreateSampler({.Min = Native::Filter::Nearest,
                                        .Mag = Native::Filter::Nearest,
                                        .Mip = Native::Filter::Nearest});
  if (!sampling) { return std::unexpected(sampling.error()); }
  draw.Vertices = std::move(*vertices);
  draw.Indices = std::move(*indices);
  draw.Placement = std::move(*placement);
  draw.Heights = std::move(*heights);
  draw.Sampling = std::move(*sampling);
  return {};
}

Native::Result<void> Upload(Native::Device &device,
                            UploadQueue *uploads,
                            const FieldDraw &draw,
                            const FieldSurface &surface,
                            std::span<const float> heights,
                            const GpuPlacement &placement) {
  const std::array data{std::as_bytes(std::span(surface.Vertices)),
                        std::as_bytes(std::span(surface.Indices)),
                        std::as_bytes(std::span(&placement, 1)),
                        std::as_bytes(heights)};
  size_t bytes = 0;
  for (const auto part : data) { bytes += part.size(); }
  auto staging = device.CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::CopySource, .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapping = device.MapWrite(**staging, {.Bytes = bytes});
  if (!mapping) { return std::unexpected(mapping.error()); }
  size_t offset = 0;
  for (const auto part : data) {
    std::memcpy(mapping->data() + offset, part.data(), part.size());
    offset += part.size();
  }
  auto accepted = device.Unmap(**staging);
  if (!accepted) { return accepted; }
  auto commands = BeginUpload(device, uploads);
  if (!commands) { return std::unexpected(commands.error()); }
  const std::array targets{draw.Vertices.get(), draw.Indices.get(), draw.Placement.get()};
  offset = 0;
  for (size_t at = 0; at < targets.size(); ++at) {
    accepted = (*commands)->Copy(
        **staging, *targets[at], {.SourceOffset = offset, .Bytes = data[at].size()});
    if (!accepted) { return accepted; }
    offset += data[at].size();
  }
  accepted = (*commands)->Copy(
      **staging,
      *draw.Heights,
      {.BufferOffset = offset,
       .RowBytes = uint64_t{draw.Parameters.Shape[0]} * sizeof(float),
       .Region = {.Width = draw.Parameters.Shape[0], .Height = draw.Parameters.Shape[1]}});
  if (!accepted) { return accepted; }
  auto submitted = SubmitUpload(device, uploads, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  return {};
}
}

Native::Result<uint64_t> FieldRenderer::Place(Native::Device &device,
                                              UploadQueue *uploads,
                                              const FieldMaskGpu &parameters,
                                              std::span<const float> heights,
                                              uint32_t materialSlot,
                                              const GpuPlacement &placement) {
  if (!std::ranges::all_of(placement.Current, [](float v) { return std::isfinite(v); }) ||
      !std::ranges::all_of(placement.Previous, [](float v) { return std::isfinite(v); })) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "field placement is nonfinite"});
  }
  auto surface = BuildFieldSurface(parameters, heights);
  if (!surface) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "field dimensions, metric or heights are invalid"});
  }
  FieldDraw draw;
  draw.Parameters = parameters;
  draw.MaterialSlot = materialSlot;
  draw.IndexCount = static_cast<uint32_t>(surface->Indices.size());
  if (draw.IndexCount != 0) {
    auto accepted = Allocate(device, draw, *surface);
    if (!accepted) { return std::unexpected(accepted.error()); }
    accepted = Upload(device, uploads, draw, *surface, heights, placement);
    if (!accepted) { return std::unexpected(accepted.error()); }
  }
  draw.Id = ++Generation_;
  Draws_.push_back(std::move(draw));
  return Generation_;
}

bool FieldRenderer::Release(uint64_t id) {
  const auto count = std::erase_if(Draws_, [id](const FieldDraw &draw) { return draw.Id == id; });
  if (count != 0) { ++Generation_; }
  return count != 0;
}

FieldRenderer::AllocationStats FieldRenderer::Allocations() const {
  AllocationStats stats;
  for (const auto &draw : Draws_) {
    if (draw.IndexCount == 0) { continue; }
    stats.VertexBytes += draw.Vertices->Description().Bytes;
    stats.IndexBytes += draw.Indices->Description().Bytes;
    stats.PlacementBytes += draw.Placement->Description().Bytes;
    stats.HeightPayloadBytes +=
        uint64_t{draw.Parameters.Shape[0]} * draw.Parameters.Shape[1] * sizeof(float);
  }
  return stats;
}

std::array<Native::Binding, 3> FieldDraw::Bindings() const {
  return {
      Native::Binding{.Set = 0,
                      .Slot = 0,
                      .Value = Native::StorageBuffer{.Data = Placement.get(),
                                                     .Range = {.Bytes = sizeof(GpuPlacement)}}},
      Native::Binding{.Set = 2,
                      .Slot = 12,
                      .Value = Native::SampledTexture{.Image = {.Image = Heights.get()},
                                                      .Sampling = Sampling.get()}},
      Native::Binding{.Set = 3,
                      .Slot = 2,
                      .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&Parameters, 1))}}};
}

Native::Result<void> FieldDraw::Encode(Native::Commands &commands,
                                       const Native::GraphicsPipeline &pipeline,
                                       std::span<const Native::Binding> bindings,
                                       Native::Viewport area,
                                       Native::Scissor clip) const {
  if (IndexCount == 0) { return {}; }
  const std::array vertices{Native::VertexBuffer{
      .Stream = 0, .Data = Vertices.get(), .Range = {.Bytes = Vertices->Description().Bytes}}};
  return commands.DrawIndexed(
      {.State = {.Pipeline = &pipeline,
                 .Bindings = bindings,
                 .Vertices = vertices,
                 .Area = area,
                 .Clip = clip},
       .Indices = {.Data = Indices.get(), .Range = {.Bytes = Indices->Description().Bytes}},
       .Range = {.Count = IndexCount}});
}
}
