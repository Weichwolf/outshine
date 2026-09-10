#include <array>
#include <numbers>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scenario/Scenario.h>
#include "Surfacing.h"
#include "Subject.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const std::array<uint8_t, 16> pixels{
      255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255};
  const int image = geometry.addImage(2, 2, pixels);
  CHECK(image >= 0, "native RGBA image is accepted");
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Unlit = true;
  material.BaseColourMap.Image = image;
  material.BaseColourMap.Sampler.Magnify = Filter::Nearest;
  material.BaseColourMap.Sampler.Minify = Filter::Nearest;
  material.BaseColourMap.Sampler.Mip = MipFilter::None;
  material.BaseColourMap.Sampler.WrapU = Wrap::ClampToEdge;
  material.BaseColourMap.Sampler.WrapV = Wrap::ClampToEdge;
  const int part = geometry.addPart("quad", geometry.addSurface("colour", material).value());
  CHECK(geometry.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "positions");
  CHECK(geometry.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "normals");
  CHECK(geometry.setTexture(part, std::array<float, 8>{0, 1, 1, 1, 1, 0, 0, 0}), "UVs");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "triangles");
  std::array<Render::SubjectMaterial, 1> slots;
  slots[0].Row = material;
  slots[0].Row.NormalMap = material.BaseColourMap;
  slots[0].Row.NormalMap.Set = UvSet::Uv1;
  slots[0].Row.NormalMap.Uv.OffsetUv = {{0.25, 0.5}};
  slots[0].Row.MetalRoughMap = material.BaseColourMap;
  slots[0].Row.EmissiveMap = material.BaseColourMap;
  slots[0].Row.SpecularStrengthMap = material.BaseColourMap;
  slots[0].Row.SpecularTintMap = material.BaseColourMap;
  std::string error;
  CHECK(Render::ResolveNativeTextures(geometry, slots, error),
        "all supported native sockets resolve");
  CHECK(slots[0].Normal.Set == UvSet::Uv1 && slots[0].Normal.Uv.M[2] == 0.25 &&
            slots[0].Normal.Uv.M[5] == 0.5,
        "socket-specific UV set and translation survive");
  for (const auto *texture : {&slots[0].Colour,
                              &slots[0].Normal,
                              &slots[0].MetalRough,
                              &slots[0].Emissive,
                              &slots[0].SpecularStrength,
                              &slots[0].SpecularTint}) {
    CHECK(texture->Rgba == geometry.imageAt(image).Rgba.data() && texture->Width == 2 &&
              texture->Height == 2,
          "each socket borrows the declared image bytes");
    CHECK(texture->Magnify == Render::SubjectFilter::Nearest &&
              texture->Mip == Render::SubjectMip::None &&
              texture->WrapU == Render::SubjectWrap::ClampToEdge,
          "native sampler reaches the socket");
  }
  slots[0].Row.BaseColourMap.Image = image + 1;
  CHECK(!Render::ResolveNativeTextures(geometry, slots, error),
        "absent image references are refused");
  Gltf::Subject assembled;
  CHECK(assembled.Assemble(geometry), "native geometry assembles");
  auto converted = assembled.Handed();
  CHECK(converted.has_value(), "native conversion succeeds");
  if (!converted) { return Report(); }
  Geometry roundtrip = std::move(*converted);
  CHECK(roundtrip.images() == 1 && roundtrip.imageAt(0).Rgba.size() == pixels.size() &&
            std::equal(pixels.begin(), pixels.end(), roundtrip.imageAt(0).Rgba.begin()),
        "assembly roundtrip owns and preserves native image bytes");
  const std::array<uint8_t, 4> secondPixel{17, 31, 63, 255};
  Material secondMaterial = material;
  secondMaterial.BaseColourMap.Image = roundtrip.addImage(1, 1, secondPixel);
  CHECK(roundtrip.setSurface(roundtrip.materialOf(0), secondMaterial).has_value(),
        "second material names its own image");
  Gltf::Subject appended;
  CHECK(appended.Assemble(roundtrip) && assembled.Append(appended),
        "native textured subjects append");
  auto appendedGeometry = assembled.Handed();
  CHECK(appendedGeometry.has_value(), "appended conversion succeeds");
  if (!appendedGeometry) { return Report(); }
  Geometry joined = std::move(*appendedGeometry);
  CHECK(joined.images() == 3 && joined.surfaces() == 2 &&
            joined.surfaceAt(joined.materialOf(0)).BaseColourMap.Image == 0 &&
            joined.surfaceAt(joined.materialOf(1)).BaseColourMap.Image == 2 &&
            std::equal(secondPixel.begin(), secondPixel.end(), joined.imageAt(2).Rgba.begin()),
        "append rebases image and material references together");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Engine engine;
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {320, 320};
  scenario.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "native-map";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{0, 0, 2}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scenario.Views.push_back(view);
  std::vector<float> frame;
  if (!engine.drawsInto({320, 320}) || !engine.declare(scenario) || !engine.setGeometry(geometry)) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  Geometry rotated = geometry.clone();
  geometry.clear();
  if (!engine.assemble() || !engine.advance() || !engine.renderer().render({}) ||
      !engine.renderer().readPixels(Buffer::Linear, frame)) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  CHECK(frame.size() == 320u * 320u * 4u, "linear frame has the declared dimensions");
  if (frame.size() != 320u * 320u * 4u) { return Report(); }
  const std::array<std::array<int, 2>, 4> sample{{{120, 120}, {200, 120}, {120, 200}, {200, 200}}};
  const std::array<std::array<int, 3>, 4> expected{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}}};
  for (size_t at = 0; at < sample.size(); ++at) {
    const size_t offset = static_cast<size_t>(sample[at][1] * 320 + sample[at][0]) * 4;
    for (size_t channel = 0; channel < 3; ++channel) {
      CHECK(expected[at][channel] ? frame[offset + channel] > 0.1f
                                  : frame[offset + channel] < 0.001f,
            "native image colour and top-left UV convention reach the GPU");
    }
  }
  std::filesystem::create_directories("build/native-materials");
  CHECK(engine.renderer().saveScreenshot("build/native-materials/colour.png").has_value(),
        "native map PNG is written");
  material.BaseColourMap.Uv = {.OffsetUv = {{1, 0}}, .RotationRad = std::numbers::pi / 2};
  CHECK(rotated.setSurface(rotated.materialOf(0), material).has_value(),
        "declare a quarter-turn UV mapping");
  if (!engine.setGeometry(rotated) || !engine.advance() || !engine.renderer().render({}) ||
      !engine.renderer().readPixels(Buffer::Linear, frame)) {
    Unprepared(engine.error().c_str());
    return Report();
  }
  // Mapping (u,v) -> (1-v,u): top-left samples green; then yellow, red, blue.
  constexpr std::array<size_t, 4> rotatedCorners{1, 3, 0, 2};
  for (size_t at = 0; at < sample.size(); ++at) {
    const size_t offset = static_cast<size_t>(sample[at][1] * 320 + sample[at][0]) * 4;
    for (size_t channel = 0; channel < 3; ++channel) {
      CHECK(expected[rotatedCorners[at]][channel] ? frame[offset + channel] > 0.1f
                                                  : frame[offset + channel] < 0.001f,
            "positive UV rotation samples the independently derived four image quadrants");
    }
  }
  CHECK(engine.renderer().saveScreenshot("build/native-materials/rotated.png").has_value(),
        "rotated native map PNG is written");
  return Report();
}
