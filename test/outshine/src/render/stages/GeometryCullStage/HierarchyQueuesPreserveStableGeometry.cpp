#include "Check.h"
#include "GeometryHierarchyAdmission.h"
#include "GpuPlacement.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <print>
#include <span>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kGuard = 0xdeadbeefu;

struct alignas(16) CullView {
  std::array<float, 24> Planes{1, 0,  0, 1, -1, 0, 0, 1, 0, 1, 0,  1,
                               0, -1, 0, 1, 0,  0, 1, 0, 0, 0, -1, 1};
  std::array<float, 4> Shift{};
  uint32_t Jobs = 0;
  float ErrorPerMetre = 0;
  std::array<uint32_t, 2> Pad{};
  std::array<float, 16> Clip{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::array<uint32_t, 4> PyramidWide{1, 1, 1, 1};
  std::array<uint32_t, 4> PyramidHigh{1, 1, 1, 1};
  std::array<uint32_t, 4> PyramidAt{};
  uint32_t Occludes = 0;
  uint32_t SourceWide = 1;
  uint32_t SourceHigh = 1;
  uint32_t Reserved = 0;
};

static_assert(sizeof(CullView) == 256);
static_assert(offsetof(CullView, Clip) == 128 && offsetof(CullView, Occludes) == 240);

struct Inputs {
  std::vector<GeometryRoot> Roots;
  std::vector<GeometryClusterTemplate> Templates;
  std::vector<float> Spheres;
  std::vector<uint32_t> Batches, Arguments, Prefix{0}, Source;
  GpuPlacement Placement{.Current = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  uint32_t Logical = 0;
  uint32_t IndexWords = 0;
};

template <class T, size_t N> std::vector<uint32_t> Words(std::span<const T, N> values) {
  std::vector<uint32_t> words(values.size_bytes() / sizeof(uint32_t));
  std::memcpy(words.data(), values.data(), values.size_bytes());
  return words;
}

template <class T> Binding UniformAt(uint32_t slot, const T &value) {
  return {.Set = 2, .Slot = slot, .Value = Uniform{std::as_bytes(std::span(&value, 1))}};
}

void AddProduct(Inputs &inputs, uint32_t count) {
  const auto templateFirst = static_cast<uint32_t>(inputs.Templates.size());
  const auto batch = static_cast<uint32_t>(inputs.Batches.size() / 2);
  for (uint32_t region = 0; region < 2; ++region) {
    for (uint32_t at = 0; at < count; ++at) {
      const auto sphere = static_cast<uint32_t>(inputs.Spheres.size() / 12);
      const float x = region == 0 ? 0.0f : 4.0f;
      inputs.Spheres.insert(
          inputs.Spheres.end(),
          {x, 0, 0.5f, 0.1f, x, 0, 0.5f, 0.1f, at % 2 == 0 ? 0.0f : 2.0f, 1.0e30f, 0, 0});
      const auto first = static_cast<uint32_t>(inputs.Source.size());
      inputs.Templates.push_back(
          {.SphereIndex = sphere, .RelativeFirstIndex = first, .IndexCount = 3});
      for (uint32_t vertex = 0; vertex < 3; ++vertex) { inputs.Source.push_back(first + vertex); }
    }
  }
  inputs.Batches.insert(inputs.Batches.end(), {inputs.Logical, count * 4});
  inputs.Arguments.insert(inputs.Arguments.end(), {0, 1, inputs.IndexWords, 0, 1});
  for (uint32_t instance = 0; instance < 4; ++instance) {
    const uint32_t region = instance == 1 ? 1u : 0u;
    const uint32_t first = templateFirst + region * count;
    inputs.Roots.push_back(
        {.LocalSphere = {instance == 0 || instance == 3 ? 0.0f : 4.0f, 0, 0.5f, 0.1f},
         .TemplateFirst = first,
         .TemplateCount = count,
         .LogicalFirst = inputs.Logical,
         .BatchRow = batch,
         .ModelSlot = 1,
         .IndexCapacity = count * 3,
         .Flags = instance == 2 ? 0u : kGeometryRootBound});
    inputs.Logical += count;
    inputs.Prefix.push_back(inputs.Prefix.back() + count / 64 + uint32_t{count % 64 != 0});
    inputs.IndexWords += count * 3;
  }
}

Inputs Fixture() {
  Inputs inputs;
  for (const uint32_t count : {0u, 1u, 31u, 32u, 33u, 63u, 64u, 65u, 129u}) {
    AddProduct(inputs, count);
  }
  for (uint32_t at = 0; at < 8; ++at) { AddProduct(inputs, 0); }
  return inputs;
}

struct Buffers {
  std::unique_ptr<Buffer> Roots, Templates, Spheres, Placements, Prefix, Source, Pyramid;
  std::unique_ptr<Buffer> Chunks, ChunkState, Clusters, ClusterState, Visible;
  std::unique_ptr<Buffer> Kept, Slot, Arguments, Index;
};

Buffers Uploaded(Device &device, Commands &commands, const Inputs &inputs) {
  const auto upload = [&](const auto &values, BufferUse use = BufferUse::Storage) {
    return Upload(device, commands, Words(std::span(values)), use);
  };
  Buffers result;
  result.Roots = upload(inputs.Roots);
  result.Templates = upload(inputs.Templates);
  result.Spheres = upload(inputs.Spheres);
  auto wrongPlacement = inputs.Placement;
  wrongPlacement.Current[12] = 100;
  result.Placements = upload(std::array{wrongPlacement, inputs.Placement});
  result.Prefix = upload(inputs.Prefix);
  result.Source = upload(inputs.Source);
  result.Pyramid = upload(std::array{1.0f});
  result.Chunks = upload(std::vector<uint32_t>(inputs.Prefix.back() * 2 + 16, kGuard));
  result.Clusters = upload(std::vector<uint32_t>(inputs.Logical * 2 + 16, kGuard));
  const auto stateUse =
      static_cast<BufferUse>(static_cast<uint32_t>(BufferUse::Storage | BufferUse::Indirect));
  const std::array states{GeometryQueueState{}};
  result.ChunkState = Upload(device, commands, Words(std::span(states)), stateUse);
  result.ClusterState = Upload(device, commands, Words(std::span(states)), stateUse);
  result.Visible = upload(std::vector<uint32_t>(inputs.Roots.size() + 16, kGuard));
  result.Kept = upload(std::vector<uint32_t>(inputs.Logical + 16, kGuard));
  result.Slot = upload(std::vector<uint32_t>(inputs.Logical + 16, kGuard));
  result.Arguments = upload(inputs.Arguments);
  result.Index = upload(std::vector<uint32_t>(inputs.IndexWords + 16, kGuard));
  return result;
}

void Dispatch(Device &device,
              Commands &commands,
              const char *shader,
              std::span<const Binding> bindings,
              uint32_t groups,
              const Buffer *indirect = nullptr,
              uint32_t width = 7) {
  auto pipeline = Require(device.CreateComputePipeline(Shader(shader)));
  if (indirect != nullptr) {
    Require(commands.DispatchIndirect(*pipeline, bindings, *indirect, 16));
  } else {
    Require(commands.Dispatch(
        *pipeline,
        bindings,
        {.X = std::min(width, groups), .Y = groups / width + uint32_t{groups % width != 0}}));
  }
}

void Hierarchy(Device &device,
               Commands &commands,
               Buffers &b,
               const CullView &view,
               const GeometryHierarchyView &hierarchy,
               bool allRoots = false) {
  auto rootView = view;
  if (allRoots) { rootView.Planes.fill(0); }
  const std::array root{Storage(0, 0, *b.Roots),
                        Storage(0, 1, *b.Placements),
                        Storage(1, 0, *b.Chunks),
                        Storage(1, 1, *b.ChunkState),
                        Storage(1, 2, *b.Visible),
                        UniformAt(0, rootView),
                        UniformAt(1, hierarchy)};
  Dispatch(device,
           commands,
           "geometryRootCull.comp.spv",
           root,
           hierarchy.RootCount / 64 + uint32_t{hierarchy.RootCount % 64 != 0},
           nullptr,
           hierarchy.GridWidth);
  const std::array leaf{Storage(0, 0, *b.Roots),
                        Storage(0, 1, *b.Templates),
                        Storage(0, 2, *b.Spheres),
                        Storage(0, 3, *b.Placements),
                        Storage(0, 4, *b.Chunks),
                        Storage(0, 5, *b.ChunkState),
                        Storage(0, 6, *b.Prefix),
                        Storage(0, 7, *b.Pyramid),
                        Storage(1, 0, *b.Kept),
                        Storage(1, 1, *b.Clusters),
                        Storage(1, 2, *b.ClusterState),
                        UniformAt(0, view),
                        UniformAt(1, hierarchy)};
  Dispatch(device, commands, "geometryLeafCull.comp.spv", leaf, 0, b.ChunkState.get());
  const std::array scan{Storage(0, 0, *b.Roots),
                        Storage(0, 1, *b.Visible),
                        Storage(0, 2, *b.Kept),
                        Storage(1, 0, *b.Slot),
                        Storage(1, 1, *b.Arguments),
                        UniformAt(0, hierarchy)};
  Dispatch(device,
           commands,
           "geometryRootScan.comp.spv",
           scan,
           hierarchy.RootCount,
           nullptr,
           hierarchy.GridWidth);
  const std::array compact{Storage(0, 0, *b.Roots),
                           Storage(0, 1, *b.Templates),
                           Storage(0, 2, *b.Clusters),
                           Storage(0, 3, *b.ClusterState),
                           Storage(0, 4, *b.Slot),
                           Storage(0, 5, *b.Arguments),
                           Storage(0, 6, *b.Source),
                           Storage(0, 7, *b.Visible),
                           Storage(1, 0, *b.Index),
                           UniformAt(0, view),
                           UniformAt(1, hierarchy)};
  Dispatch(device, commands, "geometrySelectedCompact.comp.spv", compact, 0, b.ClusterState.get());
}

struct Readback {
  std::unique_ptr<Buffer> Data;

