#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE ORACLE IS FREE FALL, WHICH OWES NOTHING TO OUR DESIGN. A body released from rest under
// gravity alone has fallen `g t^2 / 2` after time `t` and is moving at `g t`. Both are closed forms
// and both are checked here against the engine's own declared gravity and step, so the case
// measures the engine rather than agreeing with it.
//
// WHAT IT CLOSES is a car's assumption wearing an engine's clothes. `Scenario::Body` carried
// everything about WHAT a body is and nothing about WHERE it is; its first position came from one
// place, `DriveAssembly.cpp:303`, computed from a route's start. So `Engine::State::Routes`
// returned early on `if (!declared.Routed.Declared)` and **a body without a journey could not
// stand**. A crate, a fallen tree, a parked lorry and a swinging door all have a place and no
// route.
//
// Both benchmarks join place and physics at the actor: Unreal's `AActor` has the transform and its
// `UPrimitiveComponent` carries the `FBodyInstance`; RAGE's `fwEntity` has the matrix and its
// `phInst` binds physics to it. Neither has a body that can only be placed by being sent somewhere.
//
// `Physics::Step(Body &, const Wrench &, double)` was there all along and reachable. What was
// missing was a body it could be called FOR -- the shape this tree keeps producing, a complete
// capability no declaration reaches.
constexpr double kGravityMs2 = 9.80665;
constexpr double kStepS = 1.0 / 120.0;
constexpr int kSteps = 60;
constexpr double kStartUpM = 500.0;

constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

[[nodiscard]] std::string Minimal(void) {
  return std::string(
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
      "\"material\":0}]}],"
      "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
      "\"min\":[0,0,0],\"max\":[1,1,0]},"
      "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
      "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
      "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,") +
      kTriangleBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.Numbers()) {
    if (held.What == what) { return held.How; }
  }
  return 0.0;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so the engine cannot stand a picture to advance beside");
    return Report();
  }

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its crate into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/crate.gltf", Minimal())) {
    Unprepared("the crate's mesh could not be written into the nest");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{64, 64})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{64, 64};
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Ground.Declared = true;
  stands.Ground.GravityMs2 = kGravityMs2;
  stands.Motion.StepS = kStepS;

  outshine::Asset shown;
  shown.Uri = "crate.gltf";
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);

  outshine::Body crate;
  crate.Name = "crate";
  crate.Asset = "crate.gltf";
  crate.MassKg = 40.0;
  crate.InertiaKgM2[0] = 2.0;
  crate.InertiaKgM2[1] = 2.0;
  crate.InertiaKgM2[2] = 2.0;
  crate.Placed = true;
  crate.AtM[1] = kStartUpM;
  stands.Bodies.push_back(crate);

  if (!engine.Declare(stands) || !engine.Assemble()) {
    Unprepared(("the crate did not stand: " + engine.Error()).c_str());
    return Report();
  }
  for (int step = 0; step < kSteps; ++step) {
    if (!engine.Advance()) { break; }
  }

  const double standing = Measured(engine, "bodies standing on no route");
  const double upM = Measured(engine, "the first of them, up");
  const double fallingMs = Measured(engine, "and how fast it falls");

  const double afterS = kStepS * (double)kSteps;
  const double owedFallM = 0.5 * kGravityMs2 * afterS * afterS;
  const double owedMs = kGravityMs2 * afterS;

  std::printf("A CRATE, NO ROUTE, NO DRIVE   bodies standing %.0f\n", standing);
  std::printf("AFTER %5.3f s                 up %10.5f m, falling %8.5f m/s\n", afterS, upM,
              fallingMs);
  std::printf("FREE FALL OWES                up %10.5f m, falling %8.5f m/s\n",
              kStartUpM - owedFallM, owedMs);

  CHECK(standing == 1.0,
        "**A BODY WITHOUT A JOURNEY STANDS**: a crate declares a place and no route, and the "
        "engine holds it. Until this landed a body's first position came only from a drive's "
        "start, so `Routes` returned early on an undeclared journey and nothing without one "
        "existed -- a car's assumption wearing an engine's clothes");
  CHECK(std::fabs(fallingMs + owedMs) < 1.0e-9,
        "and it falls at `g t`, which is the closed form and not our arithmetic repeated: 60 "
        "steps of 1/120 s under 9.80665 m/s^2 owe -4.90333 m/s, and the sign says it falls rather "
        "than rises");
  // THE DISCREPANCY IS NOT SLACK, IT IS THE INTEGRATOR'S OWN FIRST-ORDER TERM, so it is asserted
  // rather than tolerated -- and it was asserted with the sign the wrong way round first, which the
  // measurement corrected.
  //
  // A symplectic Euler step raises the velocity by the whole `g dt` and THEN moves at that new
  // velocity, so after n steps it has travelled `g dt^2 n(n+1)/2` where the continuous solution has
  // travelled `g (n dt)^2 / 2`. The difference is `g dt t / 2` DOWNWARD: the scheme falls further,
  // not less far, because every step moves at the end-of-step speed. 0.5 * 9.80665 * (1/120) * 0.5
  // = 0.020430 m after half a second, and the measurement reads -0.020431 m.
  //
  // A closed form and a discrete scheme disagreeing by exactly the scheme's known term is a
  // stronger reading than either alone. A loose bound here would have passed either sign and hidden
  // an integrator that changed underneath.
  const double owedLag = -0.5 * kGravityMs2 * kStepS * afterS;
  std::printf("THE SCHEME LAGS BY           %10.6f m, and it measures %10.6f m\n", owedLag,
              upM - (kStartUpM - owedFallM));
  CHECK(std::fabs((upM - (kStartUpM - owedFallM)) - owedLag) < 1.0e-6,
        "and it has fallen `g t^2 / 2` from where it was DECLARED to stand, short by exactly the "
        "half-step a symplectic Euler scheme lags by -- so the placement in the declaration is "
        "the one integration started from, and the only difference from the closed form is the "
        "scheme's own");

  const double meshUpM = Measured(engine, "the mesh it carries, up");
  std::printf("AND THE PICTURE CARRIES IT AT up %10.5f m\n", meshUpM);
  CHECK(std::fabs(meshUpM - upM) < 1.0e-9,
        "**AND IT REACHES THE PICTURE**: the mesh the frame carries stands where the integrated "
        "body stands. A body reaches the picture through the SUBJECT it names -- `Body::Asset` "
        "was always that link and nothing walked it, so a body with no route was integrated and "
        "never placed, which is a state no renderer can tell from a body that is not there");

  Covers("the sim: a body declares where it stands, and one with no route at all is held, "
         "integrated and placed in the picture through the asset it names");
  return Report();
}
