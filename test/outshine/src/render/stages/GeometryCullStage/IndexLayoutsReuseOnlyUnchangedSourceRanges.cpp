#include "Check.h"
#include "GeometryHierarchy.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kGuard = 0xdeadbeefu;

struct alignas(16) CompactView {
  std::array<uint32_t, 28> Before{};
  uint32_t Jobs = 4;
  std::array<uint32_t, 35> After{};
};

static_assert(sizeof(CompactView) == 256 && offsetof(CompactView, Jobs) == 112);

template <class T, size_t N> std::vector<uint32_t> Words(const std::array<T, N> &values) {
  std::vector<uint32_t> result(sizeof(values) / sizeof(uint32_t));
  std::memcpy(result.data(), values.data(), sizeof(values));
  return result;
}

template <class T> Binding UniformAt(uint32_t slot, const T &value) {
  return {.Set = 2, .Slot = slot, .Value = Uniform{std::as_bytes(std::span(&value, 1))}};
}

std::vector<uint32_t> Read(Device &device, const Buffer &buffer) {
  auto read =
      Require(ReadbackTransfer::Start(device, buffer, {.Bytes = buffer.Description().Bytes}));
  const auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "index-layout readback completes");
  if (!bytes) { Failed(); }
  std::vector<uint32_t> result(bytes->size() / sizeof(uint32_t));
  std::memcpy(result.data(), bytes->data(), bytes->size());
  return result;
}

void Put(Device &device, Commands &commands, Buffer &into, std::span<const uint32_t> words) {
  const auto from = Upload(device, commands, words);
  Require(commands.Copy(*from, into, {.Bytes = words.size_bytes()}));
}

void Complete(Device &device, std::unique_ptr<Commands> commands) {
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the index-layout operation completes within its offline budget");
}

struct Fixture {
  std::unique_ptr<Buffer> Roots, Templates, Visible, Kept, Slots, Arguments, Source, Index;
  std::unique_ptr<Buffer> Queue, QueueState;
  std::unique_ptr<ComputePipeline> Scan, Compact;
  GeometryHierarchyView View{.RootCount = 4, .TemplateCount = 4, .GridWidth = 4, .LogicalCount = 4};
  CompactView Cull;
};

std::vector<uint32_t> ColdSlots() {
  std::vector<uint32_t> slots(4, 0xffffffffu);
  slots.resize(7, kGeometryIndexCold);
  slots.resize(23, kGuard);
  return slots;
}

Fixture Prepare(Device &device) {
  Fixture f;
  auto commands = Require(device.Begin());
  const std::array roots{
      GeometryRoot{.TemplateFirst = 0, .TemplateCount = 1, .LogicalFirst = 0, .BatchRow = 0},
      GeometryRoot{.TemplateFirst = 1, .TemplateCount = 1, .LogicalFirst = 1, .BatchRow = 0},
      GeometryRoot{.TemplateFirst = 2, .TemplateCount = 2, .LogicalFirst = 2, .BatchRow = 1},
      GeometryRoot{.TemplateFirst = 4, .TemplateCount = 0, .LogicalFirst = 4, .BatchRow = 2}};
  std::array<GeometryClusterTemplate, 4> templates{};
  for (uint32_t at = 0; at < templates.size(); ++at) {
    templates[at] = {.RelativeFirstIndex = at * 3, .IndexCount = 3};
  }
  f.Roots = Upload(device, *commands, Words(roots));
  f.Templates = Upload(device, *commands, Words(templates));
  f.Visible = Upload(device, *commands, std::vector<uint32_t>(4));
  f.Kept = Upload(device, *commands, std::vector<uint32_t>(4));
  f.Slots = Upload(device, *commands, ColdSlots());
  const std::array<uint32_t, 15> arguments{0, 1, 0, 0, 7, 0, 1, 6, 0, 8, 0, 1, 12, 0, 9};
  f.Arguments = Upload(device, *commands, arguments);
  const std::array<uint32_t, 12> source{100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111};
  f.Source = Upload(device, *commands, source);
  f.Index = Upload(device, *commands, std::vector<uint32_t>(28, kGuard));
  f.Queue = Upload(device, *commands, std::vector<uint32_t>(8));
  f.QueueState = Upload(device, *commands, Words(std::array{GeometryQueueState{.Overflow = 1}}));
  Complete(device, std::move(commands));
  f.Scan = Require(device.CreateComputePipeline(Shader("geometryRootScan.comp.spv")));
  f.Compact = Require(device.CreateComputePipeline(Shader("geometrySelectedCompact.comp.spv")));
  return f;
}

