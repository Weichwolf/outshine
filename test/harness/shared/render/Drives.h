#ifndef OUTSHINE_TEST_RENDER_DRIVES_H
#define OUTSHINE_TEST_RENDER_DRIVES_H

#include <numbers>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

namespace outshine::Test {

// A CORPUS CASE AS A CLIENT SEES IT: a file, whose surfaces, a camera, a light, an hour, and which
// pictures the frame is to keep. Everything under this is the engine's business, and a harness
// that reaches past it measures the internals rather than the product -- which is board:2038's
// finding, and what this replaces.
//
// THE LINE COUNT IS THE POINT. CLAUDE.md: a client is almost no code and its length measures the
// door. The file this stands in front of is 2430 lines with 41 includes and none from `include/`.
// If driving a case ever needs more than a screenful again, the door is the finding.
struct Drives {
  std::string Path;
  std::vector<SurfaceOverride> Surfaces;
  Extent Frame{512, 512};

  double AtM[3] = {0.0, 0.0, 0.0};
  double LookAtM[3] = {0.0, 0.0, 0.0};
  double UpM[3] = {0.0, 1.0, 0.0};
  double YfovRad = 0.0, NearM = 0.0, FarM = 0.0;

  // GLTF HAS TWO CAMERAS AND A CASE MAY DECLARE EITHER. Passing only the field of view drew an
  // orthographic subject in perspective, which is the largest coverage difference available -- and
  // coverage is what a per-pixel colour metric reads when the shape moves.
  bool Orthographic = false;
  double XMagM = 0.0, YMagM = 0.0;

  Light Key;
  double IndirectLight[3] = {0.0, 0.0, 0.0};
  Weather Sky;
  std::string StartUtc;

  std::vector<std::string> Stages, Keeps;
  std::string Transfer, Precision;
  double Exposure = 0.0;
  double Fps = 60.0;
  bool Animated = false;

  [[nodiscard]] Scenario Declared() const {
    Scenario stands;
    Asset shown;
    shown.Uri = Path;
    shown.Kind = "gltf";
    shown.Animation = Animated ? AssetAnimation::Play : AssetAnimation::Ignore;
    shown.Surfaces = Surfaces;
    stands.Assets.push_back(shown);

    stands.Render.Declared = true;
    stands.Render.Frame = Frame;
    stands.Render.Fps = Fps;
    stands.Render.Stages = Stages;
    stands.Render.Outputs = Keeps;
    stands.Render.Transfer = Transfer;
    stands.Render.Precision = Precision;
    stands.Render.Exposure = Exposure;

    stands.Lit.Declared = true;
    stands.Lit.Key = Key;
    for (int channel = 0; channel < 3; ++channel) {
      stands.Lit.IndirectLight[channel] = IndirectLight[channel];
    }
    stands.Ground.Sky = Sky;
    stands.Time.Declared = true;
    stands.Time.Live = StartUtc.empty();
    stands.Time.Start = StartUtc;

    View watches;
    watches.Id = "oracle";
    watches.Sees.Placed = true;
    watches.Sees.LooksAt = true;
    for (int axis = 0; axis < 3; ++axis) { watches.Sees.UpM[axis] = UpM[axis]; }
    if (Orthographic) {
      watches.Sees.setProjection(-XMagM, XMagM, -YMagM, YMagM, NearM, FarM);
    } else {
      watches.Sees.setProjection(YfovRad * 180.0 / std::numbers::pi, NearM, FarM);
    }
    for (int axis = 0; axis < 3; ++axis) {
      watches.Sees.Stands.AtM[axis] = AtM[axis];
      watches.Sees.LookAtM[axis] = LookAtM[axis];
    }
    stands.Views.push_back(watches);
    return stands;
  }

  [[nodiscard]] bool Stands(Engine &engine, std::string &why) const {
    const Scenario said = Declared();
    if (!engine.declare(said) || !engine.assemble()) {
      why = engine.error();
      return false;
    }
    return true;
  }

  [[nodiscard]] bool Renders(Engine &engine, std::string &why) const {
    SwapChain into = engine.swapChain();
    Renderer draws = engine.renderer();
    if (!draws.beginFrame(into)) {
      why = engine.error();
      return false;
    }
    for (int frame = 0; frame < draws.settleFrames(); ++frame) {
      if (!draws.render(Extent{})) {
        why = engine.error();
        return false;
      }
    }
    if (!draws.endFrame() || !draws.flushAndWait()) {
      why = engine.error();
      return false;
    }
    return true;
  }
};

} // namespace outshine::Test
#endif
