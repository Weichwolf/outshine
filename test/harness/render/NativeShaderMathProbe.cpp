#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct alignas(16) Inputs {
  // (1+2^-13)(1-2^-13)-1 is 0 with separate rounding, -2^-26 when fused.
  std::array<float, 4> Arithmetic{1.0001220703125F, 0.9998779296875F, -1.0F, 33554432.0F};
  std::array<uint32_t, 4> Special{};
};

static_assert(sizeof(Inputs) == 32 && alignof(Inputs) == 16 && offsetof(Inputs, Special) == 16);

void CheckValues(Device &device,
                 const GraphicsPipeline &graphics,
                 const ComputePipeline &compute,
                 const Inputs &inputs) {
  constexpr uint64_t rowBytes = 256;
  constexpr uint64_t bytes = rowBytes * 2 + sizeof(std::array<float, 4>);
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  std::array<std::unique_ptr<Texture>, 2> images;
  std::array<ColourAttachment, 2> colours;
  for (size_t i = 0; i < images.size(); ++i) {
    images[i] = Require(
        device.CreateTexture({.Width = 1,
                              .Height = 1,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    colours[i] = {.Image = {.Image = images[i].get()}, .Begin = Load::Clear};
  }
  auto commands = Require(device.Begin());
  auto output = Require(device.CreateBuffer(
      {.Bytes = sizeof(std::array<float, 4>), .Use = BufferUse::Storage | BufferUse::CopySource}));
  const Binding uniform{
      .Set = 0, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&inputs, 1))}};
  const std::array computeBindings{uniform, Storage(1, 0, *output)};
  Require(commands->Dispatch(compute, computeBindings, {.X = 1}));
  Require(
      commands->Copy(*output,
                     *readback,
                     {.DestinationOffset = rowBytes * 2, .Bytes = output->Description().Bytes}));
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  const std::array graphicsBindings{uniform};
  Require(commands->Draw({.State = {.Pipeline = &graphics,
                                    .Bindings = graphicsBindings,
                                    .Vertices = {},
                                    .Area = {.Width = 1, .Height = 1},
                                    .Clip = {.Width = 1, .Height = 1}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  for (size_t i = 0; i < images.size(); ++i) {
    Require(commands->Copy(
        *images[i],
        *readback,
        {.BufferOffset = i * rowBytes, .RowBytes = rowBytes, .Region = {.Width = 1, .Height = 1}}));
  }
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "shader math finishes within the bounded diagnostic wait");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = bytes}));
  for (size_t i = 0; i < 3; ++i) {
    std::array<float, 4> actual{};
    std::memcpy(actual.data(), mapped.data() + i * rowBytes, sizeof(actual));
    CHECK(actual[0] == 0.0F, "precise multiplication/addition cannot contract in any shader stage");
    CHECK(actual[1] == (i == 1 ? 1.0F : 0.0F),
          "vertex and compute preserve cancellation; fragment shading preserves finite division");
    CHECK(actual[2] == float(std::isnan(std::bit_cast<float>(inputs.Special[0]))),
          "every stage preserves NaN classification from runtime bits");
    CHECK(actual[3] == float(std::isinf(std::bit_cast<float>(inputs.Special[1]))),
          "every stage preserves signed infinity classification from runtime bits");
  }
  Require(device.Unmap(*readback));
}

void CheckMath(Device &device,
               const char *vertexPath,
               const char *fragmentPath,
               const char *computePath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}},
                           ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  const auto graphics =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                             .Fragment = ShaderCode{.Spirv = fragment},
                                             .Streams = {},
                                             .Attributes = {},
                                             .Colours = targets,
                                             .Depth = std::nullopt,
                                             .Culling = Cull::None,
                                             .Label = {}}));
  const auto compute = Require(device.CreateComputePipeline(Spirv(computePath)));
  for (const auto special : {std::array{0x3f800000U, 0x40000000U},
                             std::array{0x7fc00000U, 0x7f800000U},
                             std::array{0xffc00000U, 0xff800000U},
                             std::array{0x7f800000U, 0x7fc00000U},
                             std::array{0xff800000U, 0x00000000U}}) {
    Inputs inputs;
    inputs.Special[0] = special[0];
    inputs.Special[1] = special[1];
    CheckValues(device, *graphics, *compute, inputs);
  }
}
}

int main(int argc, char *const *argv) {
  if ((argc != 4 && argc != 5) || (argc == 5 && std::string_view(argv[4]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 5}));
  CheckMath(*device, argv[1], argv[2], argv[3]);
  return Report();
}
