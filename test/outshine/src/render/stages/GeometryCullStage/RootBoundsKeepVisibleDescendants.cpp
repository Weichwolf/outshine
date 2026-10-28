#include "BatchVisibility.h"
#include "Check.h"
#include "GpuPlacement.h"
#include "GeometryHierarchy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeLeafSelection.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr size_t kSphereFloats = 12;
constexpr uint32_t kRejected = 0xdeadbeefu;

struct alignas(16) View {
  std::array<float, 24> Planes{1, 0,  0, 1, -1, 0, 0, 1, 0, 1, 0,  1,
                               0, -1, 0, 1, 0,  0, 1, 0, 0, 0, -1, 1};
  std::array<float, 4> Shift{};
  uint32_t Jobs = 0;
  float ErrorPerMetre = 0;
  std::array<std::byte, 136> Rest{};
};

static_assert(sizeof(View) == 256 && alignof(View) == 16);
static_assert(offsetof(View, Jobs) == 112 && offsetof(View, ErrorPerMetre) == 116);

struct Witness {
  std::array<bool, 2> Visible{};
  std::array<Vec3f, 2> Points{};
  bool UnknownModel = false;
};

struct Inputs {
  std::vector<float> Spheres;
  std::vector<GeometryRoot> Roots;
  std::vector<GeometryClusterTemplate> Templates;
  std::vector<GpuPlacement> Placements;
  std::vector<Witness> Cases;
};

bool Inside(const GpuPlacement &placement,
            const Vec3f &point,
            const std::array<float, 4> &shift = {}) {
  std::array<double, 3> exact{};
  for (size_t axis = 0; axis < exact.size(); ++axis) {
    exact[axis] = double{placement.Current[12 + axis]} + shift[axis];
    for (size_t column = 0; column < 3; ++column) {
      exact[axis] += double{placement.Current[column * 4 + axis]} * point[column];
    }
  }
  return std::ranges::all_of(exact, [](double value) { return std::isfinite(value); }) &&
         std::abs(exact[0]) <= 1 && std::abs(exact[1]) <= 1 && exact[2] >= 0 && exact[2] <= 1;
}

void Add(Inputs &inputs, float distance, float scale, float shear, float target) {
  const auto batch = static_cast<uint32_t>(inputs.Cases.size());
  std::array<DagCluster, 2> leaves{};
  for (size_t at = 0; at < leaves.size(); ++at) {
    leaves[at].SelfCenter = {{at == 0 ? -distance : distance, 0, 0}};
    leaves[at].SelfRadius = std::max(distance * 0.1f, 0.0001f);
    leaves[at].ParentErr = kDagRootErr;
    leaves[at].First = static_cast<uint32_t>(at * 3);
    leaves[at].Count = 3;
  }
  const auto root = LocalSphereOf(leaves, 6);
  CHECK(root.has_value(), "valid source cluster intervals provide a conservative local root");
  if (!root) { Failed(); }
  GpuPlacement placement;
  placement.Current = {scale,
                       0,
                       0,
                       0,
                       shear,
                       scale,
                       0,
                       0,
                       0,
                       shear * 0.5f,
                       scale,
                       0,
                       target + scale * distance,
                       0,
                       0.5f,
                       1};
  Witness witness;
  for (size_t at = 0; at < leaves.size(); ++at) {
    witness.Points[at] = leaves[at].SelfCenter;
    witness.Visible[at] = Inside(placement, leaves[at].SelfCenter);
  }
  inputs.Cases.push_back(witness);
  inputs.Placements.push_back(placement);
  inputs.Roots.push_back({.TemplateFirst = batch * 3,
                          .TemplateCount = 3,
                          .LogicalFirst = batch * 3,
                          .ModelSlot = batch});
  std::array<BatchSphere, 3> spheres{
      *root,
      BatchSphere{leaves[0].SelfCenter[0], 0, 0, leaves[0].SelfRadius},
      BatchSphere{leaves[1].SelfCenter[0], 0, 0, leaves[1].SelfRadius}};
  for (size_t at = 0; at < spheres.size(); ++at) {
    const auto sphere = static_cast<uint32_t>(inputs.Spheres.size() / kSphereFloats);
    inputs.Spheres.insert(inputs.Spheres.end(), spheres[at].begin(), spheres[at].end());
    inputs.Spheres.insert(inputs.Spheres.end(), {0, 0, 0, 0, 0, kDagRootErr, 0, 0});
    inputs.Templates.push_back({.SphereIndex = sphere, .IndexCount = at == 0 ? 6u : 3u});
  }
}

