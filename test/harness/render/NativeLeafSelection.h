#ifndef OUTSHINE_TEST_RENDER_NATIVELEAFSELECTION_H
#define OUTSHINE_TEST_RENDER_NATIVELEAFSELECTION_H

#include "NativeGpuCheck.h"
#include "src/render/stages/GeometryHierarchy.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Test::Gpu {

struct LeafSelectionInput {
  std::span<const Render::GeometryRoot> Roots;
  std::span<const Render::GeometryClusterTemplate> Templates;
  const Buffer &Spheres;
  const Buffer &Placements;
  const Buffer &Pyramid;
  std::span<const std::byte> View;
  std::span<const uint32_t> Initial;
};

template <class T, size_t N> std::vector<uint32_t> SelectionWords(std::span<const T, N> values) {
  std::vector<uint32_t> words(values.size_bytes() / sizeof(uint32_t));
  std::memcpy(words.data(), values.data(), values.size_bytes());
  return words;
}

inline std::unique_ptr<Buffer>
SelectAllLeaves(Device &device, Commands &commands, const LeafSelectionInput &input) {
  using namespace Render;
  std::vector<uint32_t> chunks;
  std::vector<uint32_t> prefix{0};
  uint32_t logical = 0;
  for (uint32_t at = 0; at < input.Roots.size(); ++at) {
    const auto &root = input.Roots[at];
    for (uint32_t first = 0; first < root.TemplateCount; first += kGeometryChunkClusters) {
      chunks.insert(chunks.end(), {at, first});
    }
    prefix.push_back(static_cast<uint32_t>(chunks.size() / 2));
    logical += root.TemplateCount;
  }
  CHECK(input.View.size() == 256 && logical > 0 && input.Initial.size() >= logical,
        "the leaf fixture supplies a complete view and guarded logical output");
  if (input.View.size() != 256 || logical == 0 || input.Initial.size() < logical) { Failed(); }
  auto roots = Upload(device, commands, SelectionWords(input.Roots));
  auto templates = Upload(device, commands, SelectionWords(input.Templates));
  auto work = Upload(device, commands, chunks);
  auto offsets = Upload(device, commands, prefix);
  const auto count = static_cast<uint32_t>(chunks.size() / 2);
  const std::array chunkState{GeometryQueueState{.Requested = count}};
  const std::array clusterState{GeometryQueueState{}};
  auto queued = Upload(device, commands, SelectionWords(std::span(chunkState)));
  auto selected = Upload(device, commands, SelectionWords(std::span(clusterState)));
  auto clusters = Upload(device, commands, std::vector<uint32_t>(size_t{logical} * 2));
  auto kept = Upload(device, commands, input.Initial);
  const GeometryHierarchyView hierarchy{.RootCount = static_cast<uint32_t>(input.Roots.size()),
                                        .TemplateCount =
                                            static_cast<uint32_t>(input.Templates.size()),
                                        .ChunkCapacity = count,
                                        .ClusterCapacity = logical,
                                        .WorstChunks = count,
                                        .GridWidth = 7};
  const std::array bindings{
      Storage(0, 0, *roots),
      Storage(0, 1, *templates),
      Storage(0, 2, input.Spheres),
      Storage(0, 3, input.Placements),
      Storage(0, 4, *work),
      Storage(0, 5, *queued),
      Storage(0, 6, *offsets),
      Storage(0, 7, input.Pyramid),
      Storage(1, 0, *kept),
      Storage(1, 1, *clusters),
      Storage(1, 2, *selected),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{input.View}},
      Binding{.Set = 2, .Slot = 1, .Value = Uniform{std::as_bytes(std::span(&hierarchy, 1))}}};
  auto pipeline = Require(device.CreateComputePipeline(Shader("geometryLeafCull.comp.spv")));
  Require(commands.Dispatch(
      *pipeline,
      bindings,
      {.X = std::min(count, hierarchy.GridWidth),
       .Y = count / hierarchy.GridWidth + uint32_t{count % hierarchy.GridWidth != 0}}));
  return kept;
}

}
#endif
