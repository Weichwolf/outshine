#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Journey.h"
#include "Live.h"
#include "Renderer.h"
#include "Ribbon.h"

using outshine::Driver::Between;
using outshine::Driver::Journey;
using outshine::Driver::Ridden;
using outshine::Driver::Sink;

namespace {

constexpr double kMarienplatzLat = 48.1371;
constexpr double kMarienplatzLon = 11.5754;
constexpr double kRathausmarktLat = 53.5503;
constexpr double kRathausmarktLon = 9.9920;
constexpr int kZoom = 10;
constexpr double kStepS = 1.0e-3;
constexpr int kWidePx = 1280;
constexpr int kHighPx = 720;
constexpr double kFps = 60.0;
constexpr double kShownM = 400.0;
constexpr double kRibbonStepM = 2.0;
constexpr int kFrames = 120;

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Quiet quiet;
  Journey journey;
  const Between between{kMarienplatzLat, kMarienplatzLon, kRathausmarktLat, kRathausmarktLon};
  const bool laid = journey.Lay(between, "tools/driver/f31.scenario", kZoom, quiet);
  CHECK(laid, "**THE WINDOW LAYS THE ROAD WITH THE SAME CALL THE HEADLESS DRIVER DOES.** One "
              "translation unit, two binaries: the headless one links no renderer at all and this "
              "one links the whole of it, and neither knows which it is");
  if (!laid) { return Report(); }

  outshine::Section section;
  section.HalfWidthM = 3.75;
  section.ShoulderM = 2.5;
  section.ThicknessM = 0.35;
  const outshine::Ribbon ribbon =
      Sweep(journey.Corridor(), section, 0.0, kShownM, kRibbonStepM);
  if (!ribbon.Woven) { std::printf("REFUSED %s\n", ribbon.Error.c_str()); }
  CHECK(ribbon.Woven, "and the first 400 m of it sweeps into a solid");
  if (!ribbon.Woven) { return Report(); }

  Note("triangles in the road that is shown", (double)ribbon.Triangles, "triangles");
  Note("vertices", (double)ribbon.Vertices, "vertices");

  outshine::Gltf::Piece piece;
  piece.NodeName = "corridor";
  piece.PositionsM = outshine::Span<const float>(ribbon.PositionM.data(), ribbon.PositionM.size());
  piece.Normals = outshine::Span<const float>(ribbon.NormalM.data(), ribbon.NormalM.size());
  piece.Indices = outshine::Span<const uint32_t>(ribbon.Index.data(), ribbon.Index.size());
  const outshine::Gltf::Assembly assembly{outshine::Span<const outshine::Gltf::Piece>(&piece, 1)};

  outshine::Gltf::Subject road;
  const bool assembled = road.Assemble(assembly);
  if (!assembled) { std::printf("REFUSED %s\n", road.Error().c_str()); }
  CHECK(assembled,
        "**AND IT BECOMES GEOMETRY WITHOUT PASSING THROUGH A FILE.** Ribbon's floats are what "
        "Gltf::Piece wants, and Subject::Assemble takes them as they stand -- no GLB written, none "
        "read back, board:1535");
  if (!assembled) { return Report(); }
  Note("parts the road assembles into", (double)road.Parts().size(), "parts");
  Note("triangles the subject carries", (double)road.TriangleCount(), "triangles");

  outshine::Render::Renderer renderer;
  outshine::Clients::Declaration declaration;
  declaration.SurfaceWidthPx = kWidePx;
  declaration.SurfaceHeightPx = kHighPx;
  declaration.Fps = kFps;
  declaration.Built = &road;
  declaration.Surface.BaseColour[0] = 0.14f;
  declaration.Surface.BaseColour[1] = 0.14f;
  declaration.Surface.BaseColour[2] = 0.15f;
  declaration.Surface.Roughness = 0.92f;
  declaration.Surface.Metalness = 0.0f;
  declaration.KeyLux = 40000.0;
  declaration.KeyElevationDeg = 42.0;
  declaration.KeyBearingDeg = 150.0;
  declaration.Environment[0] = 0.06;
  declaration.Environment[1] = 0.07;
  declaration.Environment[2] = 0.09;

  std::unique_ptr<outshine::Clients::Live> standing;
  std::string error;
  const bool stood =
      outshine::Clients::Live::Open(renderer, std::move(declaration), nullptr, standing, error);
  if (!stood) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(stood, "**AND THE RENDERER STANDS THE GENERATED ROAD UP AT 1280 BY 720.** Live took a "
               "SUBJECT and a declared surface rather than a filename, which is the door "
               "board:1535 says a generator and a file must share");
  if (!stood) { return Report(); }

  const long perFrame = (long)(1.0 / kFps / kStepS + 0.5);
  Note("physics steps a frame", (double)perFrame, "steps");

  double worstMs = 0.0, totalMs = 0.0;
  long drawn = 0;
  Ridden rode;
  for (int frame = 0; frame < kFrames; ++frame) {
    for (long step = 0; step < perFrame; ++step) {
      rode = journey.Ride(kStepS);
      if (!rode.Found || rode.Arrived || rode.Lost) { break; }
    }
    const outshine::Physics::Body &body = journey.Carried();
    outshine::Gltf::Placement eye;
    const double aheadBody[3] = {0.0, 0.0, -1.0};
    double ahead[3];
    outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
    for (int axis = 0; axis < 3; ++axis) {
      eye.EyeM[axis] = body.PositionM[axis] - ahead[axis] * 8.0;
      eye.Forward[axis] = ahead[axis];
    }
    eye.EyeM[1] += 2.5;
    eye.Up[0] = 0.0;
    eye.Up[1] = 1.0;
    eye.Up[2] = 0.0;
    eye.Right[0] = ahead[2];
    eye.Right[1] = 0.0;
    eye.Right[2] = -ahead[0];
    standing->Eye(eye);

    const auto began = std::chrono::steady_clock::now();
    if (!standing->Advance(error)) {
      std::printf("REFUSED %s\n", error.c_str());
      break;
    }
    const double ms =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() * 1000.0;
    if (frame > 0) {
      worstMs = ms > worstMs ? ms : worstMs;
      totalMs += ms;
      ++drawn;
    }
  }

  Note("frames drawn", (double)drawn, "frames");
  Note("how far the car got while they were drawn", rode.ReachedM, "m");
  Note("the mean frame", drawn > 0 ? totalMs / (double)drawn : 0.0, "ms");
  Note("the worst frame", worstMs, "ms");
  Note("the budget at 60 Hz", 1000.0 / kFps, "ms");

  CHECK(drawn >= kFrames - 1, "every frame of the run was drawn");
  CHECK(rode.ReachedM > 0.0,
        "**AND THE CAR MOVED WHILE THEY WERE.** The same Ride that carried it 774 km headless is "
        "stepping here, sixteen times a frame, with the camera behind it -- one code path, two "
        "modes");
  CHECK(worstMs < 1000.0 / kFps,
        "and the worst frame is inside the 16.67 ms budget, which is what makes this a test of the "
        "engine rather than a viewer");

  journey.Close();

  Covers("I.4.6 the road the car drives is the road the renderer draws: the corridor sweeps into a "
         "solid, becomes a Subject without passing through a file, and stands up at 1280 by 720 "
         "while the shared Ride steps the same physics the headless driver runs");
  return Report();
}
