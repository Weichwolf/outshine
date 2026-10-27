#include "test/harness/render/SrgbColour.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include "ImageCapture.h"
#include <array>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <vector>
#include <SDL3/SDL.h>
#include <Engine.h>
#include <Scenario.h>
#include "Check.h"

namespace {
double LinearFromSrgb(double encoded) {
  return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

void CheckColourCodes() {
  using namespace outshine::Test;
  for (unsigned code = 0; code < 256; ++code) {
    const auto original = static_cast<uint8_t>(code);
    const double encoded = code / 255.0;
    const double linear = LinearFromSrgb(encoded);
    CHECK(EncodeSrgb8(linear) == original, "every original colour byte roundtrips exactly");
    for (unsigned bit = 0; bit < 8; ++bit) {
      const double changed = (code ^ (1u << bit)) / 255.0;
      CHECK(EncodeSrgb8(LinearFromSrgb(changed)) != original,
            "every changed source bit remains detectable");
    }
    if (code != 0 && code != 255) {
      CHECK(EncodeSrgb8(encoded) != original, "missing sRGB decoding changes the source code");
      CHECK(EncodeSrgb8(LinearFromSrgb(linear)) != original,
            "double sRGB decoding changes the source code");
    }
  }
  for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity(),
                               -0.01,
                               1.01}) {
    CHECK(!EncodeSrgb8(invalid), "invalid linear colour cannot match a source code");
  }
  CHECK(EncodeSrgb8(0.5) == uint8_t{188}, "half linear intensity encodes as sRGB byte188");
  CHECK(EncodeSrgb8(0.18) == uint8_t{118}, "middle gray encodes as sRGB byte118");
  CHECK(EncodeSrgb8(LinearFromSrgb(128.0 / 255)) != uint8_t{188},
        "averaging encoded checker texels cannot stand in for a linear mip");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto accepted = [](const Result &result) {
    if (!result) { Unprepared(result.error().c_str()); }
    return result.has_value();
  };
  CheckColourCodes();
  Geometry geometry;
  std::vector<uint8_t> pixels(size_t{512} * 512 * 4, 255);
  for (size_t y = 0; y < 512; ++y) {
    for (size_t x = 0; x < 512; ++x) { pixels[(y * 512 + x) * 4] = (x + y) % 2 != 0 ? 255 : 0; }
  }
  const int image = *geometry.addImage(512, 512, pixels);
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Unlit = true;
  material.BaseColourMap.Image = image;
  material.BaseColourMap.Sampler.Magnify = Filter::Nearest;
  material.BaseColourMap.Sampler.Minify = Filter::Nearest;
  material.BaseColourMap.Sampler.Mip = MipFilter::Linear;
  material.BaseColourMap.Sampler.WrapU = Wrap::ClampToEdge;
  material.BaseColourMap.Sampler.WrapV = Wrap::ClampToEdge;
  const int part =
      geometry.addPart("quad", geometry.addSurface("colour", material).value()).value();
  CHECK(geometry.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "positions");
  CHECK(geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "normals");
  CHECK(geometry.setTexture(part, std::array<float, 8>{0, 1, 1, 1, 1, 0, 0, 0}), "UVs");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "triangles");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Engine engine;
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {.WidthPx = 320, .HeightPx = 320};
  scenario.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "native-map";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scenario.Views.push_back(view);
  std::vector<float> frame;
  if (!accepted(engine.setRenderTarget({.WidthPx = 320, .HeightPx = 320})) ||
      !accepted(engine.declare(scenario)) || !accepted(engine.setGeometry(geometry))) {
    return Report();
  }
  geometry.clear();
  if (!accepted(engine.assemble()) || !accepted(engine.advance()) ||
      !accepted(engine.renderer().render({})) ||
      !accepted(engine.renderer().readPixels(Buffer::Linear, frame))) {
    return Report();
  }
  CHECK(frame.size() == size_t{320} * 320 * 4, "linear frame has the declared dimensions");
  if (frame.size() != size_t{320} * 320 * 4) { return Report(); }
  constexpr float kLinearHalf = 0.5f;
  constexpr float kSrgbCodes = 255.0f;
  const auto encodedFromLinear = [](float value) {
    return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
  };
  const auto expectedRedCode =
      static_cast<uint8_t>(std::lround(encodedFromLinear(kLinearHalf) * kSrgbCodes));
  size_t mismatchedPixels = 0;
  for (size_t y = 90; y < 230; ++y) {
    for (size_t x = 90; x < 230; ++x) {
      const size_t at = (y * 320 + x) * 4;
      if (frame[at + 1] <= 0.1f) {
        ++mismatchedPixels;
        continue;
      }
      mismatchedPixels +=
          static_cast<size_t>(EncodeSrgb8(frame[at] / frame[at + 1]) != expectedRedCode);
    }
  }
  CHECK(mismatchedPixels == 0,
        "minified checker preserves the stored sRGB half-intensity code against constant green");
  std::vector<float> repeat;
  CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeat),
        "repeat renders");
  CHECK(frame == repeat, "resident minified image repeats exactly");
  std::filesystem::create_directories("build/native-materials");
  CHECK(outshine::App::SaveImage(engine, "build/native-materials/minified.png").has_value(),
        "native map PNG is written");
  return Report();
}
