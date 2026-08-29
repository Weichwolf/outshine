#ifndef OUTSHINE_TEST_RENDER_DRIVES_H
#define OUTSHINE_TEST_RENDER_DRIVES_H

#include <cmath>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

namespace outshine::Test {

// A CORPUS CASE AS A CLIENT SEES IT: a file, a camera, a light, a frame. Everything under this is
// the engine's business, and a harness that reaches past it measures the internals rather than the
// product -- which is what board:2038 found, and what this replaces.
//
// The LINE COUNT is the point. CLAUDE.md says a client is almost no code and its length measures
// the door; the file this stands in front of was 2430 lines with 41 includes and none from
// `include/`. If driving a case ever needs more than a screenful again, the door is the finding.
struct Drives {
  std::string Path;
  int WidePx = 512;
  int HighPx = 512;

  double AtM[3] = {0.0, 0.0, 0.0};
  double LookAtM[3] = {0.0, 0.0, 0.0};
  double RollRad = 0.0;
  double YfovRad = 0.0;
  double NearM = 0.0;
  double FarM = 0.0;

  Light Key;
  double IndirectLight[3] = {0.0, 0.0, 0.0};

  [[nodiscard]] bool Renders(Engine &engine, std::string &why) const {
    Scenario stands;
    Asset shown;
    shown.Uri = Path;
    shown.Kind = "gltf";
    shown.Animation = AssetAnimation::Ignore;
    stands.Assets.push_back(shown);

    stands.Render.Declared = true;
    stands.Render.Frame = Extent{WidePx, HighPx};
    stands.Lit.Declared = true;
    stands.Lit.Key = Key;
    for (int channel = 0; channel < 3; ++channel) {
      stands.Lit.IndirectLight[channel] = IndirectLight[channel];
    }

    View watches;
    watches.Id = "oracle";
    watches.Sees.Placed = true;
    watches.Sees.LooksAt = true;
    watches.Sees.RollRad = RollRad;
    watches.Sees.NearM = NearM;
    watches.Sees.FarM = FarM;
    watches.Sees.FovDeg = YfovRad * 180.0 / std::numbers::pi;
    for (int axis = 0; axis < 3; ++axis) {
      watches.Sees.Stands.AtM[axis] = AtM[axis];
      watches.Sees.LookAtM[axis] = LookAtM[axis];
    }
    stands.Views.push_back(watches);

    if (!engine.declare(stands) || !engine.assemble()) {
      why = engine.error();
      return false;
    }
    for (int frame = 0; frame < engine.renderer().settleFrames(); ++frame) {
      if (!engine.renderer().render(Extent{})) {
        why = engine.error();
        return false;
      }
    }
    return true;
  }
};

}
#endif
