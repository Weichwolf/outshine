#include "MaterialFieldPass.h"
#include "MaterialFieldRasterInput.h"
#include "ImpostorAtlas.h"
#include "EnvironmentSpecularLayout.h"
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <print>
#include <memory>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

float ReadHalf(const std::byte *bytes) {
  uint16_t bits = 0;
  std::memcpy(&bits, bytes, sizeof(bits));
  const int exponent = (bits >> 10u) & 31u;
  const float mantissa = static_cast<float>(bits & 1023u);
  if (exponent == 31) { return std::numeric_limits<float>::quiet_NaN(); }
  const float value = exponent == 0 ? std::ldexp(mantissa, -24)
                                    : std::ldexp(1.0f + mantissa / 1024.0f, exponent - 15);
  return (bits & 0x8000u) != 0 ? -value : value;
}

bool MatchingRadiance(float first, float second) {
  constexpr float kHalfRelativeStep = 1.0f / 1024.0f;
  constexpr float kHalfSubnormalStep = 1.0f / 16777216.0f;
  const float tolerance =
      kHalfSubnormalStep + kHalfRelativeStep * std::max(std::abs(first), std::abs(second));
  return std::isfinite(first) && std::isfinite(second) && std::abs(first - second) <= tolerance;
}

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
    const float alpha = ReadHalf(bytes + 3 * sizeof(uint16_t));
    const bool covered = pixel == 0 || pixel == 15;
    CHECK(alpha == (covered ? 1.0f : 0.0f), "native coverage has the two known samples");
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
      for (size_t column = 0; column < 4; ++column) {
        const size_t pixel = row * 4 + column;
        const auto *bytes = first.Rows.data() + row * first.RowBytes + column * texelBytes;
        const auto *expected = second.Rows.data() + row * second.RowBytes + column * texelBytes;
        if (component == 0) {
          for (size_t channel = 0; channel < 4; ++channel) {
            const float actual = ReadHalf(bytes + channel * sizeof(uint16_t));
            const float reference = ReadHalf(expected + channel * sizeof(uint16_t));
            if (!MatchingRadiance(actual, reference)) {
              std::println(stderr,
                           "radiance mismatch pixel {} channel {}: {} vs {}",
                           pixel,
                           channel,
                           actual,
                           reference);
            }
            CHECK(MatchingRadiance(actual, reference),
                  "stored samples match independent original radiance within binary16 precision");
          }
        } else {
          float actualDepth = 0, expectedDepth = 0;
          std::memcpy(&actualDepth, bytes, sizeof(actualDepth));
          std::memcpy(&expectedDepth, expected, sizeof(expectedDepth));
          CHECK(std::isfinite(actualDepth) && std::isfinite(expectedDepth) &&
                    std::abs(actualDepth - expectedDepth) <= 1.e-6f,
                "stored samples retain the independent original depth");
        }
        CheckSample(pixel, bytes, component == 0);
      }
    }
    ++component;
  }
}

bool SameColour(Device &device, const MaterialFieldImages &a, const MaterialFieldImages &b) {
  const auto first = ReadImage(device, *a.Unfiltered->Texture, {.Width = 4, .Height = 4});
  const auto second = ReadImage(device, *b.Unfiltered->Texture, {.Width = 4, .Height = 4});
  for (size_t row = 0; row < 4; ++row) {
    for (size_t component = 0; component < 16; ++component) {
      const float firstValue =
          ReadHalf(first.Rows.data() + row * first.RowBytes + component * sizeof(uint16_t));
      const float secondValue =
          ReadHalf(second.Rows.data() + row * second.RowBytes + component * sizeof(uint16_t));
      if (!MatchingRadiance(firstValue, secondValue)) { return false; }
    }
  }
  return true;
}

bool FiniteColour(Device &device, const MaterialFieldImages &field) {
  const auto pixels = ReadImage(device, *field.Unfiltered->Texture, {.Width = 4, .Height = 4});
  for (size_t row = 0; row < 4; ++row) {
    for (size_t component = 0; component < 16; ++component) {
      const float value =
          ReadHalf(pixels.Rows.data() + row * pixels.RowBytes + component * sizeof(uint16_t));
      if (!std::isfinite(value)) { return false; }
    }
  }
  return true;
}

MaterialFieldImages RelightPair(Device &device,
                                MaterialFieldPass &pass,
                                const ResidentMaterialField &source,
                                const ResidentMaterialField &original,
                                const MaterialFieldView &view) {
  auto field =
      Require(RetryOffline([&] { return pass.Relight(source, view); }, std::chrono::seconds(5)));
  const auto reference =
      Require(RetryOffline([&] { return pass.Relight(original, view); }, std::chrono::seconds(5)));
  SamePixels(device, field, reference);
  CHECK(FiniteColour(device, field),
        "a lighting observer cannot poison the material field with nonfinite colour");
  CHECK(field.Camera.EyeM == view.Camera.EyeM &&
            field.LightingCamera.EyeM == view.LightingCamera.value_or(view.Camera).EyeM,
        "each output owns the independent raster and lighting camera bindings");
  return field;
}

