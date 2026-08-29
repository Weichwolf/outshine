#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// BOTH BENCHMARKS ARE BUILT AROUND THIS RATHER THAN HAVING ADDED IT. Unreal's World Partition
// replaced the hand-placed level and made the cell grid the world's own shape; RAGE streams map
// nodes and an `fwEntity` belongs to the node it stands in. Neither has a code path that assumes
// the world is resident, which is why neither has one to repair.
//
// THE ORACLE IS WHAT STREAMING MEANS and it owes nothing to our design: if the world streams, what
// is HELD is set by how far you can see, not by how far you could go. So the same start with a
// destination three times further away must hold about the same, and a world that grew with the
// route would be a world that fits until it does not.
//
// Two routes over the same city, one 0.871 km and one 2.916 km, each composed and driven twenty
// steps from the same start. What is compared is the heap the process holds -- crude, and the
// right crudeness: a residency measure that counted only what someone remembered to register would
// miss exactly the thing that grows.
//
// WHAT THIS DOES NOT SHOW, MEASURED RATHER THAN ASSUMED. The tile pool's byte budget is 64 MB and
// these routes hold 58, so the EVICTION path is never entered -- disabling it entirely leaves this
// case green at 1.024 against 1.012. So the reading below says the composed corridor is bounded by
// what can be SEEN rather than by the route, and it says nothing about what happens when the budget
// is reached, because nothing this tree can currently drive reaches it: the connected road graph
// around the declared start refuses a destination much beyond three kilometres. That gap is
// board:1955's and it is named there rather than papered over here.
//
// The drive needs terrain and OSM tiles, so this runs OFFLINE and reports UNPREPARED rather than
// red on a machine that has never driven. Pinning them is board:1964's remaining half.
constexpr int kSteps = 20;
constexpr const char *kScenario = "src/assets/drive/f31.scenario";

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

struct Held {
  bool Stood = false;
  double Bytes = 0.0;
  double CorridorM = 0.0;
};

[[nodiscard]] Held Drove(double toLat, double toLon, std::string &why) {
  Held out;
  outshine::Engine engine;
  engine.setRoots(outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", true});
  if (!engine.drawsInto(outshine::Extent{320, 180})) {
    why = "the device stood no canvas";
    return out;
  }
  if (!engine.readScenario(kScenario)) {
    why = engine.error();
    return out;
  }
  outshine::Scenario declared = engine.declaration();
  declared.Render.Frame = outshine::Extent{320, 180};
  declared.Routed.ToLatDeg = toLat;
  declared.Routed.ToLonDeg = toLon;
  if (!engine.declare(declared) || !engine.assemble()) {
    why = engine.error();
    return out;
  }
  for (int step = 0; step < kSteps; ++step) {
    if (!engine.advance()) { break; }
  }
  out.Bytes = Measured(engine, "bytes the world holds while it drives");
  out.CorridorM = Measured(engine, "how long the corridor is");
  out.Stood = out.Bytes > 0.0 && out.CorridorM > 0.0;
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be driven");
    return Report();
  }

  std::string why;
  const Held near = Drove(48.14200, 11.58100, why);
  if (!near.Stood) {
    Unprepared(("the drive needs terrain and OSM tiles and this machine has none cached: " + why)
                   .c_str());
    return Report();
  }
  const Held far = Drove(48.15500, 11.59500, why);
  if (!far.Stood) {
    Unprepared(("the longer route did not stand: " + why).c_str());
    return Report();
  }

  const double furtherBy = far.CorridorM / near.CorridorM;
  const double heavierBy = far.Bytes / near.Bytes;
  std::printf("A ROUTE OF %8.1f m   holds %10.0f bytes\n", near.CorridorM, near.Bytes);
  std::printf("A ROUTE OF %8.1f m   holds %10.0f bytes\n", far.CorridorM, far.Bytes);
  std::printf("SO %.2f TIMES THE ROUTE COSTS %.3f TIMES THE MEMORY\n", furtherBy, heavierBy);

  CHECK(furtherBy > 2.0,
        "the second route really is more than twice the first, so what follows is read across a "
        "difference in extent and not across two routes that happen to be the same size");
  CHECK(heavierBy < 1.2,
        "**A WIDER WORLD DOES NOT HOLD MORE**: three times the route costs a few percent of the "
        "memory, because what is composed is set by how far you can SEE and not by how far you "
        "could GO. A world that grew with the route is a world that fits until it does not, and "
        "both benchmarks are built around the cell grid rather than having added it. This is the "
        "COMPOSITION being bounded; the eviction path is not reached at this size and board:1955 "
        "carries that half");
  CHECK(heavierBy > 0.8,
        "and it did not SHRINK either, which would mean the longer route composed less world "
        "rather than the same amount -- a reading that would say the case measured the wrong "
        "thing rather than that streaming works");

  Covers("the world: what the engine COMPOSES is set by how far it can see rather than by how far "
         "the route runs, measured over two routes differing more than threefold in length -- "
         "below the tile budget, where nothing is evicted");
  return Report();
}
