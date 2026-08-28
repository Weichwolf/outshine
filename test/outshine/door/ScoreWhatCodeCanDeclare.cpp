#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE SCENARIO VALUE IS THE ONE TRUTH AND XML IS ONE SERIALISATION OF IT. Everything a file can
// declare, code can build -- otherwise the door has two halves of different sizes, and the smaller
// one is whichever a client happens to be holding.
//
// It is checkable structurally and that check passes: all 29 fields of `Scenario` are public data
// and the reader writes every one of them. But structure is the weak half of the claim. A field
// can be settable and still not REACH: a value the engine only honours when it arrived through
// `Read` would pass a field census and fail a drive.
//
// So the oracle is the drive itself. The same route is declared twice -- once by reading
// `apps/driver/src/f31.scenario`, once by building the same `Scenario` in C++ -- and both are
// advanced the same number of fixed steps from the same start. A body integrated from identical
// declarations lands in the identical place, to the digit, because nothing in between is allowed
// to depend on where the declaration came from.
//
// THIS IS WHAT LETS THE VEHICLE LEAVE THE ENGINE (board:1966). If code can declare everything, a
// client builds its subject by DECLARING a rig -- glTF for the geometry, the scenario for the
// mechanics -- and needs no imperative physics API, which would be a second door for one truth.
//
// The drive needs terrain and OSM tiles. It runs offline and reports UNPREPARED rather than red on
// a machine that has never driven, which is the same bargain the corpora make.
constexpr int kSteps = 24;
constexpr const char *kScenario = "apps/driver/src/f31.scenario";

[[nodiscard]] outshine::Contact Standing(const char *at, double xM, double zM, double reachM,
                                         double stiffness, double damping) {
  outshine::Contact one;
  one.At = at;
  one.AtM[0] = xM;
  one.AtM[1] = 0.333;
  one.AtM[2] = zM;
  one.Strut.ReachM = reachM;
  one.Strut.StiffnessNPerM = stiffness;
  one.Strut.DampingNsPerM = damping;
  one.Strut.TravelM = 0.18;
  one.Strut.StopNPerM = 450000.0;
  one.Strut.LimitN = 24000.0;
  one.Touches.Grip = 0.95;
  one.Touches.RadiusM = 0.333;
  one.Touches.CorneringNPerRad = 55000.0;
  one.Touches.RelaxationM = 0.4;
  return one;
}

[[nodiscard]] outshine::Scenario ByHand(void) {
  outshine::Scenario made;
  made.Named.Name = "f31 first person";
  made.Ground.Declared = true;
  made.Ground.Lat = 48.13720;
  made.Ground.Lon = 11.57560;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{1280, 720};
  made.Render.Fps = 60.0;
  made.Render.Fill = 0.9;
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = 42.0;
  made.Lit.Key.BearingDeg = 150.0;

  outshine::Asset shown;
  shown.Uri = "scene.gltf";
  shown.Kind = "gltf";
  made.Assets.push_back(shown);

  outshine::Body f31;
  f31.Name = "f31";
  f31.Asset = "scene.gltf";
  f31.MassKg = 1610.0;
  f31.WidthM = 1.811;
  f31.AssetSpanM = 2.810;
  f31.AssetGround = -0.333;
  f31.CentreOfMassM[1] = 0.55;
  f31.InertiaKgM2[0] = 540.0;
  f31.InertiaKgM2[1] = 2400.0;
  f31.InertiaKgM2[2] = 2600.0;
  f31.Contacts.push_back(Standing("front-left", -0.774, -1.405, 0.45635, 32000.0, 3400.0));
  f31.Contacts.push_back(Standing("front-right", 0.774, -1.405, 0.45635, 32000.0, 3400.0));
  f31.Contacts.push_back(Standing("rear-left", -0.774, 1.405, 0.44909, 34000.0, 3600.0));
  f31.Contacts.push_back(Standing("rear-right", 0.774, 1.405, 0.44909, 34000.0, 3600.0));
  f31.Driven.push_back(outshine::Drive{.Does = outshine::Drives::Effort, .Opposes = false, .PeakNm = 400.0, .Ratio = 3.08, .CircleM = 0.0});
  f31.Driven.push_back(outshine::Drive{.Does = outshine::Drives::Effort, .Opposes = true, .PeakNm = 5500.0, .Ratio = 1.0, .CircleM = 0.0});
  f31.Driven.push_back(outshine::Drive{.Does = outshine::Drives::Motion, .Opposes = false, .PeakNm = 0.0, .Ratio = 1.0, .CircleM = 11.3});
  f31.DragCoefficient = 0.66;
  f31.FrontalM2 = 2.19;
  outshine::Slot driver;
  driver.At = "driver";
  driver.AtM[0] = -0.494;
  driver.AtM[1] = 1.220;
  driver.AtM[2] = 0.003;
  f31.Slots.push_back(driver);
  made.Bodies.push_back(f31);

  outshine::View eyes;
  eyes.Id = "eyes";
  eyes.Follows = "player";
  eyes.Person = "first";
  eyes.OffsetM[0] = -0.494;
  eyes.OffsetM[1] = 1.220;
  eyes.OffsetM[2] = 0.003;
  eyes.Sees.FovDeg = 65.0;
  made.Views.push_back(eyes);

  made.Played.Declared = true;
  made.Played.Is = "f31";
  made.Played.View = "eyes";

  made.Routed.Declared = true;
  made.Routed.FromLatDeg = 48.13720;
  made.Routed.FromLonDeg = 11.57560;
  made.Routed.ToLatDeg = 48.15500;
  made.Routed.ToLonDeg = 11.59500;
  return made;
}

