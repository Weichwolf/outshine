#include "Earth.h"
#include "AzimuthElevation.h"
#include "math/Units.h"
#include "math/Mat4.h"
#include "math/Vec3.h"
#include "Heap.h"
#include <algorithm>
#include <chrono>
#include <numbers>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <expected>
#include <ratio>
#include <cstdint>

#include "EngineHeld.h"
#include "Lens.h"
#include "Viewing.h"
#include "Views.h"
#include "Live.h"
#include "TileGeodesy.h"

namespace outshine {

constexpr double kBelowAnyGroundM = -1.0e3;

namespace Says {
constexpr auto kInvalidTriggerProbe =
    "trigger probe rejected an invalid body position or simulation time";
constexpr auto kInvalidElapsedTime = "advance requires finite nonnegative elapsed seconds";
constexpr auto kElapsedTimeOverflow = "elapsed time exceeds the simulation accumulator range";
constexpr auto kCameraAssemblyRequired = "assemble the current declaration before following a body";
constexpr auto kCameraBodyRequired = "the followed body is missing or no longer alive";
constexpr auto kInvalidViewProjection =
    "the selected view declares an invalid or unrepresentable projection";
constexpr auto kInvalidCarriedView = "the carried view has no valid camera basis";
}

namespace {
[[nodiscard]] Mat4 BodyTransform(const Physics::Rigid &body, const Vec3 &shiftM) {
  Mat4 worldFromBody = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  {
    const Quat &q = body.OrientationQ;
    const double w = q.W;
    const double x = q.X;
    const double y = q.Y;
    const double z = q.Z;
    worldFromBody[0] = 1.0 - 2.0 * (y * y + z * z);
    worldFromBody[1] = 2.0 * (x * y + z * w);
    worldFromBody[2] = 2.0 * (x * z - y * w);
    worldFromBody[4] = 2.0 * (x * y - z * w);
    worldFromBody[5] = 1.0 - 2.0 * (x * x + z * z);
    worldFromBody[6] = 2.0 * (y * z + x * w);
    worldFromBody[8] = 2.0 * (x * z + y * w);
    worldFromBody[9] = 2.0 * (y * z - x * w);
    worldFromBody[10] = 1.0 - 2.0 * (x * x + y * y);
  }
  for (int axis = 0; axis < 3; ++axis) {
    worldFromBody[12 + axis] = body.PositionM[axis] + worldFromBody[0 + axis] * shiftM[0] +
                               worldFromBody[4 + axis] * shiftM[1] +
                               worldFromBody[8 + axis] * shiftM[2];
  }

  return worldFromBody;
}

[[nodiscard]] std::expected<void, Render::LensError>
ApplyCamera(Core::Live &live,
            const Render::SceneRenderer &renderer,
            const Camera &camera,
            Render::Viewpoint view) noexcept {
  view.Kind =
      camera.Orthographic ? Render::CameraKind::Orthographic : Render::CameraKind::Perspective;
  view.YfovRad = (camera.FovDeg == 0 ? Scenario::kFovUnsaidDeg : camera.FovDeg) * kDeg2Rad;
  view.ZNearM = !camera.Orthographic && camera.NearM == 0 ? Camera::kNearestM : camera.NearM;
  view.ZFarM = camera.FarM;
  view.XMagM = camera.XMagM;
  view.YMagM = camera.YMagM;
  const auto lens = Render::Lens::From(view, renderer.PictureW(), renderer.PictureH());
  if (!lens) { return std::unexpected(lens.error()); }
  live.Eye(view);
  return {};
}
}

bool Engine::State::Watches() {
  if (!Session.Views || !Picture.Standing) { return true; }
  const Scenario::View &seen = Session.Views->Active();
  if (seen.Placement == Scenario::CameraPlacement::FollowEntity) {
    return FollowCamera(*Session.Views);
  }
  Vec3 station = seen.Sees.PositionM + seen.OffsetM;
  if (seen.Placement == Scenario::CameraPlacement::Geodetic) {
    double heightM = seen.Geographic.Geodetic.HeightM;
    if (seen.Geographic.SamplesHeight) {
      if (!World.Stack.Opened()) {
        Error = "a view samples the ground's height and no ground stands -- a scenario declares a "
                "world before anything can be placed on it";
        return false;
      }
      const GroundSample under =
          World.Stack.Ground().At({.LongitudeDeg = seen.Geographic.Geodetic.LongitudeDeg,
                                   .LatitudeDeg = seen.Geographic.Geodetic.LatitudeDeg});
      const std::optional<double> aslM = under.AslM();
      if (!aslM) {
        Error = "a view samples the ground at " + Said(seen.Geographic.Geodetic.LatitudeDeg) +
                ", " + Said(seen.Geographic.Geodetic.LongitudeDeg) +
                " and the terrain there is not resident -- the height it stands at is not a "
                "number this engine may invent";
        return false;
      }
      heightM += *aslM;
    }
    const Ground::EnuFrame frame = Ground::EnuFrame::At(
        Ground::Geo{.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                    .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg});
    const std::optional<Ground::Enu> where =
        frame.FromGeo(Ground::Geo{.LongitudeDeg = seen.Geographic.Geodetic.LongitudeDeg,
                                  .LatitudeDeg = seen.Geographic.Geodetic.LatitudeDeg,
                                  .HeightM = heightM});
    if (!where) {
      Error = "a view stands at " + Said(seen.Geographic.Geodetic.LatitudeDeg) + ", " +
              Said(seen.Geographic.Geodetic.LongitudeDeg) +
              " and the world's own origin is too polar for a local frame to carry it";
      return false;
    }
    station[0] = where->EastM + seen.OffsetM[0];
    station[1] = where->UpM + seen.OffsetM[1];
    station[2] = -where->NorthM + seen.OffsetM[2];
  }
  Published.Places("the standing eye, east", station[0], "m");
  Published.Places("the standing eye, up", station[1], "m");
  Published.Places("the standing eye, south", station[2], "m");
  Camera resolved = seen.Sees;
  resolved.PositionM = station;
  if (seen.Placement == Scenario::CameraPlacement::Geodetic && !seen.Sees.LooksAt) {
    const double bearing = seen.Geographic.BearingDeg * kDeg2Rad;
    const double pitch = seen.Geographic.PitchDeg * kDeg2Rad;
    const Vec3 ahead = EastUpSouthDirection(bearing, pitch);
    resolved.LooksAt = true;
    resolved.LookAtM = station + ahead;
  }
  Mat4 model;
  if (!resolved.modelMatrix(model)) {
    Error = "the declared camera has no valid local transform";
    return false;
  }
  Render::Viewpoint standing;
  standing.EyeM = station;
  for (int axis = 0; axis < 3; ++axis) {
    standing.Right[axis] = model[axis];
    standing.Up[axis] = model[4 + axis];
    standing.Forward[axis] = -model[8 + axis];
  }
  if (!ApplyCamera(*Picture.Standing, Picture.Device, seen.Sees, standing)) {
    Error = Says::kInvalidViewProjection;
    return false;
  }
  return true;
}

bool Engine::State::Carries(size_t which, const Physics::Rigid &body, const Vec3 &shiftM) {
  const Mat4 bodyFromWorld = BodyTransform(body, shiftM);

  if (!Picture.Standing) { return true; }
  const Mat4 stillM = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  if (!Picture.Standing->Carry(
          which, {.WorldFromBodyM = bodyFromWorld, .AsBuilt = stillM}, Error)) {
    return false;
  }
  if (which > 0) { return true; }
  Published.Places("the body, east", body.PositionM[0], "m");
  Published.Places("the body, up", body.PositionM[1], "m");
  Published.Places("the body, south", body.PositionM[2], "m");
  Published.Places("the mesh it carries, east", bodyFromWorld[12], "m");
  Published.Places("the mesh it carries, up", bodyFromWorld[13], "m");
  Published.Places("the mesh it carries, south", bodyFromWorld[14], "m");
  return true;
}

bool Engine::State::FollowCamera(const ViewBook &views) {
  const size_t active = views.ActiveIndex();
  if (Simulation->DeclarationRevision != Session.DeclarationRevision ||
      active >= Simulation->ViewBodies.size()) {
    Error = Says::kCameraAssemblyRequired;
    return false;
  }
  const auto binding = Simulation->ViewBodies[active];
  if (!binding || !Simulation->Entities.alive(Simulation->DynamicBodies[*binding].Owner)) {
    Error = Says::kCameraBodyRequired;
    return false;
  }
  const Physics::Rigid &body = Simulation->DynamicBodies[*binding].Motion;
  const Mat4 bodyFromWorld = BodyTransform(body, {});
  const Scenario::View &seen = views.Active();
  const Vec3 &seatM = seen.OffsetM;
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
  const std::optional<Render::Viewpoint> stood =
      Render::Viewpoint::LookAt({.EyeM = eye, .AimM = seen.DistanceM > 0.0 ? at : ahead}, 0.0);
  if (!stood) {
    Error = Says::kInvalidCarriedView;
    return false;
  }
  if (!ApplyCamera(*Picture.Standing, Picture.Device, seen.Sees, *stood)) {
    Error = Says::kInvalidViewProjection;
    return false;
  }
  return true;
}

void Engine::State::HandsPiecesOver() {
  World.Pieces.Into(Picture.Standing.get());
  World.Sheets.Into(Picture.Standing.get());
  if (!World.Pool) { World.Pool = std::make_unique<Tasks>(Tasks::ComputeThreads()); }
  World.Bakes.Opens(World.Pool.get(), &World.Shipping.Shaping());
  if (World.PiecesFramed) { return; }
  World.Pieces.Framed(
      TangentFrame::At({.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                        .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg}));
  World.PiecesFramed = true;
}

bool Engine::State::Bakes(size_t landsMost) {
  if (!World.Stack.Opened()) { return true; }
  const auto landed = World.Bakes.Lands(World.Stack, World.Pieces, landsMost);
  if (!landed) {
    Error = Generators::Describe(landed.error());
    return false;
  }
  (void)World.Bakes.Posts(World.Stack);
  Published.Places(
      "buildings: tiles posted to the bake", static_cast<double>(World.Bakes.Posted()), "tiles");
  Published.Places(
      "buildings: tiles landed from it", static_cast<double>(World.Bakes.Landed()), "tiles");
  Published.Places(
      "buildings: tiles in the bake right now", static_cast<double>(World.Bakes.Queued()), "tiles");
  Published.Places(
      "buildings: tiles deferred for ground", static_cast<double>(World.Bakes.Deferred()), "asks");
  return true;
}

bool Engine::State::UpdateTriggers() {
  if (!Simulation->Triggers || Simulation->DeclarationRevision != Session.DeclarationRevision) {
    return true;
  }
  for (const auto &body : Simulation->DynamicBodies) {
    if (!Simulation->Entities.alive(body.Owner)) { continue; }
    if (!Simulation->Triggers->Probe(body.Owner, body.Motion.PositionM, Ticking.ElapsedS)) {
      Error = Says::kInvalidTriggerProbe;
      return false;
    }
  }
  for (const TriggerField::Fired &fired : Simulation->Triggers->Drain()) {
    ++Session.Fired;
    Session.Carried.push_back("a volume fired event " + std::to_string(fired.Event) +
                              " for entity " + std::to_string(fired.Body.Index) + ":" +
                              std::to_string(fired.Body.Generation));
  }
  Published.Places(
      "events a declared volume has fired", static_cast<double>(Session.Fired), "events");
  return true;
}

bool Engine::State::Updates() {
  if (Session.Declared.Ground.Declared) {
    const LongitudeLatitude stands = WhereTheEyeStands();
    if (World.Stack.Opened()) {
      const auto streamingFrom = std::chrono::steady_clock::now();
      const size_t heldBefore = World.Stack.Footprints().IngestedTiles();
      {
        const Heap::Tagged restanding("world-restand");
        HandsPiecesOver();
        const auto streamed = World.Stack.Restand(stands);
        if (!streamed) {
          Error = streamed.error();
          return false;
        }
        if (!Bakes(kBakesLandedPerFrame)) { return false; }
        {
          const Heap::Tagged growing("world-grow");
          (void)Grows(stands.LatitudeDeg, stands.LongitudeDeg);
        }
      }
      Cost.StreamedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                  streamingFrom)
                            .count();
      Cost.StreamedTiles = World.Stack.Footprints().IngestedTiles() - heldBefore;
    } else {
      Cost.StreamedMs = 0.0;
      Cost.StreamedTiles = 0;
    }
  }

