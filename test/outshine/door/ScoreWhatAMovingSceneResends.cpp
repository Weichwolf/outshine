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
  double batches = 0.0, moving = 0.0, furthest = 0.0;
  double ways = 0.0, water = 0.0, prints = 0.0, grown = 0.0, reached = 0.0;
  for (int half = 0; half < 2; ++half) {
    for (int step = 0; step < kFrames; ++step) {
      if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) { break; }
    }
    rowsAt[half] = Measured(engine, "placement rows the renderer has been sent");
    batches = Measured(engine, "batches the picture draws");
    moving = Measured(engine, "pixels the velocity target says moved");
    ways = Measured(engine, "streets the world holds");
    water = Measured(engine, "water surfaces it holds");
    prints = Measured(engine, "building footprints it holds");
    grown = Measured(engine, "bodies the world's generators placed");
    reached = Measured(engine, "how far the placement chain reached");
    furthest = Measured(engine, "the furthest any of them moved");
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
  std::printf("AND ITS VELOCITY SAYS  %.0f pixel(s) moved, the furthest by %.6g ndc\n",
              moving, furthest);
  // THIS SCENE CARRIES A VELOCITY TARGET AND ITS ASSET ANIMATES NOTHING, which is the whole of
  // what this number says here. `DeclarePlan` used to add `SceneVelocity` only when the glTF
  // ANIMATED, so the one scene in the tree where things actually move -- a car re-placed nine
  // rows a frame over a ground ring placed once -- rendered without one, and this number read -1.
  // The catalogue already says `subjects` writes velocity unconditionally; the output now follows
  // the catalogue rather than a property of the file.
  //
  // WHAT IT DOES NOT SAY, measured rather than assumed: it does not prove the placement row's
  // PREVIOUS half. With `was = now` forced in `HandPlacements` this case reads the same 57600 px
  // and the same 0.695012 ndc, because the camera drives too and its motion sets both numbers.
  // Isolating a placement's own contribution needs a STILL camera over a moving subject, and no
  // case in this tree has one. That is board:1998's remaining predicate and it is not this case.
  CHECK(moving > 0.0,
        "**A SCENE THAT MOVES CARRIES A VELOCITY TARGET, WHETHER OR NOT ITS ASSET ANIMATES**: "
        "Unreal's base pass writes velocity whenever TAA can run and RAGE keeps the same buffer "
        "for its own reprojection; neither asks whether the mesh has an animation track. A target "
        "conditioned on the FILE rather than on the plan leaves the one scene that moves without "
        "the buffer that describes its motion");

  std::printf("AND THE WORLD HOLDS    %.0f street(s), %.0f water surface(s), %.0f footprint(s)\n",
              ways, water, prints);
  // THE WORLD'S VECTOR DATA HAS AN OWNER. Unreal's PCG reads a level's own data because the level
  // owns it for as long as it is loaded; RAGE keeps map data resident per node. Measured before
  // this: `Surrounds` held a height stack and no vector field at all, and the only `OsmField` the
  // tree ever built was a LOCAL in `Sim::DriveAssembly` that laid the road graph and died with the
  // call -- so `BuildingField::Build`, `WaterField::Ingest` and `StreetField::Ingest` had no
  // caller anywhere and the three derived fields were never populated by anything.
  // `GroundStack` owns them now, beside the class field it already owned, and `Restand` fills
  // them where the camera stands.
  std::printf("AND ITS GENERATORS PLACED %.0f bod(y|ies), chain reached step %.0f\n", grown, reached);
  // A SHIPPED GENERATOR IS RUN BY THE ENGINE, and this number says how far it gets. Unreal's PCG
  // is a plugin the level runs; RAGE has none. `Surrounds` now carries a placement registry
  // beside its `Generates` one, and the forest in it is built from DECLARATION and not from code:
  // species heights from `src/assets/world/species/`, per-template tree density from
  // `vegetation.json`'s `trees.perM2`, and the treeline from its `alpineLimit`.
  //
  // MEASURED, AND THREE WRITTEN CAUSES DIED ON THE WAY. `SnapshotOver` takes ONE `Tile` for two
  // sources at different resolutions: the vector provider's finest is 14 and the ground stream
  // serves only its own zoom -- `GroundStream::BlockAt` opens with
  // `if (z != Surface_.Z) return block;`. Both benchmarks sample each streaming source at its own
  // granularity into one working frame rather than forcing a tile index on both, so `GroundQuery`
  // gained `BlockZoom()` and the snapshot asks for the ANCESTOR block that contains its region.
  // Then the classes and the features were still missing, and the reason was the same one twice:
  // `Composes` and `Restand` each ran ONCE, in assembly, against builders that are asynchronous.
  // Both are per-frame now, and the anchor two of the fields assert on moved with them -- it
  // belongs to setting a field up, not to filling it.
  CHECK(reached >= 40.0,
        "the placement chain is entered at all: the registry is not empty, the ground table "
        "stands and the vector fields are held, so what stops it is the snapshot and not a "
        "missing part -- a step below 40 means one of those three went away again");

  CHECK(grown > 0.0,
        "**THE ENGINE RUNS A SHIPPED PLACEMENT GENERATOR OVER ITS OWN WORLD**: Unreal's PCG is a "
        "plugin the LEVEL runs and RAGE keeps map data resident per node; in both, what places "
        "things reads the world the game is standing in. The forest here is built from "
        "DECLARATION and not from code -- species heights from `src/assets/world/species/`, "
        "per-template density from `vegetation.json`'s `trees.perM2`, the treeline from its "
        "`alpineLimit` -- and a generator tier only a test can reach is a library the product "
        "does not use");

  CHECK(ways > 0.0,
        "**THE WORLD HOLDS ITS OWN VECTOR DATA**: a drive through a city crosses streets, and a "
        "ground that cannot say so hands every generator an unmapped world. This is what "
        "`SnapshotOver` needs before it will answer `Taken` at all, so it is the difference "
        "between a placement generator that reads the world and one that reads nothing");

  CHECK(std::fabs(perFrame - std::round(perFrame)) < 0.01,
        "and the count is a whole number of rows per frame, so what is being read is rows and not "
        "an average over frames that placed different amounts");

  Covers("the render: the renderer keeps its placement table across frames and the world sends "
         "only the rows that moved, measured over a drive where the car moves and the ground "
         "under it does not");
  return Report();
}