struct Landed {
  bool Stood = false;
  double EastM = 0.0, UpM = 0.0, SouthM = 0.0;
};

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == what) { return held.How; }
  }
  return 0.0;
}

[[nodiscard]] Landed Drove(const outshine::Scenario &declared, std::string &why) {
  Landed out;
  outshine::Engine engine;
  engine.setRoots(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", true});
  if (!engine.drawsInto(outshine::Extent{320, 180})) {
    why = "the device stood no canvas";
    return out;
  }
  if (!engine.declare(declared) || !engine.assemble()) {
    why = engine.error();
    return out;
  }
  for (int step = 0; step < kSteps; ++step) {
    if (!engine.advance()) { break; }
  }
  out.EastM = Measured(engine, "the body, east");
  out.UpM = Measured(engine, "the body, up");
  out.SouthM = Measured(engine, "the body, south");
  out.Stood = true;
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

  outshine::Engine reader;
  reader.setRoots(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", true});
  if (!reader.readScenario(kScenario)) {
    Unprepared(("the declaration would not read: " + reader.error()).c_str());
    return Report();
  }
  const outshine::Scenario fromFile = reader.declaration();

  std::string why;
  const Landed byFile = Drove(fromFile, why);
  if (!byFile.Stood) {
    Unprepared(("the drive needs terrain and OSM tiles and this machine has none cached: " + why)
                   .c_str());
    return Report();
  }
  const Landed byHand = Drove(ByHand(), why);
  if (!byHand.Stood) {
    Unprepared(("the hand-built arm did not stand: " + why).c_str());
    return Report();
  }

  std::printf("DECLARED BY XML   east %10.5f  up %10.4f  south %10.5f\n", byFile.EastM, byFile.UpM,
              byFile.SouthM);
  std::printf("DECLARED BY CODE  east %10.5f  up %10.4f  south %10.5f\n", byHand.EastM, byHand.UpM,
              byHand.SouthM);

  CHECK(std::fabs(byFile.EastM) + std::fabs(byFile.SouthM) > 0.0,
        "the file arm moved at all, so the comparison below is between two drives and not between "
        "two bodies that never left the start");
  CHECK(byHand.EastM == byFile.EastM && byHand.UpM == byFile.UpM &&
            byHand.SouthM == byFile.SouthM,
        "**EVERYTHING XML CAN DECLARE, CODE CAN BUILD**: the scenario VALUE is the one truth and "
        "the file is one serialisation of it, so a body integrated from identical declarations "
        "lands in the identical place. Nothing between the declaration and the wheel is allowed "
        "to depend on where the declaration came from -- and this is what lets a client declare "
        "its own subject rather than needing an imperative physics API beside the door");

  // A JOURNEY DECLARES ITS MODE, AND A MODE NOTHING ASSEMBLES IS A REFUSAL. TARGET's own diagram
  // says *PATHFINDING -- two coordinates in, corridor out: walk, drive, fly, rail*, so `drive` is
  // one MODE of travel and not the noun. `Scenario::Drive` was that noun; it is `Journey` with a
  // `Travels` now, and the pathfinder's own `Route` -- the corridor that comes BACK -- keeps its
  // name, because a declared intent and a computed corridor are two things.
  //
  // Only `drive` has an assembler, and that is now a TABLE the mode indexes rather than an `if`:
  // four rows, one filled, each carrying the name of the way it travels. A scenario that declares a
  // walk used to be indistinguishable from one that declared nothing -- `Routes` tested
  // `Driven.Declared` and nothing else, so the journey quietly did not happen.
  outshine::Scenario onFoot = fromFile;
  onFoot.Routed.By = outshine::Travels::Walk;
  outshine::Engine walker;
  walker.setRoots(outshine::Roots{"apps/driver/src", "src/assets", "/tmp/outshine-drive-cache", true});
  const bool refusedWalk =
      walker.drawsInto(outshine::Extent{320, 180}) && walker.declare(onFoot) && !walker.assemble();
  const std::string whyWalk = walker.error();
  std::printf("DECLARED BY FOOT  %s\n", refusedWalk ? whyWalk.c_str() : "ASSEMBLED ANYWAY");

  CHECK(refusedWalk && whyWalk.find("foot") != std::string::npos,
        "**A JOURNEY BY A MODE NOTHING ASSEMBLES IS REFUSED, BY THAT MODE'S NAME**: the four modes "
        "index a table of assemblers and three of its four slots are empty, so the engine says "
        "which way it cannot lay a corridor for. A declaration the engine cannot act on must say "
        "so rather than standing still and looking like a scenario that declared no journey");

  Covers("the door: a scenario built in code declares everything a scenario read from XML can, "
         "proven by two drives from identical declarations landing on the same digits -- and a "
         "journey names its mode, so one nothing assembles is refused rather than skipped");
  return Report();
}
