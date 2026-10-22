#include "FieldRenderer.h"
#include "FieldPipelines.h"
#include "ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <print>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using outshine::Test::Gpu::Require;
using Vector = std::array<float, 3>;
constexpr uint32_t kSide = 96;
constexpr auto kWait = std::chrono::seconds(10);
constexpr FieldMaskGpu kParameters{
    .OriginPitch = {10, 3, -20, 10}, .Shape = {3, 2, 2, 0}, .Coverage = {.75f, 0, 0, 0}};
constexpr std::array kHeights{2.f, 0.f, 7.f, 2.f, 5.f, 7.f};
constexpr std::array<Vector, 6> kForward{
    {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}}};

struct View {
  Vector Right{}, Up{}, Forward{};
  float Left = 0, Bottom = 0, Near = 0, Width = 0, Height = 0, Range = 0;
  std::array<float, 20> Uniform{};
};

View MakeView(Vector forward) {
  View view{.Forward = forward};
  view.Up = std::abs(forward[1]) > .5f ? Vector{0, 0, forward[1]} : Vector{0, 1, 0};
  const auto &up = view.Up;
  view.Right = {forward[1] * up[2] - forward[2] * up[1],
                forward[2] * up[0] - forward[0] * up[2],
                forward[0] * up[1] - forward[1] * up[0]};
  const Vector least{10, 3, -20}, most{40, 10, 0};
  const auto interval = [&](Vector axis) {
    float low = 0, high = 0;
    for (size_t i = 0; i < 3; ++i) {
      low += std::min(least[i] * axis[i], most[i] * axis[i]);
      high += std::max(least[i] * axis[i], most[i] * axis[i]);
    }
    return std::array{low, high};
  };
  const auto horizontal = interval(view.Right), vertical = interval(view.Up),
             depth = interval(forward);
  view.Left = horizontal[0];
  view.Bottom = vertical[0];
  view.Near = depth[0] - 1;
  view.Width = horizontal[1] - horizontal[0];
  view.Height = vertical[1] - vertical[0];
  view.Range = depth[1] - depth[0] + 2;
  for (size_t axis = 0; axis < 3; ++axis) {
    view.Uniform[axis * 4] = 2 * view.Right[axis] / view.Width;
    view.Uniform[axis * 4 + 1] = 2 * view.Up[axis] / view.Height;
    view.Uniform[axis * 4 + 2] = -forward[axis] / view.Range;
  }
  view.Uniform[12] = -1 - 2 * view.Left / view.Width;
  view.Uniform[13] = -1 - 2 * view.Bottom / view.Height;
  view.Uniform[14] = (depth[1] + 1) / view.Range;
  view.Uniform[15] = 1;
  return view;
}

float ExpectedDepth(const View &view, uint32_t x, uint32_t y, bool backFaces) {
  Vector origin{};
  for (size_t axis = 0; axis < 3; ++axis) {
    origin[axis] = view.Right[axis] * (view.Left + (x + .5f) * view.Width / kSide) +
                   view.Up[axis] * (view.Bottom + (kSide - y - .5f) * view.Height / kSide) +
                   view.Forward[axis] * view.Near;
  }
  float nearest = std::numeric_limits<float>::infinity();
  for (size_t cell = 0; cell < kHeights.size(); ++cell) {
    if (kHeights[cell] <= 0) { continue; }
    const float x0 = 10 + static_cast<float>(cell % 3) * 10 + 1.25f;
    const float z0 = -20 + static_cast<float>(cell / 3) * 10 + 1.25f;
    const Vector low{x0, 3, z0}, high{x0 + 7.5f, 3 + kHeights[cell], z0 + 7.5f};
    float entry = 0, exit = view.Range;
    for (size_t axis = 0; axis < 3; ++axis) {
      if (view.Forward[axis] == 0) {
        if (origin[axis] < low[axis] || origin[axis] > high[axis]) { exit = -1; }
      } else {
        const float a = (low[axis] - origin[axis]) / view.Forward[axis];
        const float b = (high[axis] - origin[axis]) / view.Forward[axis];
        entry = std::max(entry, std::min(a, b));
        exit = std::min(exit, std::max(a, b));
      }
    }
    if (entry <= exit) { nearest = std::min(nearest, backFaces ? exit : entry); }
  }
  return std::isfinite(nearest) ? 1 - nearest / view.Range : 0;
}

void Verify(Device &device,
            const FieldDraw &field,
            const GraphicsPipeline &pipeline,
            Texture &depth,
            const View &view,
            bool backFaces) {
  auto commands = Require(device.Begin());
  Require(commands->BeginRenderPass(
      {.Colours = {}, .Depth = DepthAttachment{.Image = {.Image = &depth}, .Begin = Load::Clear}}));
  const auto resources = field.Bindings();
  const std::array bindings{
      resources[0],
      resources[1],
      resources[2],
      Binding{
          .Set = 1, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(view.Uniform))}}};
  Require(field.Encode(*commands,
                       pipeline,
                       bindings,
                       {.Width = kSide, .Height = kSide},
                       {.Width = kSide, .Height = kSide}));
  Require(commands->EndRenderPass());
  const auto submitted = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(submitted, kWait)) == CompletionStatus::Ready,
        "the real field caster completes before its depth is inspected");
  auto transfer =
      Require(ReadbackTransfer::Start(device, depth, {.Width = kSide, .Height = kSide}));
  const auto bytes = Require(transfer.Wait(device, kWait));
  CHECK(bytes, "the complete D32 attachment is available");
  if (!bytes) { return; }
  size_t wrong = 0, covered = 0;
  float maximumError = 0;
  for (uint32_t y = 0; y < kSide; ++y) {
    for (uint32_t x = 0; x < kSide; ++x) {
      float actual = 0;
      std::memcpy(
          &actual, bytes->data() + y * transfer.RowBytes() + x * sizeof(float), sizeof(float));
      const float expected = ExpectedDepth(view, x, y, backFaces);
      const float error = std::abs(actual - expected);
      wrong += !std::isfinite(actual) || error > 2e-6f;
      covered += expected > 0;
      maximumError = std::max(maximumError, error);
    }
  }
  std::println("FIELD_DEPTH back_faces={} forward={},{},{} covered={} wrong={} maximum_error={}",
               backFaces,
               view.Forward[0],
               view.Forward[1],
               view.Forward[2],
               covered,
               wrong,
               maximumError);
  CHECK(covered > 0 && wrong == 0,
        "all pixels match independent ray/box intersections, including empty lots and repeated "
        "roofs");
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  FieldRenderer fields;
  GpuPlacement placement;
  placement.Current = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  placement.Previous = placement.Current;
  const auto id = Require(fields.Place(*device, nullptr, kParameters, kHeights, 0, placement));
  auto depth =
      Require(device->CreateTexture({.Width = kSide,
                                     .Height = kSide,
                                     .Texels = Format::Depth32Float,
                                     .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  for (const bool backFaces : {false, true}) {
    auto pipeline = Require(ConfigureFieldDepth(*device, backFaces ? Cull::Front : Cull::Back));
    for (const auto forward : kForward) {
      Verify(*device, fields.Drawn().front(), *pipeline, *depth, MakeView(forward), backFaces);
    }
  }
  CHECK(fields.Release(id) && fields.Drawn().empty() && !fields.Release(id),
        "a field has explicit ownership and cannot be released twice");
  return Report();
}
