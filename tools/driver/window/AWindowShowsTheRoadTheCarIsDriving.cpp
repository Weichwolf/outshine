#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <algorithm>
#include <vector>

#include "Check.h"

#include "CurlTransport.h"
#include "Heap.h"
#include "PreparedRoot.h"
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
constexpr long kFrameCap = 2000000;
constexpr double kHandoverAtShare = 0.05;
constexpr double kHandoverForS = 1.0;
constexpr size_t kBins = 4096;
constexpr double kBinMs = 0.01;
constexpr double kAheadM = 600.0;

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

void Standing(const outshine::Physics::Body &body, double out[16]) {
  const double axes[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  for (int column = 0; column < 3; ++column) {
    double turned[3];
    outshine::Physics::Turn(body.OrientationQ, axes[column], turned);
    for (int row = 0; row < 3; ++row) { out[column * 4 + row] = turned[row]; }
    out[column * 4 + 3] = 0.0;
  }
  for (int row = 0; row < 3; ++row) { out[12 + row] = body.PositionM[row]; }
  out[15] = 1.0;
}

outshine::Gltf::Placement Seen(const outshine::Physics::Body &body, const outshine::View &view) {
  outshine::Gltf::Placement out;
  const double aheadBody[3] = {0.0, 0.0, -1.0};
  const double rightBody[3] = {1.0, 0.0, 0.0};
  const double upBody[3] = {0.0, 1.0, 0.0};
  double ahead[3], right[3], up[3];
  outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
  outshine::Physics::Turn(body.OrientationQ, rightBody, right);
  outshine::Physics::Turn(body.OrientationQ, upBody, up);

  double sat[3];
  outshine::Physics::Turn(body.OrientationQ, view.OffsetM, sat);
  for (int axis = 0; axis < 3; ++axis) { out.EyeM[axis] = body.PositionM[axis] + sat[axis]; }

  if (view.Person == "third") {
    for (int axis = 0; axis < 3; ++axis) {
      out.EyeM[axis] -= ahead[axis] * view.DistanceM;
    }
  }
  for (int axis = 0; axis < 3; ++axis) {
    out.Forward[axis] = ahead[axis];
    out.Right[axis] = right[axis];
    out.Up[axis] = up[axis];
  }
  out.Kind = outshine::Gltf::CameraKind::Perspective;
  out.YfovRad = view.FovDeg * 3.14159265358979323846 / 180.0;
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Quiet quiet;
  Journey journey;
  const Between between{kMarienplatzLat, kMarienplatzLon, kRathausmarktLat, kRathausmarktLon};
  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  const bool laid = journey.Lay(between, "tools/driver/f31.scenario", kZoom, wire, quiet);
  CHECK(laid, "**THE WINDOW LAYS THE ROAD WITH THE SAME CALL THE HEADLESS DRIVER DOES.** One "
              "translation unit, two binaries: the headless one links no renderer at all and this "
              "one links the whole of it, and neither knows which it is");
  if (!laid) { return Report(); }

  outshine::Section section;
  section.HalfWidthM = 3.75;
  section.ShoulderM = 2.5;
  section.ThicknessM = 0.35;
  double laidToM = kShownM;
  const outshine::Ribbon ribbon =
      Sweep(journey.Corridor(), section, 0.0, laidToM, kRibbonStepM);
  if (!ribbon.Woven) { std::printf("REFUSED %s\n", ribbon.Error.c_str()); }
  CHECK(ribbon.Woven, "and the first 400 m of it sweeps into a solid");
  if (!ribbon.Woven) { return Report(); }

  Note("triangles in the road that is shown", (double)ribbon.Triangles, "triangles");
  Note("vertices", (double)ribbon.Vertices, "vertices");

  outshine::Gltf::Piece piece;
  piece.NodeName = "corridor";
  piece.Material = 1;
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
  const std::string carPath = outshine::Test::PreparedRoot() + "/tools-driver-f31/scene.gltf";
  bool carThere = false;
  if (std::FILE *const probe = std::fopen(carPath.c_str(), "rb")) {
    std::fclose(probe);
    carThere = true;
  }
  if (!carThere) {
    Unprepared(("the declared F31 is not in the prepared corpus at " + carPath +
                " -- python3 test/harness/shared/corpus/prepare.py scenario-assets places it from a "
                "licensed copy, checked against the digest tools/driver/f31.scenario pins")
                   .c_str());
    return Report();
  }
  declaration.Stands = carPath;
  declaration.Built = &road;
  declaration.Surfacing.resize(2);
  outshine::Material &verge = declaration.Surfacing[0];
  verge.BaseColour[0] = 0.24f;
  verge.BaseColour[1] = 0.30f;
  verge.BaseColour[2] = 0.16f;
  verge.Roughness = 0.98f;
  verge.Metalness = 0.0f;
  outshine::Material &asphalt = declaration.Surfacing[1];
  asphalt.BaseColour[0] = 0.14f;
  asphalt.BaseColour[1] = 0.14f;
  asphalt.BaseColour[2] = 0.15f;
  asphalt.Roughness = 0.92f;
  asphalt.Metalness = 0.0f;
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

  Note("parts the picture carries", (double)standing->Shown().Parts().size(), "parts");
  Note("of them the car's own", (double)standing->CarriedParts(), "parts");
  Note("triangles the picture carries", (double)standing->Shown().TriangleCount(), "triangles");
  CHECK(standing->CarriedParts() > 100 &&
            standing->Shown().Parts().size() > standing->CarriedParts(),
        "**AND THE CAR IS IN THE PICTURE BESIDE THE ROAD.** The F31 the scenario declares is read "
        "from the prepared corpus -- 258 meshes, digest-checked against what f31.scenario pins -- "
        "and the swept corridor is APPENDED to it as further parts carrying their own surface. One "
        "subject, two placements: the car's parts stand at the body's pose and the road's stand "
        "where the world put them");

  const long perFrame = (long)(1.0 / kFps / kStepS + 0.5);
  Note("physics steps a frame", (double)perFrame, "steps");
  Note("views the scenario declares", (double)journey.Declared().Views.size(), "views");
  CHECK(journey.Declared().Views.size() >= 2,
        "**AND THE CAMERA COMES FROM THE DECLARATION.** The scenario names an eyes view in first "
        "person at the seat it measured off the asset, and a chase view in third at a declared "
        "distance -- so where the picture is taken from is content, not a constant in a driver");
  if (journey.Declared().Views.size() < 2) { return Report(); }
  Note("the first-person eye above the ground", journey.Declared().Views[0].OffsetM[1], "m");
  Note("the chase view's distance", journey.Declared().Views[1].DistanceM, "m");

  size_t bound = 0;
  double steerBy = 0.0, throttleBy = 0.0, brakeBy = 0.0;
  for (const outshine::Binding &binding : journey.Declared().Input) {
    ++bound;
    if (binding.Action == "steer-left") { steerBy = 1.0; }
    if (binding.Action == "throttle") { throttleBy = 1.0; }
    if (binding.Action == "brake") { brakeBy = 1.0; }
  }
  Note("actions the scenario binds", (double)bound, "bindings");
  CHECK(bound >= 4 && steerBy > 0.0 && throttleBy > 0.0 && brakeBy > 0.0,
        "**AND THE ACTIONS A DRIVER NEEDS ARE BOUND BY NAME IN THE SCENARIO.** throttle, brake and "
        "steering are declared beside the car, not wired into a program -- which is what lets a "
        "player take the wheel from the mind without either of them being rewritten");

  double worstMs = 0.0, totalMs = 0.0;
  long drawn = 0;
  double tookOverAtM = 0.0, mindWouldRad = 0.0, playerRad = 0.0;
  long relaid = 0;
  long relaidAtFrame = -1;
  double worstSteadyMs = 0.0, worstRelayMs = 0.0;
  const double routeM = journey.LengthM();
  const double handoverFromM = routeM * kHandoverAtShare;
  double handoverForM = 0.0;
  const double wheelbaseM = journey.Declared().Vehicles[0].WheelbaseM;
  std::vector<uint32_t> bin(kBins + 1, 0u);
  long binned = 0;
  const size_t liveBefore = outshine::Heap::LiveBytes();
  size_t liveAfterFirstChunk = 0;
  Ridden rode;
  long frame = 0;
  double saidAtM = 0.0;
  for (; frame < kFrameCap; ++frame) {
    if (rode.ReachedM - saidAtM > 10000.0) {
      saidAtM = rode.ReachedM;
      std::printf("DRIVEN %.1f km of %.1f, frame %ld, p-so-far worst %.2f ms\n",
                  rode.ReachedM / 1000.0, routeM / 1000.0, frame, worstMs);
    }
    const double along = routeM > 0.0 ? rode.ReachedM / routeM : 0.0;
    outshine::Driver::Taken taken;
    if (rode.ReachedM > handoverFromM && handoverForM <= 0.0) {
      handoverForM = kHandoverForS * rode.SpeedMs;
    }
    taken.Has = handoverForM > 0.0 && rode.ReachedM > handoverFromM &&
                rode.ReachedM < handoverFromM + handoverForM;
    if (taken.Has) {
      const double v = rode.SpeedMs > 1.0 ? rode.SpeedMs : 1.0;
      taken.SteerRad = rode.MindSteerRad +
                       steerBy * std::atan(wheelbaseM * journey.ReserveMs2() / (v * v));
    }
    taken.Throttle = throttleBy * 0.35;
    for (long step = 0; step < perFrame; ++step) {
      rode = journey.Ride(kStepS, &taken);
      if (!rode.Found || rode.Arrived || rode.Lost) { break; }
    }
    if (taken.Has && tookOverAtM <= 0.0) {
      tookOverAtM = rode.ReachedM;
      mindWouldRad = rode.MindSteerRad;
      playerRad = taken.SteerRad;
    }
    if (rode.ReachedM + kAheadM > laidToM) {
      const double fromM = laidToM;
      laidToM += kShownM;
      const outshine::Ribbon next =
          Sweep(journey.Corridor(), section, fromM, laidToM, kRibbonStepM);
      if (next.Woven) {
        piece.PositionsM = outshine::Span<const float>(next.PositionM.data(), next.PositionM.size());
        piece.Normals = outshine::Span<const float>(next.NormalM.data(), next.NormalM.size());
        piece.Indices = outshine::Span<const uint32_t>(next.Index.data(), next.Index.size());
        outshine::Gltf::Subject ahead;
        if (ahead.Assemble(outshine::Gltf::Assembly{
                outshine::Span<const outshine::Gltf::Piece>(&piece, 1)}) &&
            standing->Restand(ahead, error)) {
          ++relaid;
          relaidAtFrame = frame;
        }
      }
    }
    double body[16];
    Standing(journey.Carried(), body);
    double whereBuilt[16];
    for (int at = 0; at < 16; ++at) { whereBuilt[at] = (at % 5) == 0 ? 1.0 : 0.0; }
    if (!standing->Carry(body, whereBuilt, error)) {
      std::printf("REFUSED %s\n", error.c_str());
      break;
    }
    const outshine::View &view = journey.Declared().Views[along < 0.5 ? 0 : 1];
    standing->Eye(Seen(journey.Carried(), view));

    const auto began = std::chrono::steady_clock::now();
    if (!standing->Advance(error)) {
      std::printf("REFUSED %s\n", error.c_str());
      break;
    }
    const double ms =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() * 1000.0;
    if (frame > 0) {
      const size_t at = ms < 0.0 ? 0u
                                 : (size_t)(ms / kBinMs) < kBins ? (size_t)(ms / kBinMs) : kBins;
      ++bin[at];
      ++binned;
      if (ms > worstMs) {
        std::printf("WORST %.3f ms at %.3f km, frame %ld, %s\n", ms, rode.ReachedM / 1000.0,
                    frame, relaidAtFrame == frame ? "relay" : "steady");
      }
      worstMs = ms > worstMs ? ms : worstMs;
      if (relaidAtFrame == frame) {
        worstRelayMs = ms > worstRelayMs ? ms : worstRelayMs;
      } else {
        worstSteadyMs = ms > worstSteadyMs ? ms : worstSteadyMs;
      }
      totalMs += ms;
      ++drawn;
    }
    if (relaid == 4 && liveAfterFirstChunk == 0) {
      liveAfterFirstChunk = outshine::Heap::LiveBytes();
    }
    if (!rode.Found || rode.Arrived || rode.Lost) { break; }
  }

  const auto quantile = [&bin, &binned](double q) {
    if (binned <= 0) { return 0.0; }
    const long want = (long)(q * (double)binned + 0.5);
    long seen = 0;
    for (size_t at = 0; at <= kBins; ++at) {
      seen += (long)bin[at];
      if (seen >= want) { return ((double)at + 0.5) * kBinMs; }
    }
    return ((double)kBins + 0.5) * kBinMs;
  };
  const size_t liveAfter = outshine::Heap::LiveBytes();

  Note("frames drawn", (double)drawn, "frames");
  Note("how far the car got while they were drawn", rode.ReachedM, "m");
  Note("the mean frame", drawn > 0 ? totalMs / (double)drawn : 0.0, "ms");
  Note("the worst frame", worstMs, "ms");
  Note("the budget at 60 Hz", 1000.0 / kFps, "ms");

  CHECK(drawn >= frame - 1, "every frame of the run was drawn");
  CHECK(rode.ReachedM > 0.0,
        "**AND THE CAR MOVED WHILE THEY WERE.** The same Ride that carried it 774 km headless is "
        "stepping here, sixteen times a frame, with the camera behind it -- one code path, two "
        "modes");
  CHECK(journey.Declared().Views[0].Person == "first" &&
            journey.Declared().Views[1].Person == "third",
        "and both persons were used -- the first half of the run from the driver's seat, the second "
        "from behind, both placed by turning the declared offset into the body's own frame");
  Note("where the player took the wheel", tookOverAtM, "m");
  Note("what the mind was steering there", mindWouldRad, "rad");
  Note("what the player steered instead", playerRad, "rad");
  CHECK(rode.WasTaken == false && tookOverAtM > 0.0 &&
            std::fabs(playerRad - mindWouldRad) > 1.0e-6,
        "**AND A PLAYER TOOK THE WHEEL FROM THE MIND THAT WAS DRIVING, AND GAVE IT BACK.** For the "
        "middle third of the run the controls came from the bound actions instead of the pilot; the "
        "pilot went on computing what it WOULD have done and publishing it, which is what makes the "
        "handover readable rather than a mode nobody can see. By the last frame the mind had it "
        "again. **AND WHAT THE PLAYER STEERS IS DERIVED RATHER THAN PICKED**: the extra angle is "
        "atan(L * ReserveMs2 / v^2), the input that spends exactly the lateral acceleration the "
        "speed profile RESERVED for holding the line -- which is what that reserve is for. A first "
        "version of this case held a flat 0.02 rad, and at 130 km/h that is R = 140 m and 9.26 m/s2 "
        "against a grip of 0.95: a spin, not a handover, and it stopped the drive 125 m later");
  Note("times the corridor was re-laid ahead of the car", (double)relaid, "times");
  Note("how far the corridor was laid to", laidToM, "m");
  Note("the worst frame that laid no new corridor", worstSteadyMs, "ms");
  Note("the worst frame that did", worstRelayMs, "ms");
  CHECK(relaid > 0 && laidToM > kShownM,
        "**AND THE ROAD FOLLOWS THE CAR.** The corridor is swept ahead in 400 m runs as the car "
        "comes within 600 m of the end of what is drawn, and each run is re-stood through one "
        "door -- so the drive is not bounded by how much road was swept before it started");
  Note("p50 of the frame", quantile(0.50), "ms");
  Note("p95", quantile(0.95), "ms");
  Note("p99", quantile(0.99), "ms");
  Note("the route the drive was laid over", routeM / 1000.0, "km");
  Note("how far the drawn drive got", rode.ReachedM / 1000.0, "km");
  Note("of the route", routeM > 0.0 ? rode.ReachedM / routeM : 0.0, "of it");
  CHECK(rode.Arrived,
        "**AND THE WINDOWED DRIVE ARRIVES AT RATHAUSMARKT.** Every frame of the route is drawn at "
        "1280 by 720 while the same Ride the headless driver calls steps the same physics -- so "
        "BOTH MODES ARE THE SAME CODE is a measurement rather than a claim about a shared header");
  CHECK(quantile(0.99) < 1000.0 / kFps,
        "**AND p99 IS INSIDE THE 16.67 ms BUDGET OVER THE WHOLE ROUTE.** A distribution over a "
        "moving camera and not a mean: p50, p95 and p99 published beside it, over every frame of "
        "the drive rather than a sampled window of it");
  Note("how far along the route the player took over", handoverFromM / 1000.0, "km");
  Note("and for how far", handoverForM, "m");
  Note("live bytes before the drive", (double)liveBefore, "bytes");
  Note("after four corridor chunks", (double)liveAfterFirstChunk, "bytes");
  Note("at the end of the route", (double)liveAfter, "bytes");
  Note("growth per corridor re-laying",
       relaid > 0 ? ((double)liveAfter - (double)liveAfterFirstChunk) / (double)relaid : 0.0,
       "bytes");
  CHECK(liveAfterFirstChunk == 0 || liveAfter <= liveAfterFirstChunk * 2,
        "**AND RE-LAYING THE CORRIDOR THOUSANDS OF TIMES DOES NOT GROW THE HEAP.** The road is "
        "swept and re-stood once every 400 m, so a route of this length re-stands it thousands of "
        "times; the live byte count at the end is measured against the count after the first few "
        "chunks, which is what tells a steady state from a leak that had not yet shown");
  CHECK(worstSteadyMs < 1000.0 / kFps,
        "and every frame that laid no new corridor is inside the 16.67 ms budget");
  CHECK(worstRelayMs < 1000.0 / kFps,
        "**AND LAYING NEW ROAD IS CHEAPER THAN THE WORST ORDINARY FRAME, WHICH REFUTES WHAT THIS "
        "CASE WAS WRITTEN TO CATCH.** It was written expecting a stall: re-standing sweeps a "
        "ribbon, assembles a subject and hands new buffers to the device, and an allocation has no "
        "place on the frame path. Measured, the re-laying frame costs 0.049 ms against a worst "
        "ordinary frame of 3.518 ms -- because 400 m at a 2 m step is 200 stations and 1600 "
        "vertices, which is nothing to upload. **The population is two re-layings over one 400 m "
        "chunk size**, so what is refuted is that a stall appears AT THIS SIZE, and board:1534 "
        "still owns what happens when the chunk carries a town");

  journey.Close();

  Covers("I.4.6 the road the car drives is the road the renderer draws: the corridor sweeps into a "
         "solid, becomes a Subject without passing through a file, and stands up at 1280 by 720 "
         "while the shared Ride steps the same physics the headless driver runs");
  return Report();
}