  std::vector<uint32_t> Read(Device &device) const {
    const auto mapped = Require(device.MapRead(*Data, {.Bytes = Data->Description().Bytes}));
    std::vector<uint32_t> words(mapped.size() / sizeof(uint32_t));
    std::memcpy(words.data(), mapped.data(), mapped.size());
    Require(device.Unmap(*Data));
    return words;
  }
};

Readback Download(Device &device, Commands &commands, const Buffer &from) {
  auto readback = Require(device.CreateBuffer({.Bytes = from.Description().Bytes,
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  Require(commands.Copy(from, *readback, {.Bytes = from.Description().Bytes}));
  return {.Data = std::move(readback)};
}

struct QueueExpectation {
  uint32_t Capacity;
  uint32_t Work;
  uint32_t Width;
};

void CheckState(std::span<const uint32_t> state, QueueExpectation expected) {
  const auto [capacity, work, width] = expected;
  CHECK(state.size() == 8 && state[0] <= work, "queue counts are bounded by admitted work");
  CHECK(state[1] == uint32_t{state[0] > capacity},
        "capacity exhaustion explicitly invalidates the queue");
  const uint32_t groups = state[1] == 0 ? state[0] : work;
  CHECK(state[4] == std::min(groups, width) &&
            state[5] == std::max(1u, groups / width + uint32_t{groups % width != 0}) &&
            state[6] == 1,
        "GPU-produced XYZ covers the valid queue or the full overflow fallback");
}

struct Case {
  uint32_t ChunkCapacity;
  uint32_t ClusterCapacity;
  float ErrorPerMetre = 0;
  bool Away = false;
  uint32_t GridWidth = 7;
  bool Reuse = false;
  bool AllBounds = false;
  bool Occludes = false;
  bool UnknownDeformation = false;
};

void Run(Device &device, const Inputs &inputs, Case test) {
  const auto [chunkCapacity,
              clusterCapacity,
              errorPerMetre,
              away,
              width,
              reuse,
              allBounds,
              occluded,
              deformed] = test;
  auto commands = Require(device.Begin());
  auto actual = Uploaded(device, *commands, inputs);
  auto reference = Uploaded(device, *commands, inputs);
  CullView view{.Jobs = inputs.Logical, .ErrorPerMetre = errorPerMetre};
  view.Occludes = uint32_t{occluded};
  if (away && !reuse) { view.Shift[0] = 100; }
  auto limits = device.Limits();
  limits.MaxWorkgroupCount[0] = width;
  const auto admitted = Require(
      AdmitGeometryHierarchy({.Roots = inputs.Roots,
                              .Templates = inputs.Templates,
                              .SphereCount = static_cast<uint32_t>(inputs.Spheres.size() / 12),
                              .PlacementCount = 2,
                              .SourceIndexCount = static_cast<uint32_t>(inputs.Source.size()),
                              .BatchCount = static_cast<uint32_t>(inputs.Batches.size() / 2)},
                             limits));
  CHECK(admitted.ChunkPrefix == inputs.Prefix && admitted.IndexWords == inputs.IndexWords,
        "CPU admission establishes the exact complete fallback prefix and destination capacity");
  auto hierarchy = admitted.View;
  hierarchy.ChunkCapacity = chunkCapacity;
  hierarchy.ClusterCapacity = clusterCapacity;
  Hierarchy(device, *commands, actual, view, hierarchy);
  auto referenceView = view;
  if (deformed) {
    referenceView.Planes.fill(0);
    referenceView.ErrorPerMetre = 0;
    referenceView.Occludes = 0;
  }
  Hierarchy(device, *commands, reference, referenceView, admitted.View, true);
  if (reuse) {
    const std::array states{GeometryQueueState{}};
    auto reset = Upload(device, *commands, Words(std::span(states)));
    Require(commands->Copy(*reset, *actual.ChunkState, {.Bytes = sizeof(GeometryQueueState)}));
    Require(commands->Copy(*reset, *actual.ClusterState, {.Bytes = sizeof(GeometryQueueState)}));
    Require(commands->Copy(*reset, *reference.ChunkState, {.Bytes = sizeof(GeometryQueueState)}));
    Require(commands->Copy(*reset, *reference.ClusterState, {.Bytes = sizeof(GeometryQueueState)}));
    view.Shift[0] = 100;
    Hierarchy(device, *commands, actual, view, hierarchy);
    Hierarchy(device, *commands, reference, view, admitted.View, true);
  }
  auto actualIndex = Download(device, *commands, *actual.Index);
  auto referenceIndex = Download(device, *commands, *reference.Index);
  auto actualArgs = Download(device, *commands, *actual.Arguments);
  auto referenceArgs = Download(device, *commands, *reference.Arguments);
  auto chunkState = Download(device, *commands, *actual.ChunkState);
  auto clusterState = Download(device, *commands, *actual.ClusterState);
  auto chunks = Download(device, *commands, *actual.Chunks);
  auto clusters = Download(device, *commands, *actual.Clusters);
  auto visible = Download(device, *commands, *actual.Visible);
  auto kept = Download(device, *commands, *actual.Kept);
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "both real GPU paths complete within the offline test budget");
  CHECK(actualIndex.Read(device) == referenceIndex.Read(device),
        "root filtering preserves the all-root reference index order and destination guards");
  CHECK(actualArgs.Read(device) == referenceArgs.Read(device),
        "multiple roots in one batch preserve all five original draw-argument words");
  const auto rootState = chunkState.Read(device);
  const auto leafState = clusterState.Read(device);
  CheckState(
      rootState,
      {.Capacity = chunkCapacity, .Work = inputs.Prefix.back(), .Width = hierarchy.GridWidth});
  CheckState(leafState,
             {.Capacity = clusterCapacity, .Work = inputs.Logical, .Width = hierarchy.GridWidth});
  CHECK(rootState[2] == inputs.Roots.size(), "each admitted root is examined exactly once");
  CHECK(leafState[2] == (deformed ? inputs.Logical
                         : away   ? (allBounds ? 0u : 418u)
                                  : 1254u),
        "rejected roots avoid all descendant cluster visits even during overflow fallback");
  CHECK(leafState[3] == (deformed             ? inputs.Logical
                         : away || occluded   ? 0u
                         : errorPerMetre == 0 ? 1254u
                                              : 636u),
        "qualified LOD/depth selection and unqualified deformation retain their contracts");
  for (auto [buffer, first] :
       std::array{std::pair{chunks.Read(device), static_cast<size_t>(chunkCapacity) * 2},
                  std::pair{clusters.Read(device), static_cast<size_t>(clusterCapacity) * 2},
                  std::pair{visible.Read(device), inputs.Roots.size()},
                  std::pair{kept.Read(device), static_cast<size_t>(inputs.Logical)}}) {
    CHECK(std::ranges::all_of(std::span(buffer).subspan(first),
                              [](uint32_t word) { return word == kGuard; }),
          "bounded queues and padded workgroups preserve all output guards");
  }
  std::println("HIERARCHY chunks={}/{} selected={}/{} visited={} lod={} away={}",
               rootState[0],
               chunkCapacity,
               leafState[0],
               clusterCapacity,
               leafState[2],
               errorPerMetre,
               away);
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  std::println("HIERARCHY_DEVICE {}", actual->Limits().Name);
  NativeOfflineDevice device(*actual);
  const auto inputs = Fixture();
  for (const float errorPerMetre : {0.0f, 1.0f}) {
    Run(device,
        inputs,
        {.ChunkCapacity = inputs.Prefix.back(),
         .ClusterCapacity = inputs.Logical,
         .ErrorPerMetre = errorPerMetre});
    Run(device,
        inputs,
        {.ChunkCapacity = 1, .ClusterCapacity = inputs.Logical, .ErrorPerMetre = errorPerMetre});
    Run(device,
        inputs,
        {.ChunkCapacity = inputs.Prefix.back(),
         .ClusterCapacity = 1,
         .ErrorPerMetre = errorPerMetre});
    Run(device, inputs, {.ChunkCapacity = 0, .ClusterCapacity = 0, .ErrorPerMetre = errorPerMetre});
  }
  Run(device,
      inputs,
      {.ChunkCapacity = inputs.Prefix.back(), .ClusterCapacity = inputs.Logical, .Away = true});
  Run(device,
      inputs,
      {.ChunkCapacity = inputs.Prefix.back(), .ClusterCapacity = inputs.Logical, .GridWidth = 1});
  Run(device, inputs, {.ChunkCapacity = 1, .ClusterCapacity = 1, .Away = true, .Reuse = true});
  auto known = inputs;
  for (auto &root : known.Roots) {
    if (root.Flags == 0) { root.LocalSphere[0] = 0; }
    root.Flags = kGeometryRootBound;
  }
  Run(device,
      known,
      {.ChunkCapacity = known.Prefix.back(),
       .ClusterCapacity = known.Logical,
       .Away = true,
       .Reuse = true,
       .AllBounds = true});
  Run(device,
      inputs,
      {.ChunkCapacity = inputs.Prefix.back(), .ClusterCapacity = inputs.Logical, .Occludes = true});
  Run(device, inputs, {.ChunkCapacity = 0, .ClusterCapacity = 0, .Occludes = true});
  auto deformed = inputs;
  for (auto &root : deformed.Roots) { root.Flags = kGeometryUnboundedDeformation; }
  Run(device,
      deformed,
      {.ChunkCapacity = deformed.Prefix.back(),
       .ClusterCapacity = deformed.Logical,
       .ErrorPerMetre = 1,
       .Away = true,
       .Occludes = true,
       .UnknownDeformation = true});
  return Report();
}