Inputs Cases() {
  Inputs inputs;
  for (const float distance : {0.01f, 1.0f, 1000.0f, 1000000.0f}) {
    for (const float scale : {0.0f, 0.001f, 1.0f, -2.0f, 1024.0f, 0x1p-20f}) {
      for (const float shear : {0.0f, 0.25f, 4.0f, -16.0f}) {
        for (const float target : {-3.0f, -1.001f, -0.9f, 0.0f, 0.9f, 1.001f, 3.0f}) {
          Add(inputs, distance, scale, shear, target);
        }
      }
    }
  }
  for (const bool projective : {false, true}) {
    Add(inputs, 1, 1, 0, 3);
    inputs.Placements.back().Current[projective ? 3 : 0] =
        projective ? 0.125f : std::numeric_limits<float>::quiet_NaN();
    inputs.Cases.back().UnknownModel = true;
    inputs.Cases.back().Visible = {};
  }
  return inputs;
}

template <class T> std::vector<uint32_t> Words(std::span<const T> values) {
  std::vector<uint32_t> words(values.size_bytes() / sizeof(uint32_t));
  std::memcpy(words.data(), values.data(), values.size_bytes());
  return words;
}

void CheckResults(const Inputs &inputs, std::span<const uint32_t> kept, bool rebased) {
  size_t visible = 0;
  size_t rejected = 0;
  for (size_t at = 0; at < inputs.Cases.size(); ++at) {
    const auto root = kept[at * 3];
    rejected += root == 0 ? 1u : 0u;
    CHECK(root == 0 || root == 6, "the actual GPU root result is initialized and bounded");
    const auto &witness = inputs.Cases[at];
    if (witness.UnknownModel) {
      CHECK(root == 6 && kept[at * 3 + 1] == 3 && kept[at * 3 + 2] == 3,
            "unknown projective or nonfinite transforms remain fail-open");
    }
    for (size_t leaf = 0; leaf < witness.Visible.size(); ++leaf) {
      if (!witness.Visible[leaf]) { continue; }
      ++visible;
      CHECK(root == 6 && kept[at * 3 + leaf + 1] == 3,
            "a root cannot reject a descendant with an independently visible source vertex");
    }
  }
  CHECK(visible > 100 && (rebased || rejected > 10),
        "the regression includes both independently visible geometry and actual root rejection");
  std::println("ROOT_BOUNDS rebased={} cases={} visible_witnesses={} roots_rejected={}",
               rebased,
               inputs.Cases.size(),
               visible,
               rejected);
  CHECK(std::ranges::all_of(kept.subspan(inputs.Cases.size() * 3),
                            [](uint32_t value) { return value == kRejected; }),
        "rounded workgroups do not overwrite the output guard range");
}

