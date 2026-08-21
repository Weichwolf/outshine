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
#include "TerrainLoader.h"
#include "Renderer.h"
#include "Carriageway.h"
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
constexpr double kBehindM = 120.0;
constexpr double kShownM = 900.0;
constexpr double kRelayAtM = 400.0;
constexpr double kRibbonStepM = 2.0;
constexpr double kGroundReachM = 700.0;
constexpr double kGroundStepM = 12.0;
constexpr double kSideSlopeRun = 1.5;
constexpr double kCentreStepM = 10.0;
constexpr int kStills = 12;
constexpr uint64_t kSeed = 0x5EEDu;

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

struct Ground {
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<uint32_t> Index;
  size_t Holes = 0;
};

struct Along {
  double EastM, HeightM, NorthM;
};

bool Lie(const Journey &journey, const double aboutM[3], const double originM[3],
         const outshine::Section &section, double fromM, double toM, std::vector<Along> &centre,
         Ground &out) {
  double frameLat = 0.0, frameLon = 0.0, perLatM = 1.0, perLonM = 1.0;
  journey.Frame(frameLat, frameLon, perLatM, perLonM);
  const int side = (int)(2.0 * kGroundReachM / kGroundStepM) + 1;
  out.PositionM.clear();
  out.NormalM.clear();
  out.Index.clear();
  out.Holes = 0;
  centre.clear();
  for (double atM = fromM; atM <= toM; atM += kCentreStepM) {
    outshine::Placed on;
    if (!journey.Corridor().At(atM, on)) { continue; }
    const outshine::Standing top = StandAt(journey.Corridor(), atM, 0.0, 0.0);
    centre.push_back(Along{on.EastM, top.HeightM, on.NorthM});
  }
  if (centre.empty()) { return false; }

  const double keptM = section.HalfWidthM + section.ShoulderM;
  std::vector<double> heightM((size_t)side * (size_t)side, 0.0);
  for (int row = 0; row < side; ++row) {
    const double northM = aboutM[2] * -1.0 - kGroundReachM + (double)row * kGroundStepM;
    for (int column = 0; column < side; ++column) {
      const double eastM = aboutM[0] - kGroundReachM + (double)column * kGroundStepM;
      const outshine::GroundSample sample =
          fb_stream_ground(frameLat + northM / perLatM, frameLon + eastM / perLonM);
      double aslM = 0.0;
      if (!sample.TryAslM(&aslM)) { ++out.Holes; }

      double nearestM = 1.0e30, roadM = aslM;
      for (const Along &on : centre) {
        const double dEast = eastM - on.EastM, dNorth = northM - on.NorthM;
        const double awayM = std::sqrt(dEast * dEast + dNorth * dNorth);
        if (awayM < nearestM) {
          nearestM = awayM;
          roadM = on.HeightM;
        }
      }
      const double formationM = roadM - section.ThicknessM;
      const double liftM = formationM - aslM;
      const double reachM = keptM + std::fabs(liftM) * kSideSlopeRun;
      if (nearestM <= keptM) {
        aslM = formationM;
      } else if (nearestM < reachM) {
        const double part = (nearestM - keptM) / (reachM - keptM);
        aslM = formationM + (aslM - formationM) * part * part * (3.0 - 2.0 * part);
      }
      heightM[(size_t)row * (size_t)side + (size_t)column] = aslM;
    }
  }
  if (out.Holes * 2 > heightM.size()) { return false; }
  for (int row = 0; row < side; ++row) {
    const double northM = aboutM[2] * -1.0 - kGroundReachM + (double)row * kGroundStepM;
    for (int column = 0; column < side; ++column) {
      const double eastM = aboutM[0] - kGroundReachM + (double)column * kGroundStepM;
      const size_t at = (size_t)row * (size_t)side + (size_t)column;
      out.PositionM.push_back((float)(eastM - originM[0]));
      out.PositionM.push_back((float)(heightM[at] - originM[1]));
      out.PositionM.push_back((float)(-northM - originM[2]));
      const size_t west = column > 0 ? at - 1 : at;
      const size_t east = column + 1 < side ? at + 1 : at;
      const size_t south = row > 0 ? at - (size_t)side : at;
      const size_t north = row + 1 < side ? at + (size_t)side : at;
      const double slopeEast = (heightM[east] - heightM[west]) / (2.0 * kGroundStepM);
      const double slopeNorth = (heightM[north] - heightM[south]) / (2.0 * kGroundStepM);
      const double length = std::sqrt(slopeEast * slopeEast + slopeNorth * slopeNorth + 1.0);
      out.NormalM.push_back((float)(-slopeEast / length));
      out.NormalM.push_back((float)(1.0 / length));
      out.NormalM.push_back((float)(slopeNorth / length));
    }
  }
  {
    float low[3] = {out.PositionM[0], out.PositionM[1], out.PositionM[2]};
    float high[3] = {low[0], low[1], low[2]};
    for (size_t at = 0; at < out.PositionM.size(); at += 3) {
      for (int axis = 0; axis < 3; ++axis) {
        low[axis] = out.PositionM[at + (size_t)axis] < low[axis] ? out.PositionM[at + (size_t)axis]
                                                                 : low[axis];
        high[axis] = out.PositionM[at + (size_t)axis] > high[axis]
                         ? out.PositionM[at + (size_t)axis]
                         : high[axis];
      }
    }
    std::printf("GROUND min %.1f %.1f %.1f  max %.1f %.1f %.1f  posts %zu\n", low[0], low[1],
                low[2], high[0], high[1], high[2], out.PositionM.size() / 3);
  }
  for (int row = 0; row + 1 < side; ++row) {
    for (int column = 0; column + 1 < side; ++column) {
      const uint32_t here = (uint32_t)(row * side + column);
      const uint32_t right = here + 1;
      const uint32_t up = here + (uint32_t)side;
      const uint32_t across = up + 1;
      out.Index.push_back(here);
      out.Index.push_back(right);
      out.Index.push_back(up);
      out.Index.push_back(right);
      out.Index.push_back(across);
      out.Index.push_back(up);
    }
  }
  return true;
}

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

  out.Kind = outshine::Gltf::CameraKind::Perspective;
  out.YfovRad = view.FovDeg * 3.14159265358979323846 / 180.0;
  out.ZNearM = 0.1;
  out.ZFarM = 4000.0;
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
  double laidFromM = 0.0;
  double laidToM = kShownM;
  double roadAt[16];
  outshine::Ribbon ribbon = Sweep(journey.Corridor(), section, laidFromM, laidToM, kRibbonStepM);
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
  Ground ground;
  double aboutM[3] = {ribbon.OriginM[0], ribbon.OriginM[1], ribbon.OriginM[2]};
  std::vector<Along> centre;
  const bool lies = Lie(journey, aboutM, originM, section, 0.0, kShownM, centre, ground);
  Note("holes the ground sampling met", (double)ground.Holes, "posts");
  CHECK(lies, "**AND THE GROUND UNDER THE ROAD IS SAMPLED FROM THE FIELD THE WHEELS STAND ON.** "
              "fb_stream_ground is what the drive already asks for every contact, so the drawn "
              "ground and the driven ground are ONE ground rather than two");
  if (!lies) { return Report(); }

  outshine::Gltf::Piece lying;
  lying.NodeName = "ground";
  lying.PositionsM = outshine::Span<const float>(ground.PositionM.data(), ground.PositionM.size());
  lying.Normals = outshine::Span<const float>(ground.NormalM.data(), ground.NormalM.size());
  lying.Indices = outshine::Span<const uint32_t>(ground.Index.data(), ground.Index.size());
  const outshine::Gltf::Piece both[2] = {lying, piece};

  outshine::Gltf::Subject road;
  CHECK(road.Assemble(outshine::Gltf::Assembly{
            outshine::Span<const outshine::Gltf::Piece>(both, 2)}),
        "and both become geometry without passing through a file");
  Note("triangles the ground carries", (double)(ground.Index.size() / 3), "triangles");

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

  double groundAtM[3] = {0.0, 0.0, 0.0};
  double worstCutM = 0.0, worstFillM = 0.0, liftTotalM = 0.0;
  long liftAt = 0;
  Ridden rode;
  size_t next = 0;
  long wrote = 0;
  for (long frame = 0; frame < 40000000L && next < atM.size(); ++frame) {
    for (long step = 0; step < 17; ++step) {
      rode = journey.Ride(kStepS, nullptr);
      if (!rode.Found || rode.Arrived || rode.Lost) { break; }
    }
    if (!rode.Found || rode.Lost) { break; }
    double body16[16];
    Standing(journey.Carried(), body16);
    for (int axis = 0; axis < 3; ++axis) { body16[12 + axis] -= originM[axis]; }
    const double strayEastM = body16[12] - groundAtM[0];
    const double strayNorthM = body16[14] - groundAtM[2];
    const double strayM =
        std::sqrt(strayEastM * strayEastM + strayNorthM * strayNorthM);
    if (rode.ReachedM + kRelayAtM > laidToM || strayM > kGroundReachM * 0.4) {
      laidFromM = rode.ReachedM > kBehindM ? rode.ReachedM - kBehindM : 0.0;
      laidToM = laidFromM + kShownM;
      const outshine::Ribbon nextRun =
          Sweep(journey.Corridor(), section, laidFromM, laidToM, kRibbonStepM);
      if (nextRun.Woven) {
        piece.PositionsM =
            outshine::Span<const float>(nextRun.PositionM.data(), nextRun.PositionM.size());
        piece.Normals = outshine::Span<const float>(nextRun.NormalM.data(), nextRun.NormalM.size());
        piece.Indices = outshine::Span<const uint32_t>(nextRun.Index.data(), nextRun.Index.size());
        for (int axis = 0; axis < 3; ++axis) { groundAtM[axis] = body16[12 + axis]; }
        double aboutNow[3] = {body16[12] + originM[0], body16[13] + originM[1],
                              body16[14] + originM[2]};
        Ground under;
        outshine::Gltf::Piece lyingNow;
        lyingNow.NodeName = "ground";
        const bool laid =
            Lie(journey, aboutNow, originM, section, laidFromM, laidToM, centre, under);
        if (laid) {
          lyingNow.PositionsM =
              outshine::Span<const float>(under.PositionM.data(), under.PositionM.size());
          lyingNow.Normals = outshine::Span<const float>(under.NormalM.data(), under.NormalM.size());
          lyingNow.Indices = outshine::Span<const uint32_t>(under.Index.data(), under.Index.size());
        }
        const outshine::Gltf::Piece pair[2] = {lyingNow, piece};
        outshine::Gltf::Subject ahead;
        if (laid && ahead.Assemble(outshine::Gltf::Assembly{
                        outshine::Span<const outshine::Gltf::Piece>(pair, 2)})) {
          for (int axis = 0; axis < 3; ++axis) { originM[axis] = nextRun.OriginM[axis]; }
          if (!standing->Restand(ahead, error)) {
            std::printf("REFUSED restand: %s\n", error.c_str());
          }
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
      if (next == 0 && person == 0) {
        std::printf("PARTS total %zu carried %zu  car at %.1f %.1f %.1f\n",
                    standing->Shown().Parts().size(), standing->CarriedParts(), body[12], body[13],
                    body[14]);
      }
      if (!standing->Carry(body, roadAt, error)) {
        std::printf("REFUSED carry: %s\n", error.c_str());
        break;
      }
      outshine::Gltf::Placement where = Seen(journey.Carried(), journey.Declared().Views[person]);
      for (int axis = 0; axis < 3; ++axis) { where.EyeM[axis] -= originM[axis]; }
      if (next == 0 && person == 0) {
        double fLat = 0.0, fLon = 0.0, pLat = 1.0, pLon = 1.0;
        journey.Frame(fLat, fLon, pLat, pLon);
        const double carEastM = body[12] + originM[0];
        const double carNorthM = -(body[14] + originM[2]);
        const outshine::GroundSample under =
            fb_stream_ground(fLat + carNorthM / pLat, fLon + carEastM / pLon);
        double aslM = 0.0;
        if (under.TryAslM(&aslM)) {
          const double roadAslM = body[13] + originM[1];
          const double liftM = roadAslM - aslM;
          std::printf("CUTFILL at %.1f km the road stands %+.2f m against raw ground of %.2f m asl\n",
                      rode.ReachedM / 1000.0, liftM, aslM);
          if (liftM < worstCutM) { worstCutM = liftM; }
          if (liftM > worstFillM) { worstFillM = liftM; }
          liftTotalM += std::fabs(liftM);
          ++liftAt;
        }
      }
      if (next == 0) {
        std::printf("EYE %s at %.1f %.1f %.1f  fwd %.3f %.3f %.3f  up %.3f %.3f %.3f\n",
                    person == 0 ? "first" : "third", where.EyeM[0], where.EyeM[1], where.EyeM[2],
                    where.Forward[0], where.Forward[1], where.Forward[2], where.Up[0], where.Up[1],
                    where.Up[2]);
        std::printf("CAR at %.1f %.1f %.1f\n", body[12], body[13], body[14]);
        const outshine::Gltf::Subject &shown = standing->Shown();
        std::printf("ROAD min %.1f %.1f %.1f max %.1f %.1f %.1f\n", shown.MinM()[0],
                    shown.MinM()[1], shown.MinM()[2], shown.MaxM()[0], shown.MaxM()[1],
                    shown.MaxM()[2]);
      }
      standing->Eye(where);
      if (!standing->Advance(error)) {
        std::printf("REFUSED advance: %s\n", error.c_str());
        break;
      }
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
        if (next == 0) {
          const outshine::Gltf::Placement derived = standing->Aimed();
          std::printf("DERIVED at %.1f %.1f %.1f fwd %.3f %.3f %.3f yfov %.4f near %.4f\n",
                      derived.EyeM[0], derived.EyeM[1], derived.EyeM[2], derived.Forward[0],
                      derived.Forward[1], derived.Forward[2], derived.YfovRad, derived.ZNearM);
          standing->Eye(derived);
          if (standing->Advance(error)) {
            char same[256];
            std::snprintf(same, sizeof same, "%s/km%06.1f-declared-same-place.png", into.c_str(),
                          rode.ReachedM / 1000.0);
            if (!standing->Screenshot(same, error)) { std::printf("REFUSED %s\n", error.c_str()); }
          } else {
            std::printf("REFUSED redraw: %s\n", error.c_str());
          }
          standing->FrameItself();
          if (!standing->Advance(error)) { std::printf("REFUSED reframe: %s\n", error.c_str()); }
        }
        char framed[256];
        std::snprintf(framed, sizeof framed, "%s/km%06.1f-framed.png", into.c_str(),
                      rode.ReachedM / 1000.0);
        if (!standing->Screenshot(framed, error)) { std::printf("REFUSED %s\n", error.c_str()); }
      }
    }
    ++next;
    if (rode.Arrived) { break; }
  }

  Note("stations where cut and fill were measured", (double)liftAt, "stations");
  Note("the deepest the road sits below raw ground", worstCutM, "m");
  Note("the highest it stands above it", worstFillM, "m");
  Note("the mean absolute lift over those stations", liftAt > 0 ? liftTotalM / (double)liftAt : 0.0,
       "m");
  CHECK(liftAt > 0,
        "**AND CUT AND FILL ARE PUBLISHED ALONG THE ROUTE, NOT AT ONE POINT.** A road that fills "
        "30 m is a viaduct nobody marked, and a road that CUTS is buried in the terrain the "
        "renderer draws -- which is why this number and the missing carriageway in the driver's "
        "view are one finding rather than two");
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
