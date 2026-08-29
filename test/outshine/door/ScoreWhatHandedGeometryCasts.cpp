#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include <Event.h>
#include <Geometry.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// GEOMETRY HANDED IN CASTS WHAT IT DRAWS, and it did not. `ScoreWhatAClientHandsIn` proves the
// two producers agree about the COLOUR pass, pixel for pixel. They disagreed about the depth the
// light sees: a subject handed through `Engine::Stands` reported `batches the shadow casts` = 0
// where the same shape declared as an asset cast every batch.
//
// The cause was a plan that outlived its declaration. `Live::Build` recompiled only when `Moves_`
// changed, so a scenario standing NOTHING compiled a plan without `Stage::LightVisibility` -- the
// shadow radius derives from the subject's extent and there was no subject -- and the plan that
// followed, with a subject in it, kept the shadowless one. The plan now follows the DECLARATION:
// `PlanSpec` compares, and a spec that differs recompiles.
//
// This case needs no file, because the claim is about the door's own producer alone: a subject
// handed in draws N batches and casts N.
constexpr int kFramePx = 96;

constexpr float kPositions[18] = {-1.0f, -1.0f, 0.0f, 1.0f,  -1.0f, 0.0f, 1.0f,  1.0f, 0.0f,
                                  -1.0f, -1.0f, 0.0f, 1.0f,  1.0f,  0.0f, -1.0f, 1.0f, 0.0f};
constexpr float kNormals[18] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
constexpr uint32_t kIndices[6] = {0, 1, 2, 3, 4, 5};

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(outshine::Roots{".", "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;

  // THE SCENARIO NAMES NO ASSET, which is the arm that broke it: the plan compiled for a subject
  // that was not there, and everything handed in afterwards lived under that plan.
  if (!engine.declare(stands)) {
    Unprepared(("the scenario would not stand: " + engine.error()).c_str());
    return Report();
  }
  const double drewEmpty = Measured(engine, "batches the picture draws");

  outshine::Geometry geometry;
  const int part = geometry.addPart("handed", outshine::MaterialInstance(0));
  const bool filled = geometry.setPositions(part, std::span<const float>(kPositions, 18)) &&
                      geometry.setNormals(part, std::span<const float>(kNormals, 18)) &&
                      geometry.setTriangles(part, std::span<const uint32_t>(kIndices, 6));
  if (!filled) {
    Unprepared("the builder refused the fixture, so there is nothing to hand in");
    return Report();
  }

  const bool handed = engine.setGeometry(geometry);
  if (!handed) { std::printf("STANDS REFUSED  %s\n", engine.error().c_str()); }
  if (handed && !engine.renderer().render(outshine::Extent{})) {
    Unprepared(("the device would not draw the handed subject: " + engine.error()).c_str());
    return Report();
  }

  const double drawn = Measured(engine, "batches the picture draws");
  const double cast = Measured(engine, "batches the shadow casts");
  std::printf("BEFORE ANYTHING STOOD  the picture drew %.0f batch(es)\n", drewEmpty);
  std::printf("HANDED IN              draws %.0f, casts %.0f\n", drawn, cast);

  CHECK(handed, "the geometry stands at all, so the counts below mean something");
  CHECK(drawn > 0.0,
        "and it DRAWS: the colour pass has it, which `ScoreWhatAClientHandsIn` shows agrees with "
        "the reader pixel for pixel");
  CHECK(cast == drawn,
        "**AND IT CASTS EVERY BATCH IT DRAWS**: geometry that reaches the colour pass and not the "
        "depth the light sees casts a PARTIAL shadow -- a body lit straight through itself. It "
        "cast NOTHING before the plan was made to follow the declaration: a scenario standing no "
        "subject compiled a plan without the light stage, because the shadow radius derives from "
        "an extent that was not there, and every subject handed in afterwards lived under it");

  Covers("the door: a subject handed in through the geometry value casts every batch it draws, "
         "and the render plan follows the declaration rather than outliving it");
  return Report();
}