  const double simulationStepS =
      Session.Declared.Motion.StepS > 0.0 ? Session.Declared.Motion.StepS : 1.0 / 60.0;
  const double gravityMs2 = Session.Declared.Ground.GravityMs2;
  Simulation->Integrate(simulationStepS, {{0.0, -gravityMs2, 0.0}});
  Ticking.ElapsedS += simulationStepS;
  if (!UpdateTriggers()) { return false; }
  if (!Watches()) { return false; }
  return Grounds(false);
}

bool Engine::State::Draws() {
  if (!Simulation->DynamicBodies.empty() && Picture.Standing && Picture.Standing->Stands()) {
    const Vec3 unshifted;
    if (!Picture.Standing->Carries(Simulation->DynamicBodies.size(), Error)) { return false; }
    for (size_t which = 0; which < Simulation->DynamicBodies.size(); ++which) {
      if (!Carries(which, Simulation->DynamicBodies[which].Motion, unshifted)) { return false; }
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
  S_->Tells();
  const bool drew = S_->Draws();
  S_->Cost.Advance.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return drew ? Result{} : std::unexpected(S_->Error);
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
  if (Heap::ProcessInstrumentationEnabled()) {
    Published.Places("process C++ bytes allocated during drawing",
                     static_cast<double>(Core::Live::TookDrawing()),
                     "bytes");
  }
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
    Render::KeptDraws kept;
    if (Picture.Device.ReadKeptIndices(kept) == Render::ReadState::Ready) {
      Published.Places(
          "cull: indices the subject cull kept", static_cast<double>(kept.Indices), "indices");
      Published.Places("cull: batches that kept any", static_cast<double>(kept.Batches), "batches");
    }
  }
  {
    std::array<float, Render::kIrradianceFloats> held = {{}};
    if (Picture.Device.ReadSkyIrradiance(held) == Render::ReadState::Ready) {
      Picture.Standing->ReadIrradiance(held);
      {
        static const std::array<const char *const, 3> kSky = {"the ambient the sky casts, red",
                                                              "the ambient the sky casts, green",
                                                              "the ambient the sky casts, blue"};
        static const std::array<const char *const, 3> kGround = {
            "the ambient the ground bounces, red",
            "the ambient the ground bounces, green",
            "the ambient the ground bounces, blue"};
        for (size_t at = 0; at < 3; ++at) {
          Published.Places(kSky[at], Picture.Standing->AmbientStood()[at], "");
          Published.Places(kGround[at], Picture.Standing->GroundStood()[at], "");
        }
      }
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
  if (!std::isfinite(elapsedS) || elapsedS < 0.0) {
    return std::unexpected(Says::kInvalidElapsedTime);
  }
  const double accumulatedS = S_->Ticking.OwedS + elapsedS;
  if (!std::isfinite(accumulatedS)) { return std::unexpected(Says::kElapsedTimeOverflow); }
  S_->Ticking.OwedS = accumulatedS;
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

}
