#include "MaterialFieldPass.h"
#include "MaterialFieldRasterInput.h"
#include "ImpostorAtlas.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct Tracking final : NativeDeviceProxy {
  explicit Tracking(Device &device) : NativeDeviceProxy(device) {}

  size_t Submits = 0, MeshBuffers = 0;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    if ((static_cast<uint32_t>(description.Use) &
         static_cast<uint32_t>(BufferUse::Vertex | BufferUse::Index)) != 0) {
      ++MeshBuffers;
    }
    return Backing().CreateBuffer(description, label);
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submits;
    return Backing().Submit(std::move(commands));
  }
};

ImpostorAtlas Source(ScatteringModel scattering) {
  ImpostorAtlas::View view;
  view.TowardEye = {{0, 0, 1}};
  view.Texels.resize(16);
  view.Materials.resize(16);
  for (const size_t at : {size_t{0}, size_t{15}}) {
    view.Texels[at] = {.Normal = {{.6f, 0, .8f}}, .Depth = at == 0 ? .375f : .625f, .Surface = 1};
    view.Materials[at] = {.BaseColour = {{.12345679f, .23456789f, .3456789f}},
                          .Roughness = .456789f,
                          .Metalness =
                              scattering == ScatteringModel::MetallicRoughness ? .567891f : 0};
  }
  std::string error;
  Material material;
  material.Scattering = scattering;
  if (scattering != ScatteringModel::MetallicRoughness) {
    material.DiffuseTransmission = {{.1f, .2f, .3f}};
  }
  auto atlas = ImpostorAtlas::Create(4, {}, 2, {material}, {std::move(view)}, error);
  CHECK(atlas, "independent floating point samples form a valid atlas");
  if (!atlas) { Failed(); }
  return std::move(*atlas);
}

Geometry Original(ScatteringModel scattering) {
  Geometry source;
  Material material;
  material.BaseColour = {{.12345679f, .23456789f, .3456789f, 1}};
  material.Roughness = .456789f;
  material.Metalness = scattering == ScatteringModel::MetallicRoughness ? .567891f : 0;
  material.Scattering = scattering;
  if (scattering != ScatteringModel::MetallicRoughness) {
    material.DiffuseTransmission = {{.1f, .2f, .3f}};
  }
  const auto row = source.addSurface("original", material);
  CHECK(row, "independent original material is valid");
  if (!row) { Failed(); }
  for (const size_t at : {size_t{0}, size_t{1}}) {
    const auto part = source.addPart("original", *row);
    CHECK(part, "independent original part is valid");
    if (!part) { Failed(); }
    const float left = at == 0 ? -2 : 1;
    const float bottom = at == 0 ? 1 : -2;
    const float z = at == 0 ? -1 : 1;
    CHECK(
        source.setPositions(*part,
                            std::array<float, 12>{left,
                                                  bottom,
                                                  z,
                                                  left + 1,
                                                  bottom,
                                                  z,
                                                  left + 1,
                                                  bottom + 1,
                                                  z,
                                                  left,
                                                  bottom + 1,
                                                  z}) &&
            source.setNormals(
                *part, std::array<float, 12>{.6f, 0, .8f, .6f, 0, .8f, .6f, 0, .8f, .6f, 0, .8f}) &&
            source.setTriangles(*part, std::array<unsigned, 6>{0, 1, 2, 0, 2, 3}),
        "independent originals declare exact planes and shading normals");
  }
  return source;
}

void CheckSample(size_t pixel, const std::byte *bytes, bool colour) {
  if (colour) {
    uint16_t alpha = 0;
    std::memcpy(&alpha, bytes + 3 * sizeof(uint16_t), sizeof(alpha));
    const bool covered = pixel == 0 || pixel == 15;
    CHECK(alpha == (covered ? 0x3c00 : 0), "native coverage has the two known samples");
    return;
  }
  float depth = 0;
  std::memcpy(&depth, bytes, sizeof(depth));
  float expected = 0;
  if (pixel == 0) { expected = .375f; }
  if (pixel == 15) { expected = .625f; }
  CHECK(depth == expected, "sample depth retains its independently derived reverse Z");
}

void SamePixels(Device &device, const MaterialFieldImages &a, const MaterialFieldImages &b) {
  CHECK(a.Unfiltered->Texture->Description().Texels == Format::Rgba16Float &&
            a.Depth->Description().Texels == Format::Depth32Float,
        "comparison reads the declared native colour and depth formats");
  size_t component = 0;
  for (const auto &textures :
       {std::array{a.Unfiltered->Texture, b.Unfiltered->Texture}, std::array{a.Depth, b.Depth}}) {
    const auto first = ReadImage(device, *textures[0], {.Width = 4, .Height = 4});
    const auto second = ReadImage(device, *textures[1], {.Width = 4, .Height = 4});
    const size_t texelBytes = component == 0 ? 8 : 4;
    for (size_t row = 0; row < 4; ++row) {
      CHECK(std::memcmp(first.Rows.data() + row * first.RowBytes,
                        second.Rows.data() + row * second.RowBytes,
                        4 * texelBytes) == 0,
            "stored samples and independent originals have exact active colour and depth");
      for (size_t column = 0; column < 4; ++column) {
        const size_t pixel = row * 4 + column;
        const auto *bytes = first.Rows.data() + row * first.RowBytes + column * texelBytes;
        CheckSample(pixel, bytes, component == 0);
      }
    }
    ++component;
  }
}

