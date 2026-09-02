#include "Earth.h"
#include "Units.h"
#include "math/Mat4.h"
#include "math/Vec3.h"
#include "Heap.h"
#include <algorithm>
#include <chrono>
#include <numbers>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include <expected>
#include <ratio>
#include <cstdint>

#include "EngineHeld.h"
#include "TileGeodesy.h"

namespace outshine {

constexpr double kFovUnsaidDeg = 55.0;
constexpr double kBelowAnyGroundM = -1.0e3;

bool Engine::State::Rides() {
  return Carries(Ticking.Drive.State.Body, Ticking.Drive.Stood.ModelShiftM);
}

bool Engine::State::Watches() {
  if (!Session.Views || !Picture.Standing) { return true; }
  const Scenario::View &seen = Session.Views->Active();
  if (!seen.Sees.Placed && !seen.Sees.Stands.GlobeAnchor) { return true; }
  Vec3 station = seen.Sees.Stands.AtM + seen.OffsetM;
  if (seen.Sees.Stands.GlobeAnchor) {
    double heightM = seen.Sees.Stands.Geodetic.HeightM;
    if (seen.Sees.Stands.SamplesHeight) {
      if (!World.Stack.Opened()) {
        Error = "a view samples the ground's height and no ground stands -- a scenario declares a "
                "world before anything can be placed on it";
        return false;
      }
      const GroundSample under = World.Stack.Ground().At(seen.Sees.Stands.Geodetic.LatitudeDeg,
                                                         seen.Sees.Stands.Geodetic.LongitudeDeg);
      double aslM = 0.0;
      if (!under.TryAslM(&aslM)) {
        Error = "a view samples the ground at " + Said(seen.Sees.Stands.Geodetic.LatitudeDeg) +
                ", " + Said(seen.Sees.Stands.Geodetic.LongitudeDeg) +
                " and the terrain there is not resident -- the height it stands at is not a "
                "number this engine may invent";
        return false;
      }
      heightM += aslM;
    }
    const Ground::EnuFrame frame = Ground::EnuFrame::At(
        Session.Declared.Ground.Origin.LatitudeDeg, Session.Declared.Ground.Origin.LongitudeDeg);
    Ground::Enu where{};
    if (!frame.TryFromGeo(Ground::Geo{.LongitudeDeg = seen.Sees.Stands.Geodetic.LongitudeDeg,
                                      .LatitudeDeg = seen.Sees.Stands.Geodetic.LatitudeDeg,
                                      .HeightM = heightM},
                          &where)) {
      Error = "a view stands at " + Said(seen.Sees.Stands.Geodetic.LatitudeDeg) + ", " +
              Said(seen.Sees.Stands.Geodetic.LongitudeDeg) +
              " and the world's own origin is too polar for a local frame to carry it";
      return false;
    }
    station[0] = where.EastM + seen.OffsetM[0];
    station[1] = where.UpM + seen.OffsetM[1];
    station[2] = -where.NorthM + seen.OffsetM[2];
  }
  Published.Places("the standing eye, east", station[0], "m");
  Published.Places("the standing eye, up", station[1], "m");
  Published.Places("the standing eye, south", station[2], "m");
  Vec3 ahead;
  if (seen.Sees.Stands.GlobeAnchor) {
    const double bearing = seen.Sees.Stands.BearingDeg * kDeg2Rad;
    const double pitch = seen.Sees.Stands.PitchDeg * kDeg2Rad;
    ahead[0] = std::cos(pitch) * std::sin(bearing);
    ahead[1] = std::sin(pitch);
    ahead[2] = -std::cos(pitch) * std::cos(bearing);
  } else {
    const Quat &q = seen.Sees.Stands.Facing;
    ahead[0] = 2.0 * (q.X * q.Z + q.W * q.Y);
    ahead[1] = 2.0 * (q.Y * q.Z - q.W * q.X);
    ahead[2] = -(1.0 - 2.0 * (q.X * q.X + q.Y * q.Y));
  }
  const Vec3 onto = seen.Sees.LooksAt ? seen.Sees.LookAtM : station + ahead;
  Render::Viewpoint standing;
  if (!Render::Viewpoint::LookAt(station, onto, seen.Sees.UpM, standing)) { return true; }
  standing.YfovRad = (seen.Sees.FovDeg > 0.0 ? seen.Sees.FovDeg : kFovUnsaidDeg) * kDeg2Rad;
  standing.ZNearM = seen.Sees.NearM > 0.0 ? seen.Sees.NearM : Core::Live::NearestStandable();
  standing.ZFarM = seen.Sees.FarM > 0.0 ? seen.Sees.FarM : 0.0;
  if (seen.Sees.Orthographic) {
    standing.Kind = Render::CameraKind::Orthographic;
    standing.YMagM = seen.Sees.YMagM;
    standing.XMagM = seen.Sees.XMagM > 0.0 ? seen.Sees.XMagM : seen.Sees.YMagM;
  }
  Picture.Standing->Eye(standing);
  return true;
}

bool Engine::State::Carries(const Physics::Rigid &body, const Vec3 &shiftM) {
  return Carries(0, body, shiftM);
}

bool Engine::State::Carries(size_t which, const Physics::Rigid &body, const Vec3 &shiftM) {
  Mat4 bodyFromWorld = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  {
    const Quat &q = body.OrientationQ;
    const double w = q.W;
    const double x = q.X;
    const double y = q.Y;
    const double z = q.Z;
    bodyFromWorld[0] = 1.0 - 2.0 * (y * y + z * z);
    bodyFromWorld[1] = 2.0 * (x * y + z * w);
    bodyFromWorld[2] = 2.0 * (x * z - y * w);
    bodyFromWorld[4] = 2.0 * (x * y - z * w);
    bodyFromWorld[5] = 1.0 - 2.0 * (x * x + z * z);
    bodyFromWorld[6] = 2.0 * (y * z + x * w);
    bodyFromWorld[8] = 2.0 * (x * z + y * w);
    bodyFromWorld[9] = 2.0 * (y * z - x * w);
    bodyFromWorld[10] = 1.0 - 2.0 * (x * x + y * y);
  }
  for (int axis = 0; axis < 3; ++axis) {
    bodyFromWorld[12 + axis] = body.PositionM[axis] + bodyFromWorld[0 + axis] * shiftM[0] +
                               bodyFromWorld[4 + axis] * shiftM[1] +
                               bodyFromWorld[8 + axis] * shiftM[2];
  }

  if (!Picture.Standing) { return true; }
  const Mat4 stillM = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  if (!Picture.Standing->Carry(which, bodyFromWorld, stillM, Error)) { return false; }
  if (which > 0) { return true; }
  Published.Places("the body, east", body.PositionM[0], "m");
  Published.Places("the body, up", body.PositionM[1], "m");
  Published.Places("the body, south", body.PositionM[2], "m");
  Published.Places("the mesh it carries, east", bodyFromWorld[12], "m");
  Published.Places("the mesh it carries, up", bodyFromWorld[13], "m");
  Published.Places("the mesh it carries, south", bodyFromWorld[14], "m");
  if (Session.Volumes) {
    Session.Volumes->Probe(0, body.PositionM, Ticking.ElapsedS);
    for (const TriggerField::Fired &fired : Session.Volumes->Drain()) {
      ++Session.Fired;
      Published.Places(
          "events a declared volume has fired", static_cast<double>(Session.Fired), "events");
      Session.Carried.push_back("a volume fired event " + std::to_string(fired.Event) +
                                " for body " + std::to_string(fired.Body));
    }
  }
  if (!Session.Views) { return true; }

  const Scenario::View &seen = Session.Views->Active();
  if (seen.Sees.Placed) { return Watches(); }

  const Vec3 seatM = seen.OffsetM - Ticking.Drive.Stood.CentreM;
  Vec3 at;
  for (int axis = 0; axis < 3; ++axis) {
    at[axis] = body.PositionM[axis] + bodyFromWorld[0 + axis] * seatM[0] +
               bodyFromWorld[4 + axis] * seatM[1] + bodyFromWorld[8 + axis] * seatM[2];
  }
  const Vec3 ahead = {
      {at[0] - bodyFromWorld[8], at[1] - bodyFromWorld[9], at[2] - bodyFromWorld[10]}};
  Vec3 eye = at;
  if (seen.DistanceM > 0.0) {
    const double back = seen.DistanceM;
    for (int axis = 0; axis < 3; ++axis) {
      eye[axis] =
          at[axis] + bodyFromWorld[8 + axis] * back + bodyFromWorld[4 + axis] * back * seen.RisesBy;
    }
  }
  Published.Places("the carried eye, east", eye[0], "m");
  Published.Places("the carried eye, up", eye[1], "m");
  Published.Places("the carried eye, south", eye[2], "m");
  Render::Viewpoint from;
  if (!Render::Viewpoint::LookAt(eye, seen.DistanceM > 0.0 ? at : ahead, 0.0, from)) {
    return true;
  }
  from.YfovRad = (seen.Sees.FovDeg > 0.0 ? seen.Sees.FovDeg : kFovUnsaidDeg) * kDeg2Rad;
  if (Picture.Standing) { Picture.Standing->Eye(from); }
  return true;
}

bool Engine::State::Updates() {
  if (Ticking.Drove || Session.Declared.Ground.Declared) {
    double atLat = 0.0;
    double atLon = 0.0;
    WhereTheEyeStands(atLat, atLon);
    if (World.Stack.Opened()) {
      const Heap::Tagged restanding("world-restand");
      World.Stack.Restand(atLat, atLon, Ground::kStreamBudgetMs);
      {
        const Heap::Tagged growing("world-grow");
        (void)Grows(atLat, atLon);
      }
    }
  }

  if (Ticking.Drove) {
    if (Ticking.Steps >= Ticking.MostSteps) {
      Error = "the drive has taken " + Said(static_cast<double>(Ticking.Steps)) +
              " steps and its own plan allows " + Said(static_cast<double>(Ticking.MostSteps)) +
              " at the slowest station on it, so it is not arriving";
      return false;
    }
    ++Ticking.Steps;
    const Heap::Tagged ticking("drive-tick");
    const Sim::Ridden &rode = Sim::DriveTick(Ticking.Drive.Way,
                                             Ticking.Drive.Stood,
                                             *Ticking.Surface,
                                             Ticking.Drive.State,
                                             Session.Declared.Motion.StepS,
                                             nullptr);
    if (!rode.Found || rode.Lost) {
      Error = "the drive left its corridor at " + Said(rode.ReachedM) + " m";
      return false;
    }
    if (rode.Arrived) {
      Published.Places("wheel-steps that asked the ground what it is",
                       static_cast<double>(rode.GroundAsked),
                       "steps");
      Published.Places("steps it could answer", static_cast<double>(rode.GroundAnswered), "steps");
      return false;
    }
    {
      const Heap::Tagged riding("drive-ride");
      if (!Rides()) { return false; }
    }
  }
  if (Ticking.Drove) {
    Published.Places("how far along it the body has come", Ticking.Drive.State.Tally.ReachedM, "m");
    Published.Places(
        "ticks the one lane task has kept", static_cast<double>(Ticking.Drive.State.Kept), "ticks");
    Published.Places("bytes the world holds while it drives",
                     static_cast<double>(HeapProbe::LiveBytes()),
                     "bytes");
  }
  Falls();
  if (!Watches()) { return false; }
  return Grounds(false);
}

bool Engine::State::Draws() {
  if (!Ticking.Drove && !Ticking.Freestanding.empty() && Picture.Standing &&
      Picture.Standing->Stands()) {
    const Vec3 unshifted;
    if (!Picture.Standing->Carries(Ticking.Freestanding.size(), Error)) { return false; }
    for (size_t which = 0; which < Ticking.Freestanding.size(); ++which) {
      if (!Carries(which, Ticking.Freestanding[which], unshifted)) { return false; }
    }
  }
  if (Picture.Standing && !Picture.Standing->Advance(Error)) { return false; }
  return true;
}

void Engine::keepSamples(size_t steps) {
  S_->Cost.Advance.Keeps(steps);
  S_->Cost.Render.Keeps(steps);
}

void Engine::stepTimesMs(std::vector<double> &out) const {
  S_->Cost.Advance.Into(out);
}

void Engine::frameTimesMs(std::vector<double> &out) const {
  S_->Cost.Render.Into(out);
}

Result Engine::advance() {
  const auto began = std::chrono::steady_clock::now();
  S_->Published.Opens();
  if (!S_->Updates()) { return std::unexpected(S_->Error); }
  S_->Ticking.ElapsedS += S_->Session.Declared.Motion.StepS;
  S_->Tells();
  const bool drew = S_->Draws();
  S_->Cost.Advance.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return drew ? Result{} : std::unexpected(S_->Error);
}

void Engine::State::Falls() {
  if (Ticking.Freestanding.empty()) { return; }
  const double stepS =
      Session.Declared.Motion.StepS > 0.0 ? Session.Declared.Motion.StepS : 1.0 / 60.0;
  const double gravityMs2 = Session.Declared.Ground.GravityMs2 > 0.0
                                ? Session.Declared.Ground.GravityMs2
                                : kStandardGravityMs2;
  for (Physics::Rigid &held : Ticking.Freestanding) {
    Physics::Wrench pulled;
    pulled.ForceN[1] = -held.MassKg * gravityMs2;
    Physics::Step(held, pulled, stepS);
  }
  Published.Places(
      "bodies standing on no route", static_cast<double>(Ticking.Freestanding.size()), "bodies");
  Published.Places("the first of them, up", Ticking.Freestanding.front().PositionM[1], "m");
  Published.Places("and how fast it falls", Ticking.Freestanding.front().VelocityMs[1], "m/s");
}

void Engine::State::Drew() {
  const Heap::Tagged drew("frame-drew");
  const Heap::Tagged telling("frame-measures");
  Published.Places(
      "bodies the world's generators placed", static_cast<double>(World.Placed), "bodies");
  Published.Places(
      "instances its draw sources made", static_cast<double>(World.Instanced), "instances");
  Published.Places(
      "how far the placement chain reached", static_cast<double>(World.Reached), "steps");
  Published.Places(
      "streets the world holds", static_cast<double>(World.Stack.Ways().Ways().size()), "ways");
  Published.Places("water surfaces it holds",
                   static_cast<double>(World.Stack.WaterBodies().Surfaces().size()),
                   "surfaces");
  Published.Places("building footprints it holds",
                   static_cast<double>(World.Stack.Footprints().Footprints().size()),
                   "footprints");
  Published.Places("batches the picture draws",
                   static_cast<double>(Picture.Device.SubjectBatchCount()),
                   "batches");
  Published.Places("stages the compiled plan runs",
                   static_cast<double>(Picture.Standing->PlanStages()),
                   "stages");
  Published.Places(
      "passes it runs them in", static_cast<double>(Picture.Standing->PlanPasses()), "passes");
  Published.Places("vertex uniform pushes the subject stages make",
                   static_cast<double>(Picture.Device.SubjectUniformPushes()),
                   "pushes");
  Published.Places(
      "batches the shadow casts", static_cast<double>(Picture.Device.ShadowCastCount()), "batches");
  Published.Places("placement rows the renderer has been sent",
                   static_cast<double>(Picture.Device.SubjectPlacementsMoved()),
                   "rows");
  Published.Places("frames the subject drew shadowed",
                   static_cast<double>(Picture.Device.ShadowedFrames()),
                   "frames");
  Published.Places("bytes the frame's drawing left behind",
                   static_cast<double>(Core::Live::TookDrawing()),
                   "bytes");
  Published.Places("its centre, east", Picture.Standing->ShadowCentreStanding()[0], "m");
  Published.Places("its centre, up", Picture.Standing->ShadowCentreStanding()[1], "m");
}

void Engine::State::Inspected() {
  if (!Picture.Standing) { return; }
  const Heap::Tagged asking("frame-measures");
  {
    std::vector<float> depth;
    if (Picture.Device.ReadShadowAtlas(depth) == Render::ReadState::Ready) {
      double least = kBeyondAnyCoordinate;
      double most = -kBeyondAnyCoordinate;
      double written = 0.0;
      for (const float one : depth) {
        least = std::min(static_cast<double>(one), least);
        most = std::max(static_cast<double>(one), most);
        if (one > 0.0f) { written += 1.0; }
      }
      Published.Places("the shadow atlas, least depth", least, "");
      Published.Places("the shadow atlas, most depth", most, "");
      Published.Places("texels above the clear", written, "texels");
      Published.Places(
          "the shadow radius it stood on", Picture.Standing->ShadowRadiusStanding(), "m");
    }
  }
  {
    uint32_t kept = 0;
    uint32_t batches = 0;
    if (Picture.Device.ReadKeptIndices(kept, batches) == Render::ReadState::Ready) {
      Published.Places("cull: indices the subject cull kept", static_cast<double>(kept), "indices");
      Published.Places("cull: batches that kept any", static_cast<double>(batches), "batches");
    }
  }
  {
    std::array<float, Render::kIrradianceFloats> held = {{}};
    if (Picture.Device.ReadSkyIrradiance(held) == Render::ReadState::Ready) {
      static const std::array<const char *const, Render::kIrradianceFloats> kNamed = {
          "the device's sky irradiance, red",
          "the device's sky irradiance, green",
          "the device's sky irradiance, blue",
          "the device's transmittance toward the sun, red",
          "the device's transmittance toward the sun, green",
          "the device's transmittance toward the sun, blue"};
      for (size_t at = 0; at < Render::kIrradianceFloats; ++at) {
        Published.Places(kNamed[at], static_cast<double>(held[at]), "");
      }
    }
  }
  {
    std::vector<float> velocity;
    if (Picture.Device.ReadSceneVelocity(velocity) == Render::ReadState::Ready) {
      double moving = 0.0;
      double furthest = 0.0;
      for (size_t at = 0; at + 1 < velocity.size(); at += 2) {
        const auto across = static_cast<double>(velocity[at]);
        const auto down = static_cast<double>(velocity[at + 1]);
        if (across <= kBelowAnyGroundM || down <= kBelowAnyGroundM) { continue; }
        const double moved = std::sqrt(across * across + down * down);
        if (moved > 0.0) { moving += 1.0; }
        furthest = std::max(moved, furthest);
      }
      Published.Places("pixels the velocity target says moved", moving, "px");
      Published.Places("the furthest any of them moved", furthest, "ndc");
    }
  }
  Published.Places("the exposure the picture applied",
                   static_cast<double>(Picture.Device.ExposureApplied()),
                   "1/(cd/m2)");
  {
    std::vector<float> linear;
    if (Picture.Device.ReadSceneLinear(linear) == Render::ReadState::Ready) {
      double brightest = 0.0;
      for (size_t at = 0; at + 3 < linear.size(); at += 4) {
        for (int channel = 0; channel < 3; ++channel) {
          brightest = static_cast<double>(linear[at + channel]) > brightest
                          ? static_cast<double>(linear[at + channel])
                          : brightest;
        }
      }
      Published.Places("the brightest the scene's linear buffer reached", brightest, "");
    }
  }
  {
    std::vector<uint8_t> shown;
    if (Picture.Device.ReadPixels(shown) == Render::ReadState::Ready) {
      double peak = 0.0;
      for (size_t at = 0; at + 3 < shown.size(); at += 4) {
        for (int channel = 0; channel < 3; ++channel) {
          peak = static_cast<double>(shown[at + channel]) > peak
                     ? static_cast<double>(shown[at + channel])
                     : peak;
        }
      }
      Published.Places("the brightest the presented frame shows", peak, "of 255");
    }
  }
}

double Engine::stepSeconds() const {
  return S_->Session.Declared.Motion.StepS;
}

Result Engine::advance(double elapsedS) {
  if (elapsedS > 0.0) { S_->Ticking.OwedS += elapsedS; }
  bool stood = true;
  for (int step = 0; step < S_->Session.Declared.Motion.MostStepsInArrears &&
                     S_->Ticking.OwedS >= S_->Session.Declared.Motion.StepS;
       ++step) {
    S_->Ticking.OwedS -= S_->Session.Declared.Motion.StepS;
    stood = advance().has_value();
    if (!stood) { break; }
  }
  if (S_->Ticking.OwedS >
      S_->Session.Declared.Motion.MostStepsInArrears * S_->Session.Declared.Motion.StepS) {
    S_->Ticking.OwedS = 0.0;
  }
  return stood ? Result{} : std::unexpected(S_->Error);
}

Result Engine::run() {
  if (!S_->Picture.Standing) {
    S_->Error = "no scenario is standing, so there is nothing to run";
    return std::unexpected(S_->Error);
  }
  while (advance()) {}
  return S_->Error.empty() ? Result{} : std::unexpected(S_->Error);
}

} // namespace outshine
