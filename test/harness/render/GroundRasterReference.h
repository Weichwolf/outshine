#ifndef OUTSHINE_TEST_RENDER_GROUNDRASTERREFERENCE_H
#define OUTSHINE_TEST_RENDER_GROUNDRASTERREFERENCE_H

#include "GroundLattice.h"
#include "NativeGpuCheck.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Test::Gpu {
struct GroundReferenceInput {
  std::span<const float> Fractions;
  std::span<const float> Heights;
  std::span<const Render::GroundTile> Tiles;
};

inline std::vector<float> GroundControlGrid(std::span<const float> fractions) {
  constexpr uint32_t side = Render::GroundLattice::kSide;
  std::vector<float> grid;
  for (uint32_t variant = 0; variant < 2; ++variant) {
    for (uint32_t y = 0; y < side; ++y) {
      for (uint32_t x = 0; x < side; ++x) {
        const float u = variant == 0 ? fractions[x] : static_cast<float>(x) / (side - 1);
        const float v = variant == 0 ? fractions[y] : static_cast<float>(y) / (side - 1);
        grid.insert(grid.end(), {u, v, 0});
      }
    }
    const auto coordinate = [fractions, variant](uint32_t at) {
      return variant == 0 ? fractions[at] : static_cast<float>(at) / (side - 1);
    };
    for (uint32_t at = 0; at < side; ++at) { grid.insert(grid.end(), {coordinate(at), 0, 1}); }
    for (uint32_t at = 0; at < side; ++at) { grid.insert(grid.end(), {coordinate(at), 1, 1}); }
    for (uint32_t at = 0; at < side; ++at) { grid.insert(grid.end(), {0, coordinate(at), 1}); }
    for (uint32_t at = 0; at < side; ++at) { grid.insert(grid.end(), {1, coordinate(at), 1}); }
  }
  return grid;
}

inline std::vector<uint32_t> GroundControlIndices() {
  constexpr uint32_t side = Render::GroundLattice::kSide;
  std::vector<uint32_t> indices;
  for (uint32_t y = 0; y + 1 < side; ++y) {
    for (uint32_t x = 0; x + 1 < side; ++x) {
      const uint32_t a = y * side + x;
      indices.insert(indices.end(), {a, a + side, a + side + 1, a, a + side + 1, a + 1});
    }
  }
  for (uint32_t edge = 0; edge < 4; ++edge) {
    for (uint32_t at = 0; at + 1 < side; ++at) {
      const std::array a{at, (side - 1) * side + at + 1, (at + 1) * side, at * side + side - 1};
      const std::array b{at + 1, (side - 1) * side + at, at * side, (at + 1) * side + side - 1};
      const uint32_t first = side * side + edge * side + at;
      const bool reverse = edge == 1 || edge == 2;
      const uint32_t downA = first + (reverse ? 1u : 0u);
      const uint32_t downB = first + (reverse ? 0u : 1u);
      indices.insert(indices.end(), {a[edge], b[edge], downA, b[edge], downB, downA});
    }
  }
  return indices;
}

inline void DrawGroundReference(Device &device,
                                Commands &commands,
                                const Render::GroundPipelineBinding &pipelines,
                                const GroundReferenceInput &input,
                                std::span<const Binding> bindings,
                                uint32_t width,
                                uint32_t height,
                                const RenderPassDescription &pass) {
  using namespace Render;
  const auto grid = GroundControlGrid(input.Fractions);
  const auto indices = GroundControlIndices();
  const std::array tiles{input.Tiles[0].Instance, input.Tiles[1].Instance};
  const auto words = [](const auto &values) {
    return std::span(reinterpret_cast<const uint32_t *>(values.data()),
                     values.size() * sizeof(values[0]) / sizeof(uint32_t));
  };
  const auto vertices = Upload(device, commands, words(grid), BufferUse::Vertex);
  auto runs = Upload(device, commands, indices, BufferUse::Index);
  auto instances = Upload(device, commands, words(tiles), BufferUse::Vertex);
  const auto page = static_cast<uint32_t>(tiles[0].Page);
  auto pages = Require(
      device.CreateTexture({.Width = GroundLattice::kPageSide * GroundLattice::kPageColumns,
                            .Height = GroundLattice::kPageSide * GroundLattice::kPageColumns,
                            .Layers = page / GroundLattice::kPagesPerLayer + 1u,
                            .Type = TextureType::Image2DArray,
                            .Texels = Format::R32Float,
                            .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  const auto heights = Upload(device, commands, words(input.Heights));
  Require(commands.Copy(
      *heights,
      *pages,
      {.RowBytes = GroundLattice::kPageSide * sizeof(float),
       .Region = {.Layer = page / GroundLattice::kPagesPerLayer,
                  .X = (page % GroundLattice::kPageColumns) * GroundLattice::kPageSide,
                  .Y = ((page % GroundLattice::kPagesPerLayer) / GroundLattice::kPageColumns) *
                       GroundLattice::kPageSide,
                  .Width = GroundLattice::kPageSide,
                  .Height = GroundLattice::kPageSide}}));
  const auto sampler = Require(device.CreateSampler(
      {.Min = Render::Native::Filter::Nearest, .Mag = Render::Native::Filter::Nearest}));
  std::vector<Binding> resources(bindings.begin(), bindings.end());
  resources.push_back({.Set = 0,
                       .Slot = 0,
                       .Value = SampledTexture{
                           .Image = {.Image = pages.get(), .Layers = pages->Description().Layers},
                           .Sampling = sampler.get()}});
  Require(commands.BeginRenderPass(pass));
  for (uint32_t tile = 0; tile < 2; ++tile) {
    const std::array streams{
        VertexBuffer{
            .Stream = 0,
            .Data = vertices.get(),
            .Range = {.Offset = uint64_t{tile} * GroundLattice::kVertices * 3u * sizeof(float),
                      .Bytes = uint64_t{GroundLattice::kVertices} * 3u * sizeof(float)}},
        VertexBuffer{.Stream = 1,
                     .Data = instances.get(),
                     .Range = {.Bytes = instances->Description().Bytes}}};
    Require(commands.DrawIndexed(
        {.State = {.Pipeline = pipelines.Lit(),
                   .Bindings = resources,
                   .Vertices = streams,
                   .Area = {.Width = static_cast<float>(width),
                            .Height = static_cast<float>(height)},
                   .Clip = {.Width = width, .Height = height}},
         .Indices = {.Data = runs.get(), .Range = {.Bytes = runs->Description().Bytes}},
         .Range = {.Count = static_cast<uint32_t>(indices.size()), .FirstInstance = tile}}));
  }
  Require(commands.EndRenderPass());
}
}
#endif