void ObserverViews(Device &device,
                   MaterialFieldPass &pass,
                   const ResidentMaterialField &source,
                   const ResidentMaterialField &original,
                   MaterialFieldView view,
                   ScatteringModel scattering) {
  const auto relight = [&] { return RelightPair(device, pass, source, original, view); };
  const auto captured = relight();
  view.LightingCamera = view.Camera;
  view.LightingCamera->EyeM = {{17, -7, 13}};
  const auto parallel = relight();
  CHECK(SameColour(device, captured, parallel),
        "a parallel lighting observer depends on direction, not its eye position");
  view.LightingCamera->Forward = {{-1, 0, 0}};
  view.LightingCamera->Right = {{0, 0, -1}};
  const auto turned = relight();
  CHECK(
      SameColour(device, captured, turned) == (scattering == ScatteringModel::DiffuseThinSheet),
      "turning the lighting observer changes PBR reflection but not diffuse thin-sheet transport");
  view.LightingCamera->Kind = CameraKind::Perspective;
  view.LightingCamera->EyeM = {{2, 1, 8}};
  const auto perspective = relight();
  CHECK(SameColour(device, captured, perspective) ==
            (scattering == ScatteringModel::DiffuseThinSheet),
        "a perspective lighting observer uses sample-to-eye rays at fixed capture pixels");
  view.Camera.EyeM[2] += 1;
  view.Camera.ZNearM += 1;
  view.Camera.ZFarM += 1;
  const auto shifted = relight();
  SamePixels(device, perspective, shifted);
  view.LightingCamera->EyeM = {{-1.5, 1.5, -1}};
  const auto coincident = Require(pass.Relight(source, view));
  const auto coincidentOriginal = Require(pass.Relight(original, view));
  CHECK(FiniteColour(device, coincident) && FiniteColour(device, coincidentOriginal),
        "an observer on the surface has no defined view direction but keeps radiance finite");
  for (size_t change = 0; change < 4; ++change) {
    auto invalid = view;
    if (change == 0) { invalid.LightingCamera->EyeM[0] = std::numeric_limits<double>::infinity(); }
    if (change == 1) { invalid.LightingCamera->EyeM[0] = std::numeric_limits<double>::max(); }
    if (change == 2) { invalid.LightingCamera->Forward[0] = 2; }
    if (change == 3) { invalid.LightingCamera->Kind = static_cast<CameraKind>(255); }
    const auto rejected = pass.Relight(source, invalid);
    CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
          "invalid lighting observers are refused");
  }
}

