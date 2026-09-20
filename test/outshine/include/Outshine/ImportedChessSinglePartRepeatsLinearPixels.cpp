#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <vector>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <import/GltfImporter.h>
#include <scenario/Scenario.h>
#include "Check.h"
#include "PreparedRoot.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto accepted = [](const Result &result) {
    if (!result) { Unprepared(result.error().c_str()); }
    return result.has_value();
  };
  const std::string path = PreparedRoot() + "/test-khronos-glTF-ABeautifulGame/scene.gltf";
  GltfImporter loaded;
  const auto ready = loaded.load(path);
  CHECK(ready.has_value(), "pinned Khronos chess geometry loads as native geometry");
  if (!ready) { return Report(); }
  Geometry native = loaded.geometry().clone();
  for (int surface = 0; surface < native.surfaces(); ++surface) {
    Material row = native.surfaceAt(MaterialInstance(surface));
    row.Unlit = true;
    CHECK(native.setSurface(MaterialInstance(surface), row), "native chess surface becomes unlit");
  }
  Geometry single;
  for (int image = 0; image < native.images(); ++image) {
    const ImageView source = native.imageAt(image);
    CHECK(single.addImage(source.WidthPx, source.HeightPx, source.Rgba),
          "single part copies image");
  }
  for (int surface = 0; surface < native.surfaces(); ++surface) {
    CHECK(single.addSurface(native.surfaceNameOf(surface),
                            native.surfaceAt(MaterialInstance(surface))),
          "single part copies material");
  }
  constexpr int selected = 0;
  const auto part = single.addPart(native.nameOf(selected), native.materialOf(selected));
  CHECK(part, "single part allocates");
  if (!part) { return Report(); }
  CHECK(single.setPlacement(*part, native.placementOf(selected)) &&
            single.setPositions(*part, native.positionsOf(selected)) &&
            single.setNormals(*part, native.normalsOf(selected)) &&
            single.setTexture(*part, native.textureOf(selected)) &&
            single.setTexture(*part, native.textureOf(selected, Geometry::UvSet::Uv1), 1) &&
            single.setTangents(*part, native.tangentsOf(selected)) &&
            single.setColours(*part, native.coloursOf(selected)) &&
            single.setTriangles(*part, native.trianglesOf(selected)),
        "single part copies every native attribute");
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {1280, 720};
  scenario.Render.Outputs = {"sceneLinear"};
  Scenario::View view;
  view.Id = "chess";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{2.781138576118416, 1.3202289998916963, 1.9473741958039332}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{0, 0.08449789705936794, 0}};
  view.Sees.setProjection(
      Camera::Perspective{.FovDeg = 0.47108996144172666 * 180 / std::numbers::pi,
                          .NearM = 3.107125103623776,
                          .FarM = 4.118946968135317});
  scenario.Views.push_back(view);
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }

  struct Sequence {
    bool Temporal;
    bool AdditionalReadbacks;
  };

  constexpr std::array sequences = {Sequence{.Temporal = false, .AdditionalReadbacks = false},
                                    Sequence{.Temporal = true, .AdditionalReadbacks = false},
                                    Sequence{.Temporal = true, .AdditionalReadbacks = true}};
  for (const Sequence sequence : sequences) {
    scenario.Render.Stages =
        sequence.Temporal
            ? std::vector<std::string>{}
            : std::vector<std::string>{
                  "subjects", "subjectsTransmissive", "compositeTransmission", "overlay"};
    std::printf(
        "temporal=%d additionalReadbacks=%d\n", sequence.Temporal, sequence.AdditionalReadbacks);
    Engine engine;
    if (!accepted(engine.drawsInto({1280, 720})) || !accepted(engine.declare(scenario)) ||
        !accepted(engine.setGeometry(single)) || !accepted(engine.assemble()) ||
        !accepted(engine.advance())) {
      return Report();
    }
    std::vector<float> first, repeated, firstDepth, repeatedDepth;
    CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, first),
          "first chess frame renders");
    if (sequence.AdditionalReadbacks) {
      CHECK(engine.renderer().readPixels(Buffer::Depth, firstDepth).has_value(),
            "first depth is read");
    }
    CHECK(first.size() == 1280u * 720u * 4u, "the full linear frame is read");
    CHECK(std::all_of(first.begin(), first.end(), [](float v) { return std::isfinite(v); }),
          "the frame contains finite values");
    size_t lit = 0;
    for (size_t at = 0; at + 3 < first.size(); at += 4) {
      if (first[at] > 0 || first[at + 1] > 0 || first[at + 2] > 0) { ++lit; }
    }
    CHECK(lit > 0, "the chess frame contains visible geometry");
    if (sequence.AdditionalReadbacks) {
      std::filesystem::create_directories("build/native-materials");
      CHECK(engine.renderer()
                .saveScreenshot("build/native-materials/chess-native-first.png")
                .has_value(),
            "first chess PNG is written");
    }
    for (int repeat = 0; repeat < 3; ++repeat) {
      CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeated),
            "resident chess frame renders again");
      size_t changed = 0, firstChanged = first.size();
      float worst = 0;
      for (size_t at = 0; at < std::min(first.size(), repeated.size()); ++at) {
        if (first[at] != repeated[at]) {
          ++changed;
          firstChanged = std::min(firstChanged, at);
        }
        worst = std::max(worst, std::abs(first[at] - repeated[at]));
      }
      if (sequence.AdditionalReadbacks) {
        CHECK(engine.renderer().readPixels(Buffer::Depth, repeatedDepth).has_value(),
              "repeated depth is read");
        size_t depthChanges = 0;
        float depthWorst = 0;
        for (size_t at = 0; at < std::min(firstDepth.size(), repeatedDepth.size()); ++at) {
          depthChanges += firstDepth[at] != repeatedDepth[at];
          depthWorst = std::max(depthWorst, std::abs(firstDepth[at] - repeatedDepth[at]));
        }
        std::printf("depth changed=%zu worst=%g\n", depthChanges, static_cast<double>(depthWorst));
      }
      std::printf("repeat=%d changed=%zu first=%zu worst=%g\n",
                  repeat,
                  changed,
                  firstChanged,
                  static_cast<double>(worst));
      CHECK(first == repeated,
            sequence.Temporal ? "temporally resolved chess repeats every linear channel exactly"
                              : "unresolved chess repeats every linear channel exactly");
    }
    if (sequence.AdditionalReadbacks) {
      std::filesystem::create_directories("build/native-materials");
      CHECK(engine.renderer()
                .saveScreenshot("build/native-materials/chess-native-repeat.png")
                .has_value(),
            "chess repeatability PNG is written");
    }
  }
  return Report();
}