void Step(Device &device, Fixture &f, std::array<uint32_t, 4> counts) {
  auto commands = Require(device.Begin());
  const std::array<uint32_t, 4> visible{uint32_t{counts[0] != 0}, uint32_t{counts[1] != 0}, 1, 1};
  Put(device, *commands, *f.Visible, visible);
  Put(device, *commands, *f.Kept, counts);
  const std::array scan{Storage(0, 0, *f.Roots),
                        Storage(0, 1, *f.Visible),
                        Storage(0, 2, *f.Kept),
                        Storage(1, 0, *f.Slots),
                        Storage(1, 1, *f.Arguments),
                        UniformAt(0, f.View)};
  Require(commands->Dispatch(*f.Scan, scan, {.X = 4}));
  const std::array compact{Storage(0, 0, *f.Roots),
                           Storage(0, 1, *f.Templates),
                           Storage(0, 2, *f.Queue),
                           Storage(0, 3, *f.QueueState),
                           Storage(0, 4, *f.Slots),
                           Storage(0, 5, *f.Arguments),
                           Storage(0, 6, *f.Source),
                           Storage(0, 7, *f.Visible),
                           Storage(1, 0, *f.Index),
                           UniformAt(0, f.Cull),
                           UniformAt(1, f.View)};
  Require(commands->Dispatch(*f.Compact, compact, {.X = 4}));
  Complete(device, std::move(commands));
}

void CheckState(Device &device, const Fixture &f, std::array<uint32_t, 3> states) {
  const auto slots = Read(device, *f.Slots);
  CHECK(std::equal(states.begin(), states.end(), slots.begin() + 4),
        "batch state distinguishes cold/changed and exact unchanged layouts");
  CHECK(std::ranges::all_of(std::span(slots).subspan(7), [](uint32_t n) { return n == kGuard; }),
        "layout metadata cannot overwrite its guarded suffix");
}

void CheckSequence(Device &device, const Fixture &f, uint32_t first, uint32_t other) {
  const auto indices = Read(device, *f.Index);
  const auto args = Read(device, *f.Arguments);
  CHECK(indices[0] == first && indices[1] == first + 1 && indices[2] == first + 2 &&
            indices[6] == other && indices[7] == other + 1 && indices[8] == other + 2,
        "independent source IDs retain their exact current compacted order");
  CHECK(args == std::vector<uint32_t>({3, 1, 0, 0, 7, 3, 1, 6, 0, 8, 0, 1, 12, 0, 9}),
        "only index counts change; all other indirect words retain their values");
  CHECK(std::ranges::all_of(std::span(indices).subspan(12), [](uint32_t n) { return n == kGuard; }),
        "index copying stays inside the admitted destination ranges");
}

void Exercise(Device &device) {
  auto f = Prepare(device);
  Step(device, f, {3, 0, 3, 0});
  CheckState(device, f, {1, 1, 1});
  CheckSequence(device, f, 100, 106);
  Step(device, f, {3, 0, 3, 0});
  CheckState(device, f, {0, 0, 0});
  CheckSequence(device, f, 100, 106);
  Step(device, f, {0, 3, 3, 0});
  CheckState(device, f, {1, 0, 0});
  CheckSequence(device, f, 103, 106);
  Step(device, f, {3, 0, 3, 0});
  CheckState(device, f, {1, 0, 0});
  CheckSequence(device, f, 100, 106);
  Step(device, f, {3, 0, 0, 3});
  CheckState(device, f, {0, 1, 0});
  CheckSequence(device, f, 100, 109);
  Step(device, f, {0, 0, 0, 0});
  CheckState(device, f, {1, 1, 0});
  const auto empty = Read(device, *f.Arguments);
  CHECK(empty[0] == 0 && empty[5] == 0, "an empty selection exposes no stale draw counts");
  Step(device, f, {3, 0, 3, 0});
  CheckSequence(device, f, 100, 106);
  auto commands = Require(device.Begin());
  Put(device, *commands, *f.Slots, ColdSlots());
  const std::array<uint32_t, 12> changed{
      200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211};
  Put(device, *commands, *f.Source, changed);
  Put(device, *commands, *f.Index, std::vector<uint32_t>(28, kGuard));
  Complete(device, std::move(commands));
  Step(device, f, {3, 0, 3, 0});
  CheckState(device, f, {1, 1, 1});
  CheckSequence(device, f, 200, 206);
  Step(device, f, {3, 0, 3, 0});
  CheckState(device, f, {0, 0, 0});
  CheckSequence(device, f, 200, 206);
  commands = Require(device.Begin());
  const auto cold = Upload(device, *commands, std::vector<uint32_t>(3, kGeometryIndexCold));
  Require(commands->Copy(
      *cold, *f.Slots, {.DestinationOffset = 4 * sizeof(uint32_t), .Bytes = 3 * sizeof(uint32_t)}));
  Put(device, *commands, *f.Index, std::vector<uint32_t>(28, kGuard));
  Complete(device, std::move(commands));
  Step(device, f, {3, 0, 3, 0});
  CheckState(device, f, {1, 1, 1});
  CheckSequence(device, f, 200, 206);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Exercise(*device);
  return Report();
}