std::shared_ptr<RadianceImage> EnvironmentAtlas(Device &device) {
  auto image = std::make_shared<RadianceImage>();
  image->Device = &device;
  image->Texture =
      Require(device.CreateTexture({.Width = kEnvironmentWidth,
                                    .Height = kEnvironmentHeight,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  image->Sampling = Require(device.CreateSampler({.Min = Native::Filter::Nearest,
                                                  .Mag = Native::Filter::Nearest,
                                                  .Mip = Native::Filter::Nearest}));
  std::vector<float> pixels(size_t{kEnvironmentWidth} * kEnvironmentHeight * 4);
  for (size_t y = 0; y < kEnvironmentHeight; ++y) {
    for (size_t x = 0; x < kEnvironmentWidth; ++x) {
      const size_t offset = (y * kEnvironmentWidth + x) * 4;
      pixels[offset] = .1f + static_cast<float>(x) / static_cast<float>(kEnvironmentWidth);
      pixels[offset + 1] = .1f + static_cast<float>(y) / static_cast<float>(kEnvironmentHeight);
      pixels[offset + 2] = pixels[offset + 3] = 1;
    }
  }
  auto staging = Require(device.CreateBuffer({.Bytes = pixels.size() * sizeof(float),
                                              .Use = BufferUse::CopySource,
                                              .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*staging, {.Bytes = staging->Description().Bytes}));
  std::memcpy(mapped.data(), pixels.data(), static_cast<size_t>(staging->Description().Bytes));
  Require(device.Unmap(*staging));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*staging,
                         *image->Texture,
                         {.RowBytes = size_t{kEnvironmentWidth} * 4 * sizeof(float),
                          .Region = {.Width = kEnvironmentWidth, .Height = kEnvironmentHeight}}));
  image->Produced = Require(device.Submit(std::move(commands)));
  return image;
}

void InvalidEnvironments(Device &device,
                         MaterialFieldPass &pass,
                         const ResidentMaterialField &source,
                         const ResidentMaterialField &original,
                         MaterialFieldView view) {
  const auto atlas = EnvironmentAtlas(device);
  const auto reject = [&](const MaterialFieldView &candidate) {
    for (const auto *input : {&source, &original}) {
      const auto result = pass.Relight(*input, candidate);
      CHECK(!result && result.error().Kind == ErrorKind::Invalid,
            "invalid environment bindings refuse both inputs");
    }
  };
  for (int which = 0; which < 6; ++which) {
    auto bad = std::make_shared<RadianceImage>(*atlas);
    switch (which) {
      case 0: bad->Device = nullptr; break;
      case 1: bad->Texture.reset(); break;
      case 2: bad->Sampling.reset(); break;
      case 3: bad->Encoding = RadianceEncoding::IntegralPairs; break;
      case 4: bad->Produced.Serial = 0; break;
      case 5: bad->Produced.Device = 0; break;
      default: CHECK(false, "unknown environment corruption case"); return;
    }
    view.Reflections.emplace().Atlas = std::move(bad);
    reject(view);
  }
  for (const auto shape :
       {TextureDescription{
            .Width = 1, .Height = 1, .Texels = Format::Rgba32Float, .Use = TextureUse::Sampled},
        TextureDescription{.Width = kEnvironmentWidth,
                           .Height = kEnvironmentHeight,
                           .Texels = Format::Rgba8Unorm,
                           .Use = TextureUse::Sampled},
        TextureDescription{.Width = kEnvironmentWidth,
                           .Height = kEnvironmentHeight,
                           .Texels = Format::Rgba32Float,
                           .Use = TextureUse::CopyDestination}}) {
    auto bad = std::make_shared<RadianceImage>(*atlas);
    bad->Texture = Require(device.CreateTexture(shape));
    view.Reflections.emplace().Atlas = std::move(bad);
    reject(view);
  }
  view.Reflections.emplace().Atlas = atlas;
  view.Reflections->Up = {{0, 0, 0}};
  reject(view);
  view.Reflections->Up = {{0, 1, 0}};
  view.Reflections->ToSun[0] = std::numeric_limits<float>::quiet_NaN();
  reject(view);
  view.Reflections->ToSun = {{0, 1, 0}};
  view.Environment.SkyLux = std::numeric_limits<double>::max();
  reject(view);
}

void EnvironmentViews(Device &device,
                      MaterialFieldPass &pass,
                      const ResidentMaterialField &source,
                      const ResidentMaterialField &original,
                      MaterialFieldView view) {
  view.Environment.SkyLux = 2;
  const auto baseline = RelightPair(device, pass, source, original, view);
  auto atlas = EnvironmentAtlas(device);
  view.Reflections.emplace().Atlas = atlas;
  view.Reflections->ToSun = {{0, 0, 1}};
  const auto mapped = RelightPair(device, pass, source, original, view);
  CHECK(!SameColour(device, baseline, mapped),
        "the actual environment atlas contributes native radiance");
  view.Reflections->ToSun = {{1, 0, 0}};
  const auto rotated = RelightPair(device, pass, source, original, view);
  CHECK(!SameColour(device, mapped, rotated),
        "the shared sky and sun basis controls environment lookup");
  view.Environment.SkyLux = 0;
  const auto dark = RelightPair(device, pass, source, original, view);
  CHECK(SameColour(device, baseline, dark),
        "zero sky weight preserves the authored environment exactly");
  view.Reflections.reset();
  const auto cleared = RelightPair(device, pass, source, original, view);
  CHECK(SameColour(device, baseline, cleared),
        "a later view does not inherit the preceding environment binding");
  view.Environment.SkyLux = 2;
  view.Reflections.emplace().Atlas = atlas;
  const std::weak_ptr<const RadianceImage> held = atlas;
  const auto pending = Require(pass.Relight(source, view));
  view.Reflections.reset();
  atlas.reset();
  CHECK(!held.expired(), "submitted field work holds the environment after caller release");
  CHECK(!ReadImage(device, *pending.Unfiltered->Texture, {.Width = 4, .Height = 4}).Rows.empty(),
        "the held environment produces a complete native result");
  Require(pass.Collect());
  CHECK(held.expired(),
        "settled field work releases its environment lease without pinning source state");
}

void Exercise(ScatteringModel scattering) {
  auto native = Require(CreateDevice({.Validation = true}));
  auto &device = *native;
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
    const auto source =
        Require(RetryOffline([&] { return pass.Prepare(*input); }, std::chrono::seconds(5)));
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
    ObserverViews(device, pass, *source, *original, view, scattering);
    EnvironmentViews(device, pass, *source, *original, view);
    InvalidEnvironments(device, pass, *source, *original, view);
    for (const size_t change : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{4}}) {
      auto moved = view;
      if (change == 0) { moved.Camera.EyeM[0] += .001; }
      if (change == 1) { moved.Camera.XMagM *= 2; }
      if (change == 2) { moved.Camera.Kind = CameraKind::Perspective; }
      if (change == 3) { moved.Camera.ZNearM += .001; }
      if (change == 4) { moved.Camera.EyeM[2] += 1; }
      const auto rejected = pass.Relight(*source, moved);
      CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
            "different capture rays or clipping cannot publish a field");
    }
    const std::atomic_bool stopped{true};
    const auto cancelled = pass.Relight(*source, view, &stopped);
    CHECK(!cancelled && cancelled.error().Kind == ErrorKind::Cancelled,
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
