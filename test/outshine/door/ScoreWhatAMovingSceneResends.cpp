#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// UNREAL KEEPS `FScene` BESIDE `UWorld` AND FEEDS IT DELTAS -- added, removed, transform changed --
// and that separation is what lets render state survive a frame at all. State that survives the
// frame is the precondition for state that lives on the DEVICE, so a renderer handed a fresh table
// every frame does not merely waste the copy: it forecloses the GPU-driven path entirely.
//
// This tree handed the renderer the WHOLE placement table every frame. The claim that it no longer
// does could not be proven until a scene held something moving beside something still, and a drive
// is exactly that: the car's parts move and the ground ring under it does not.
//
// THE ORACLE IS THE COST RULE CLAUDE.md STATES: the work a declaration causes is proportional to
// what it CHANGED, never to how big it is. So the rows a frame re-sends must equal what moved and
// not what exists -- and the two differ here by the ring, which is drawn every frame and placed
// once.
constexpr int kFrames = 24;
constexpr const char *kScenario = "apps/driver/src/f31.scenario";

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.Numbers()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be driven");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", true});
  if (!engine.DrawsInto(outshine::Extent{320, 180})) {
    Unprepared("the device stood no canvas");
    return Report();
  }
  if (!engine.Read(kScenario)) {
    Unprepared(("the declaration would not read: " + engine.Error()).c_str());
    return Report();
  }
  outshine::Scenario declared = engine.Declared();
  declared.Render.Frame = outshine::Extent{320, 180};
  if (!engine.Declare(declared) || !engine.Assemble()) {
    Unprepared(("the drive needs terrain and OSM tiles and this machine has none cached: " +
                engine.Error())
                   .c_str());
    return Report();
  }

  double rowsAt[2] = {0.0, 0.0};
  double batches = 0.0;
  for (int half = 0; half < 2; ++half) {
    for (int step = 0; step < kFrames; ++step) {
      if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) { break; }
    }
    rowsAt[half] = Measured(engine, "placement rows the renderer has been sent");
    batches = Measured(engine, "batches the picture draws");
  }

  const double perFrame = (rowsAt[1] - rowsAt[0]) / (double)kFrames;
  std::printf("THE PICTURE DRAWS      %.0f batch(es) every frame\n", batches);
  std::printf("AND RE-SENDS           %.2f placement row(s) every frame\n", perFrame);
  std::printf("SO IT LEAVES           %.2f of them alone\n", batches - perFrame);

  CHECK(batches > 1.0 && perFrame > 0.0,
        "the scene holds more than one placed thing and something is moving, so the comparison "
        "below is between what moved and what exists rather than between two zeroes");
  CHECK(perFrame < batches - 0.5,
        "**A FRAME RE-SENDS WHAT MOVED, NOT WHAT EXISTS**: the ground ring is drawn every frame "
        "and placed ONCE, while the car's parts are placed every frame because they move. The "
        "renderer was handed the whole table each frame until this stood, so the cost of placing "
        "a scene scaled with the scene rather than with its motion -- and CLAUDE.md's rule is "
        "that the work a declaration causes is proportional to what it CHANGED");
  CHECK(std::fabs(perFrame - std::round(perFrame)) < 0.01,
        "and the count is a whole number of rows per frame, so what is being read is rows and not "
        "an average over frames that placed different amounts");

  Covers("the render: the renderer keeps its placement table across frames and the world sends "
         "only the rows that moved, measured over a drive where the car moves and the ground "
         "under it does not");
  return Report();
}