std::unique_ptr<Buffer> HierarchyBounds(Device &device,
                                        Commands &commands,
                                        const Inputs &inputs,
                                        const View &view,
                                        const Buffer &placements) {
  std::vector<GeometryRoot> roots;
  for (uint32_t at = 0; at < inputs.Cases.size(); ++at) {
    const size_t first = size_t{at} * 3 * kSphereFloats;
    roots.push_back({.LocalSphere = {inputs.Spheres[first],
                                     inputs.Spheres[first + 1],
                                     inputs.Spheres[first + 2],
                                     inputs.Spheres[first + 3]},
                     .TemplateCount = 1,
                     .LogicalFirst = at,
                     .BatchRow = at,
                     .ModelSlot = at,
                     .Flags = kGeometryRootBound});
  }
  auto rootBuffer = Upload(device, commands, Words(std::span<const GeometryRoot>(roots)));
  const auto count = static_cast<uint32_t>(roots.size());
  const std::vector<uint32_t> initial(count + 16, kRejected);
  auto visible = Upload(device, commands, initial);
  auto chunks = Upload(device, commands, std::vector<uint32_t>(size_t{count} * 2, kRejected));
  const std::array<GeometryQueueState, 1> state{};
  auto queueState = Upload(device, commands, Words(std::span<const GeometryQueueState>(state)));
  const GeometryHierarchyView hierarchy{
      .RootCount = count, .ChunkCapacity = count, .WorstChunks = count, .GridWidth = 7};
  auto pipeline = Require(device.CreateComputePipeline(Shader("geometryRootCull.comp.spv")));
  const std::array bindings{
      Storage(0, 0, *rootBuffer),
      Storage(0, 1, placements),
      Storage(1, 0, *chunks),
      Storage(1, 1, *queueState),
      Storage(1, 2, *visible),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&view, 1))}},
      Binding{.Set = 2, .Slot = 1, .Value = Uniform{std::as_bytes(std::span(&hierarchy, 1))}}};
  const uint32_t groups = count / 64 + uint32_t{count % 64 != 0};
  Require(commands.Dispatch(
      *pipeline,
      bindings,
      {.X = std::min(groups, hierarchy.GridWidth),
       .Y = groups / hierarchy.GridWidth + uint32_t{groups % hierarchy.GridWidth != 0}}));
  auto readback = Require(device.CreateBuffer({.Bytes = visible->Description().Bytes,
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  Require(commands.Copy(*visible, *readback, {.Bytes = visible->Description().Bytes}));
  return readback;
}

void Run(Device &device, const Inputs &inputs, const std::array<float, 4> &shift = {}) {
  auto commands = Require(device.Begin());
  const auto sphereWords = Words(std::span(inputs.Spheres));
  const auto placementWords = Words(std::span(inputs.Placements));
  auto spheres = Upload(device, *commands, sphereWords);
  auto placements = Upload(device, *commands, placementWords);
  const std::array<uint32_t, 1> zero{};
  auto pyramid = Upload(device, *commands, zero);
  const std::vector<uint32_t> initial(inputs.Cases.size() * 3 + 16, kRejected);
  View view;
  view.Shift = shift;
  view.Jobs = static_cast<uint32_t>(inputs.Cases.size() * 3);
  auto kept = SelectAllLeaves(device,
                              *commands,
                              {.Roots = inputs.Roots,
                               .Templates = inputs.Templates,
                               .Spheres = *spheres,
                               .Placements = *placements,
                               .Pyramid = *pyramid,
                               .View = std::as_bytes(std::span(&view, 1)),
                               .Initial = initial});
  auto hierarchyReadback = HierarchyBounds(device, *commands, inputs, view, *placements);
  auto readback = Require(device.CreateBuffer({.Bytes = kept->Description().Bytes,
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  Require(commands->Copy(*kept, *readback, {.Bytes = kept->Description().Bytes}));
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the actual GPU root/leaf selection finishes within the explicit offline budget");
  const auto bytes = Require(device.MapRead(*readback, {.Bytes = readback->Description().Bytes}));
  std::vector<uint32_t> actual(bytes.size() / sizeof(uint32_t));
  std::memcpy(actual.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*readback));
  const auto rootBytes = Require(
      device.MapRead(*hierarchyReadback, {.Bytes = hierarchyReadback->Description().Bytes}));
  std::vector<uint32_t> roots(rootBytes.size() / sizeof(uint32_t));
  std::memcpy(roots.data(), rootBytes.data(), rootBytes.size());
  Require(device.Unmap(*hierarchyReadback));
  for (size_t at = 0; at < inputs.Cases.size(); ++at) {
    CHECK(roots[at] <= 1 && roots[at] * 6 == actual[at * 3],
          "the actual Root80 kernel agrees with the product leaf sphere selector");
    actual[at * 3] = roots[at] * 6;
  }
  CHECK(std::ranges::all_of(std::span(roots).subspan(inputs.Cases.size()),
                            [](uint32_t word) { return word == kRejected; }),
        "the two-dimensional root grid preserves its output guard");
  CheckResults(inputs, actual, shift != std::array<float, 4>{});
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  std::println("ROOT_BOUNDS_DEVICE {}", actual->Limits().Name);
  NativeOfflineDevice device(*actual);
  auto inputs = Cases();
  Run(device, inputs);
  constexpr std::array<float, 4> shift{0x1p18f, -0x1p20f, 0x1p16f, 0};
  for (size_t at = 0; at < inputs.Placements.size(); ++at) {
    auto &placement = inputs.Placements[at];
    auto &witness = inputs.Cases[at];
    for (size_t axis = 0; axis < 3; ++axis) { placement.Current[12 + axis] -= shift[axis]; }
    for (size_t leaf = 0; leaf < witness.Visible.size(); ++leaf) {
      witness.Visible[leaf] =
          !witness.UnknownModel && Inside(placement, witness.Points[leaf], shift);
    }
  }
  Run(device, inputs, shift);
  return Report();
}
