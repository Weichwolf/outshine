#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"

namespace {

constexpr int kWidePx = 320;
constexpr int kHighPx = 180;
constexpr double kPatienceS = 15.0;
constexpr double kSightM = 8000.0;
constexpr double kLatDeg = 49.3777;
constexpr double kLonDeg = 10.179;
constexpr double kBearingDeg = 70.0;
constexpr double kEyeAglM = 1.7;
constexpr double kPitchDeg = 0.0;
constexpr double kFovDeg = 55.0;

[[nodiscard]] double Measured(const std::vector<outshine::Measure> &measures,
                              const std::string &what) {
  for (const outshine::Measure &one : measures) {
    if (one.What == what) { return one.How; }
  }
  return -1.0;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(
      outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{kWidePx, kHighPx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario::Document stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = kLatDeg;
  stands.Ground.Origin.LongitudeDeg = kLonDeg;
  stands.Ground.PatienceS = 3.0;
  stands.Ground.SightM = kSightM;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kWidePx, kHighPx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  outshine::Scenario::View watches;
  watches.Id = "street";
  watches.Person = "first";
  watches.Sees.Stands.GlobeAnchor = true;
  watches.Sees.Stands.Geodetic.LatitudeDeg = kLatDeg;
  watches.Sees.Stands.Geodetic.LongitudeDeg = kLonDeg;
  watches.Sees.Stands.Geodetic.HeightM = kEyeAglM;
  watches.Sees.Stands.SamplesHeight = true;
  watches.Sees.Stands.BearingDeg = kBearingDeg;
  watches.Sees.Stands.PitchDeg = kPitchDeg;
  watches.Sees.FovDeg = kFovDeg;
  stands.Views.push_back(watches);
  if (!(engine.declare(stands) && engine.assemble() && engine.preload(kPatienceS) &&
        engine.advance())) {
    Unprepared(("this place needs terrain and vector tiles and this machine has none cached: " +
                engine.error())
                   .c_str());
    return Report();
  }

  const std::vector<outshine::Measure> &told = engine.measures();
  const double took = Measured(told, "streets: the geometry took them");
  const double against = Measured(told, "streets: triangles wound against their normals");
  std::printf("STREETS  the geometry took them %.0f   triangles wound against their normals %.0f\n",
              took,
              against);

  CHECK(took == 1.0,
        "**THE STREETS MESH IS TAKEN AT THE DOOR**: every normal the road generator hands over is "
        "unit length, or the door refuses the part and the place shows no streets");

  CHECK(against == 0.0,
        "**NO STREETS TRIANGLE IS WOUND AGAINST ITS NORMALS**: junction bodies are flat faces with "
        "their own vertices, each wound counter-clockwise seen from its outward side; the count "
        "at the door reads zero, and the negative control lives beside the door "
        "(TheDoorRefusesANormalThatIsNotUnit)");

  Covers("board:2148 -- every mesh the road generator hands over faces outward with unit normals");
  return Report();
}
