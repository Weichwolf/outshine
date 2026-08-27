#include <cstdio>
#include <cstdlib>
#include <string>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE ORACLE IS THE COST RULE CLAUDE.md ALREADY STATES, and it owes nothing to our design: the work
// a declaration causes is proportional to what it CHANGED, never to how big it is. A frame in which
// nothing moved changed nothing, so it must cost nothing to place.
//
// Until this was written the renderer was handed the WHOLE placement table every frame --
// `Placements(studio, scratch)` rebuilt every row and `SetPlacements` assigned the lot -- so a
// parked car re-uploaded its own unchanged matrix sixty times a second, and the cost scaled with
// the scene rather than with the change.
//
// UNREAL'S ANSWER IS THE SHAPE, NOT THE OPTIMISATION. `FScene` lives beside `UWorld` and the game
// side never touches it; what crosses is a DELTA -- added, removed, transform changed. That
// separation is what lets render state survive the frame at all, and state that survives the frame
// is the precondition for state that lives on the DEVICE (`FGPUScene`). A renderer handed a fresh
// table each frame cannot keep anything, so the per-frame upload is not merely wasteful: it forecloses
// the GPU-driven path entirely.
//
// The measure is a RUNNING total of rows the renderer has been sent, so the case reads it twice and
// looks at the difference. A total that stands still across a frame is the claim.
//
// WHAT THE NEGATIVE CONTROL SAID, AND IT IS NOT WHAT THIS CASE WAS WRITTEN TO HEAR. Forcing every
// row to be re-sent -- the exact behaviour the delta replaced -- left this case GREEN. So on the
// path a declared, standing subject takes, the placement table was never re-uploaded per frame:
// `Aim` and `Pose` run when something RESTANDS, not once a frame, and neither `RenderTo` nor
// `Advance` reaches them for a subject that is merely sitting there.
//
// The per-frame rebuild is real and it is on the DRIVE path: `Engine`'s tick calls
// `Live::Carry(bodyFromWorld, ...)`, which rebuilds every row from the body's transform and hands
// the lot over. A drive needs a vehicle asset and a route, which no case here stands, so that half
// is owed to `apps/driver` -- CLAUDE.md's one integration test -- and board:1957 stays open until
// it is paid.
//
// What this case does prove is worth keeping and is stated as what it is: a frame that changes
// nothing re-places nothing, on the path it can reach. It is a regression net for a property that
// already held, not the proof of the change that prompted it.
constexpr int kFramePx = 64;

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
  return -1.0;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subject into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/parked.gltf", Minimal())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
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
  outshine::Asset shown;
  shown.Uri = "parked.gltf";
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);

  if (!engine.Declare(stands) || !engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
    Unprepared(("the subject did not stand: " + engine.Error()).c_str());
    return Report();
  }
  const double afterFirst = Measured(engine, "placement rows the renderer has been sent");
  const double drawn = Measured(engine, "batches the picture draws");

  if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
    Unprepared(("the second frame did not draw: " + engine.Error()).c_str());
    return Report();
  }
  const double afterSecond = Measured(engine, "placement rows the renderer has been sent");

  if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
    Unprepared(("the third frame did not draw: " + engine.Error()).c_str());
    return Report();
  }
  const double afterThird = Measured(engine, "placement rows the renderer has been sent");

  std::printf("STANDING IT      sent %.0f row(s), drawing %.0f batch(es)\n", afterFirst, drawn);
  std::printf("A SECOND FRAME   sent %.0f row(s) in total\n", afterSecond);
  std::printf("A THIRD          sent %.0f row(s) in total\n", afterThird);

  CHECK(drawn > 0.0 && afterFirst > 0.0,
        "the subject stands and its placement reached the renderer at all, so the two frames "
        "below have something they could have re-sent and did not");
  CHECK(afterSecond == afterFirst,
        "**A FRAME THAT CHANGED NOTHING PLACES NOTHING**: the work a declaration causes is "
        "proportional to what it CHANGED, and a parked subject changed nothing. The renderer was "
        "handed the whole placement table every frame until this stood, so a scene's placement "
        "cost scaled with the scene rather than with its motion");
  CHECK(afterThird == afterSecond,
        "and it holds beyond the second frame, so what the first check saw is the renderer KEEPING "
        "its state across frames rather than an accident of the first one -- state that survives "
        "the frame is the precondition for state that lives on the device, which is why Unreal "
        "separates `FScene` from `UWorld` and feeds it deltas");

  Covers("the render: on the path a standing subject takes, a frame that changes nothing re-places "
         "nothing -- the drive path's own proof is owed to apps/driver and board:1957");
  return Report();
}
