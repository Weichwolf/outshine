#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
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
  constexpr int selected = 0;
  for (int image = 0; image < native.images(); ++image) {
    const ImageView source = native.imageAt(image);
    CHECK(single.addImage(source.WidthPx, source.HeightPx, source.Rgba),
          "single part copies image");
  }
  CHECK(single.addSurface(native.surfaceNameOf(native.materialOf(selected).index()),
                          native.surfaceAt(native.materialOf(selected))),
        "single part copies its material");
  const auto part = single.addPart(native.nameOf(selected), MaterialInstance(0));
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
  const auto withMipFilter = [](const Geometry &source, MipFilter filter) {
    Geometry result = source.clone();
    for (int surface = 0; surface < result.surfaces(); ++surface) {
      Material row = result.surfaceAt(MaterialInstance(surface));
      row.BaseColourMap.Sampler.Mip = filter;
      row.NormalMap.Sampler.Mip = filter;
      row.MetalRoughMap.Sampler.Mip = filter;
      row.EmissiveMap.Sampler.Mip = filter;
      row.SpecularStrengthMap.Sampler.Mip = filter;
      row.SpecularTintMap.Sampler.Mip = filter;
      if (!result.setSurface(MaterialInstance(surface), row)) { return Geometry{}; }
    }
    return result;
  };
  Geometry nearestMips = withMipFilter(single, MipFilter::Nearest);
  CHECK(nearestMips.parts() == single.parts(), "nearest-mip control retains native geometry");
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
  Scenario::View warm = view;
  warm.Id = "warm";
  warm.Sees.PositionM[0] += 0.02;
  warm.Sees.LookAtM[0] += 0.02;
  scenario.Views.push_back(warm);
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }

  struct Sequence {
    MipFilter Mip;
    const Geometry *Source;
    bool Warmup;
    bool SubjectOnly;
  };

  const std::array sequences = {
      Sequence{.Mip = MipFilter::Linear, .Source = &single, .Warmup = false, .SubjectOnly = false},
      Sequence{
          .Mip = MipFilter::Nearest, .Source = &nearestMips, .Warmup = false, .SubjectOnly = false},
      Sequence{.Mip = MipFilter::Linear, .Source = &single, .Warmup = true, .SubjectOnly = false},
      Sequence{.Mip = MipFilter::Linear, .Source = &single, .Warmup = false, .SubjectOnly = true}};
  for (const Sequence sequence : sequences) {
    scenario.Render.Stages =
        sequence.SubjectOnly
            ? std::vector<std::string>{"subjects"}
            : std::vector<std::string>{
                  "subjects", "subjectsTransmissive", "compositeTransmission", "overlay"};
    std::printf("mip=%d warmup=%d subjectOnly=%d\n",
                static_cast<int>(sequence.Mip),
                sequence.Warmup,
                sequence.SubjectOnly);
    Engine engine;
    if (!accepted(engine.setRenderTarget({1280, 720})) || !accepted(engine.declare(scenario))) {
      return Report();
    }
    if (!accepted(engine.setGeometry(*sequence.Source)) || !accepted(engine.assemble()) ||
        !accepted(engine.setView(sequence.Warmup ? "warm" : "chess")) ||
        !accepted(engine.advance())) {
      return Report();
    }
    if (sequence.Warmup) {
      std::vector<float> magnified;
      if (!accepted(engine.renderer().render({})) ||
          !accepted(engine.renderer().readPixels(Buffer::Linear, magnified)) ||
          !accepted(engine.setView("chess")) || !accepted(engine.advance())) {
        return Report();
      }
      size_t visible = 0;
      for (size_t at = 0; at + 3 < magnified.size(); at += 4) {
        visible += magnified[at] > 0 || magnified[at + 1] > 0 || magnified[at + 2] > 0;
      }
      CHECK(visible > 0, "shifted warm-up camera samples the resident linear-mip texture");
    }
    std::vector<float> first, previous, repeated;
    CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, first),
          "first chess frame renders");
    CHECK(first.size() == 1280u * 720u * 4u, "the full linear frame is read");
    CHECK(std::all_of(first.begin(), first.end(), [](float v) { return std::isfinite(v); }),
          "the frame contains finite values");
    size_t lit = 0;
    for (size_t at = 0; at + 3 < first.size(); at += 4) {
      if (first[at] > 0 || first[at + 1] > 0 || first[at + 2] > 0) { ++lit; }
    }
    CHECK(lit > 0, "the chess frame contains visible geometry");
    for (int repeat = 0; repeat < 3; ++repeat) {
      CHECK(engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear, repeated),
            "resident chess frame renders again");
      size_t changed = 0, firstChanged = first.size();
      float worst = 0;
      for (size_t at = 0; at < std::min(first.size(), repeated.size()); ++at) {
        if (first[at] != repeated[at]) {
          ++changed;
          firstChanged = std::min(firstChanged, at);
          if (repeat == 0) {
            std::printf("channel=%zu first=%a repeated=%a\n",
                        at,
                        static_cast<double>(first[at]),
                        static_cast<double>(repeated[at]));
          }
        }
        worst = std::max(worst, std::abs(first[at] - repeated[at]));
      }
      std::printf("repeat=%d changed=%zu first=%zu worst=%g\n",
                  repeat,
                  changed,
                  firstChanged,
                  static_cast<double>(worst));
      CHECK(first == repeated, "single native chess part repeats every linear channel exactly");
      if (!previous.empty()) {
        CHECK(previous == repeated, "resident chess frames repeat every linear channel exactly");
      }
      previous = repeated;
    }
  }
  return Report();
}