void Exercise(ScatteringModel scattering) {
  auto native = Require(CreateDevice({.Validation = true}));
  Tracking device(*native);
  MaterialFieldImages held;
  {
    MaterialFieldPass pass(device);
    Require(pass.Configure());
    const auto atlas = Source(scattering);
    const auto samples = MaterialFieldRasterInput::Prepare(atlas, {.View = 0, .BytesMost = 1024});
    CHECK(samples, "capture frame is available without a GPU");
    if (!samples) { Failed(); }
    auto input = MaterialFieldInput::Prepare(atlas, 0, 1024);
    CHECK(input, "stored samples prepare without display simplification or byte repacking");
    if (!input) { Failed(); }
    const auto meshBuffers = device.MeshBuffers;
    const auto source =
        Require(RetryOffline([&] { return pass.Prepare(*input); }, std::chrono::seconds(5)));
    CHECK(device.MeshBuffers == meshBuffers,
          "stored raster upload creates no vertex or index buffers");
    const std::weak_ptr<const MaterialFieldInput> cpu = *input;
    input->reset();
    CHECK(cpu.expired(), "resident samples do not retain their decoded CPU input");
    const auto originalInput =
        MaterialFieldInput::Prepare(Original(scattering), {.WidthPx = 4, .HeightPx = 4});
    CHECK(originalInput, "independent original prepares through the existing geometry input");
    if (!originalInput) { Failed(); }
    const auto original = Require(
        RetryOffline([&] { return pass.Prepare(*originalInput); }, std::chrono::seconds(5)));
    std::array<SceneLight, 1> lights{};
    lights[0].Light.Intensity = 100;
    MaterialFieldView view;
    view.Camera = samples->Capture;
    view.Target = {.WidthPx = 4, .HeightPx = 4};
    view.Lights = lights;
    for (const Vec3f direction : {Vec3f{{0, 0, -1}}, Vec3f{{-.6f, 0, -.8f}}}) {
      lights[0].Light.Direction = direction;
      held = Require(
          RetryOffline([&] { return pass.Relight(*source, view); }, std::chrono::seconds(5)));
      const auto reference = Require(
          RetryOffline([&] { return pass.Relight(*original, view); }, std::chrono::seconds(5)));
      SamePixels(device, held, reference);
    }
    lights[0].PositionM = {{0, 0, 10}};
    lights[0].Light.Direction = {{0, 0, -1}};
    view.Environment.RadianceLinear = {{.1f, .2f, .3f}};
    for (const auto kind : {LightKind::Point, LightKind::Spot}) {
      lights[0].Light.Kind = kind;
      held = Require(
          RetryOffline([&] { return pass.Relight(*source, view); }, std::chrono::seconds(5)));
      const auto reference = Require(
          RetryOffline([&] { return pass.Relight(*original, view); }, std::chrono::seconds(5)));
      SamePixels(device, held, reference);
    }
    const auto submissions = device.Submits;
    for (const size_t change : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{4}}) {
      auto moved = view;
      if (change == 0) { moved.Camera.EyeM[0] += .001; }
      if (change == 1) { moved.Camera.XMagM *= 2; }
      if (change == 2) { moved.Camera.Kind = CameraKind::Perspective; }
      if (change == 3) { moved.Camera.ZNearM += .001; }
      if (change == 4) { moved.Camera.EyeM[2] += 1; }
      const auto rejected = pass.Relight(*source, moved);
      CHECK(
          !rejected && rejected.error().Kind == ErrorKind::Invalid && device.Submits == submissions,
          "different capture rays or clipping refuse before submission without publishing a field");
    }
    const std::atomic_bool stopped{true};
    const auto cancelled = pass.Relight(*source, view, &stopped);
    CHECK(!cancelled && cancelled.error().Kind == ErrorKind::Cancelled &&
              device.Submits == submissions,
          "cancellation keeps the previous valid stored sample field");
    view.Camera.EyeM[2] += 1;
    view.Camera.ZNearM += 1;
    view.Camera.ZFarM += 1;
    const auto shifted =
        Require(RetryOffline([&] { return pass.Relight(*source, view); }, std::chrono::seconds(5)));
    const auto reference = Require(
        RetryOffline([&] { return pass.Relight(*original, view); }, std::chrono::seconds(5)));
    SamePixels(device, shifted, reference);
    Require(pass.Collect());
  }
  const auto pixels = ReadImage(device, *held.Depth, {.Width = 4, .Height = 4});
  CHECK(!pixels.Rows.empty(), "held sample output survives source and pass retirement");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native sample resources");
  for (const auto scattering :
       {ScatteringModel::MetallicRoughness, ScatteringModel::DiffuseThinSheet}) {
    Exercise(scattering);
  }
  SDL_Quit();
  return Report();
}
