#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <system_error>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "CurlTransport.h"
#include "Heap.h"
#include "Journey.h"
#include "Live.h"
#include "PreparedRoot.h"
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
constexpr double kShownM = 400.0;
constexpr double kAheadM = 600.0;
constexpr double kRibbonStepM = 2.0;
constexpr int kStills = 12;
constexpr uint64_t kSeed = 0x5EEDu;

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

void Shifted(const double byM[3], double out[16]) {
  for (int at = 0; at < 16; ++at) { out[at] = (at % 5) == 0 ? 1.0 : 0.0; }
  for (int axis = 0; axis < 3; ++axis) { out[12 + axis] = byM[axis]; }
}

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
    for (int axis = 0; axis < 3; ++axis) { out.EyeM[axis] -= ahead[axis] * view.DistanceM; }
  }
  for (int axis = 0; axis < 3; ++axis) {
    out.Forward[axis] = ahead[axis];
    out.Right[axis] = right[axis];
    out.Up[axis] = up[axis];
  }
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Quiet quiet;
  Journey journey;
  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  const Between between{kMarienplatzLat, kMarienplatzLon, kRathausmarktLat, kRathausmarktLon};
  const bool laid = journey.Lay(between, "tools/driver/f31.scenario", kZoom, wire, quiet);
  CHECK(laid, "the road is laid, exactly as the drive lays it");
  if (!laid) { return Report(); }

  const std::string carPath = PreparedRoot() + "/tools-driver-f31/scene.gltf";
  if (std::FILE *const probe = std::fopen(carPath.c_str(), "rb")) {
    std::fclose(probe);
  } else {
    Unprepared(("the declared F31 is not at " + carPath +
                " -- prepare.py scenario-assets places it").c_str());
    return Report();
  }

  const double routeM = journey.LengthM();
  Note("the route the stills are taken along", routeM / 1000.0, "km");

  std::vector<double> atM;
  {
    uint64_t state = kSeed;
    for (int one = 0; one < kStills; ++one) {
      state = state * 6364136223846793005ull + 1442695040888963407ull;
      const double share = (double)((state >> 11) & 0xFFFFFFFFull) / (double)0x100000000ull;
      atM.push_back(0.002 * routeM + share * 0.99 * routeM);
    }
    std::sort(atM.begin(), atM.end());
  }
  Note("stills to take", (double)atM.size(), "stills");
  Note("the seed that chose where", (double)kSeed, "");

  outshine::Section section;
  section.HalfWidthM = 3.75;
  section.ShoulderM = 2.5;
  section.ThicknessM = 0.35;
  double laidToM = kShownM;
  double roadAt[16];
  outshine::Ribbon ribbon = Sweep(journey.Corridor(), section, 0.0, laidToM, kRibbonStepM);
  CHECK(ribbon.Woven, "and its first run sweeps into a solid");
  double originM[3] = {ribbon.OriginM[0], ribbon.OriginM[1], ribbon.OriginM[2]};
  Shifted(originM, roadAt);
  for (int at = 0; at < 16; ++at) { roadAt[at] = (at % 5) == 0 ? 1.0 : 0.0; }
  if (!ribbon.Woven) { return Report(); }

  outshine::Gltf::Piece piece;
  piece.NodeName = "corridor";
  piece.PositionsM = outshine::Span<const float>(ribbon.PositionM.data(), ribbon.PositionM.size());
  piece.Normals = outshine::Span<const float>(ribbon.NormalM.data(), ribbon.NormalM.size());
  piece.Indices = outshine::Span<const uint32_t>(ribbon.Index.data(), ribbon.Index.size());
  outshine::Gltf::Subject road;
  CHECK(road.Assemble(outshine::Gltf::Assembly{
            outshine::Span<const outshine::Gltf::Piece>(&piece, 1)}),
        "and becomes geometry without passing through a file");

  outshine::Render::Renderer renderer;
  outshine::Clients::Declaration declaration;
  declaration.SurfaceWidthPx = kWidePx;
  declaration.SurfaceHeightPx = kHighPx;
  declaration.Fps = 60.0;
  declaration.Stands = carPath;
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
  CHECK(stood, "the picture stands up with the car and the road in it");
  if (!stood) { return Report(); }
  Note("parts the picture carries", (double)standing->Shown().Parts().size(), "parts");
  Note("triangles", (double)standing->Shown().TriangleCount(), "triangles");

  const std::string into =
      (std::filesystem::temp_directory_path() / "outshine-stills").string();
  std::error_code made;
  std::filesystem::create_directories(into, made);
  std::printf("STILLS INTO %s\n", into.c_str());

  {
    if (!standing->Advance(error)) { std::printf("REFUSED %s\n", error.c_str()); }
    const std::string framed = into + "/000-framed-by-the-engine.png";
    if (standing->Screenshot(framed, error)) {
      std::printf("STILL %s\n", framed.c_str());
    } else {
      std::printf("REFUSED %s\n", error.c_str());
    }
  }

  Ridden rode;
  size_t next = 0;
  long wrote = 0;
  for (long frame = 0; frame < 40000000L && next < atM.size(); ++frame) {
    for (long step = 0; step < 17; ++step) {
      rode = journey.Ride(kStepS, nullptr);
      if (!rode.Found || rode.Arrived || rode.Lost) { break; }
    }
    if (!rode.Found || rode.Lost) { break; }
    if (rode.ReachedM + kAheadM > laidToM) {
      const double fromM = laidToM;
      laidToM += kShownM;
      const outshine::Ribbon nextRun =
          Sweep(journey.Corridor(), section, fromM, laidToM, kRibbonStepM);
      if (nextRun.Woven) {
        piece.PositionsM =
            outshine::Span<const float>(nextRun.PositionM.data(), nextRun.PositionM.size());
        piece.Normals = outshine::Span<const float>(nextRun.NormalM.data(), nextRun.NormalM.size());
        piece.Indices = outshine::Span<const uint32_t>(nextRun.Index.data(), nextRun.Index.size());
        outshine::Gltf::Subject ahead;
        if (ahead.Assemble(outshine::Gltf::Assembly{
                outshine::Span<const outshine::Gltf::Piece>(&piece, 1)})) {
          for (int axis = 0; axis < 3; ++axis) { originM[axis] = nextRun.OriginM[axis]; }
          (void)standing->Restand(ahead, error);
        }
      }
    }
    if (rode.ReachedM < atM[next]) {
      if (rode.Arrived) { break; }
      continue;
    }

    if (next == 0) {
      const outshine::Gltf::Subject &shown = standing->Shown();
      std::printf("BOUNDS min %.1f %.1f %.1f  max %.1f %.1f %.1f\n", shown.MinM()[0],
                  shown.MinM()[1], shown.MinM()[2], shown.MaxM()[0], shown.MaxM()[1],
                  shown.MaxM()[2]);
      const outshine::Physics::Body &car = journey.Carried();
      std::printf("BODY at %.1f %.1f %.1f\n", car.PositionM[0], car.PositionM[1],
                  car.PositionM[2]);
    }
    double body[16];
    Standing(journey.Carried(), body);
    for (int axis = 0; axis < 3; ++axis) { body[12 + axis] -= originM[axis]; }
    for (int person = 0; person < 2; ++person) {
      if (!standing->Carry(body, roadAt, error)) { break; }
      outshine::Gltf::Placement where = Seen(journey.Carried(), journey.Declared().Views[person]);
      for (int axis = 0; axis < 3; ++axis) { where.EyeM[axis] -= originM[axis]; }
      standing->Eye(where);
      if (!standing->Advance(error)) { break; }
      char name[256];
      std::snprintf(name, sizeof name, "%s/km%06.1f-%s.png", into.c_str(),
                    rode.ReachedM / 1000.0, person == 0 ? "first" : "third");
      if (standing->Screenshot(name, error)) {
        ++wrote;
        std::printf("STILL %s\n", name);
      } else {
        std::printf("REFUSED %s\n", error.c_str());
      }
    }
    {
      standing->FrameItself();
      if (standing->Advance(error)) {
        char framed[256];
        std::snprintf(framed, sizeof framed, "%s/km%06.1f-framed.png", into.c_str(),
                      rode.ReachedM / 1000.0);
        if (!standing->Screenshot(framed, error)) { std::printf("REFUSED %s\n", error.c_str()); }
      }
    }
    ++next;
    if (rode.Arrived) { break; }
  }

  Note("stills written", (double)wrote, "files");
  Note("how far the drive got", rode.ReachedM / 1000.0, "km");
  CHECK(wrote >= 2 * (long)atM.size() - 2,
        "**AND A STILL IS TAKEN AT EACH SAMPLED STATION, IN BOTH PERSONS.** The stations are drawn "
        "from one declared seed, so the sample is random and REPRODUCIBLE -- a different reviewer "
        "gets the same twelve places");

  Covers("I.4.7 the drive can be looked at: stills at reproducibly-sampled stations along the route, "
         "in first and third person, written where a person can open them");
  return Report();
}
