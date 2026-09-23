#include "GeodeticCamera.h"
#include "Earth.h"
#include "AzimuthElevation.h"
#include "math/Units.h"
#include "math/Mat4.h"
#include "math/Vec3.h"
#include "Heap.h"
#include <algorithm>
#include <chrono>
#include <cassert>
#include <numbers>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <expected>
#include <ratio>
#include <cstdint>

#include "EngineHeld.h"
#include "StructureTilePublication.h"
#include "Lens.h"
#include "Viewing.h"
#include "Views.h"
#include "RuntimeScene.h"
#include "TileGeodesy.h"

namespace outshine {

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
ApplyCamera(Core::RuntimeScene &scene,
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
  scene.Eye(view);
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
    const auto position =
        ResolveGeodeticCamera(seen,
                              {.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                               .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg},
                              World.Stack.Opened() ? &World.Stack.Ground() : nullptr);
    if (!position) {
      Error = position.error();
      return false;
    }
    if (!*position) {
      Error = "geodetic camera height is pending; preload terrain before advancing";
      return false;
    }
    station = **position;
  }
  Published.Places("the standing eye, east", station[0], "m");
  Published.Places("the standing eye, up", station[1], "m");
  Published.Places("the standing eye, south", station[2], "m");
  Camera resolved = seen.Sees;
  resolved.PositionM = station;
  if (seen.Placement == Scenario::CameraPlacement::Geodetic && !seen.Sees.LooksAt) {
    const auto axes =
        ResolveGeodeticCameraAxes(seen.Geographic,
                                  {.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                                   .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg});
    if (!axes) {
      Error = axes.error();
      return false;
    }
    resolved.LooksAt = true;
    resolved.LookAtM = station + axes->Forward;
    resolved.UpM = axes->Up;
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
  World.BindSceneResources(Picture.Device);
  if (!World.Pool) { World.Pool = std::make_unique<Tasks>(Tasks::ComputeThreads()); }
  World.StructureBuilds.Opens(World.Pool.get(), &World.Shipping.Shaping());
  if (World.PiecesFramed) { return; }
  World.Pieces.Framed(
      TangentFrame::At({.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                        .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg}));
  World.PiecesFramed = true;
}

bool Engine::State::Bakes(size_t landsMost) {
  if (!World.Stack.Opened()) { return true; }
  const LongitudeLatitude eye = WhereTheEyeStands();
  const StructureBuildQueue::HeightSource heightAt{
      .Sample = [this](LongitudeLatitude at) { return World.Stack.Ground().Resident(at).AslM(); },
      .CopyField =
          [this](Data::TileId tile, Ground::HeightField::Block &into) {
            return Ground::HeightField::SharesField(
                World.Stack.Ground().StitchedField(tile), tile, into);
          },
      .Revision = {}};
  if (World.GroundBuild) {
    const auto resumeAt = std::chrono::steady_clock::now();
    World.StructureBuilds.ResumeCompletedTasks();
    Cost.BakeResume.Took(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resumeAt)
            .count());
    return StagesGroundBakes(landsMost);
  }
  if (!World.GroundPublished.Current()) {
    const auto resumeAt = std::chrono::steady_clock::now();
    World.StructureBuilds.ResumeCompletedTasks();
    Cost.BakeResume.Took(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resumeAt)
            .count());
    if (World.Stack.Ingested()) {
      const auto postingAt = std::chrono::steady_clock::now();
      (void)World.StructureBuilds.Posts(
          World.Stack, World.Stack.Footprints(), eye, heightAt, StructureCandidatesMost());
      Cost.BakePosting.Took(
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postingAt)
              .count());
    }
    return true;
  }
  const auto landingAt = std::chrono::steady_clock::now();
  auto ready = World.StructureBuilds.NextLandings(World.Stack,
                                                  World.Stack.Footprints(),
                                                  eye,
                                                  heightAt.Revision,
                                                  std::min(landsMost, size_t{1}));
  Cost.BakeLanding.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - landingAt)
          .count());
  if (!ready) {
    Error = Generators::Describe(ready.error());
    return false;
  }
  if (!ready->empty()) {
    const auto transferAt = std::chrono::steady_clock::now();
    if (auto published = PublishStructureTile(World, Picture.Device, ready->front()); !published) {
      Error = std::move(published.error());
      return false;
    }
    const double transferMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - transferAt)
            .count();
    Cost.BakeTransfer.Took(transferMs);
    Cost.BakeLiveTransfer.Took(transferMs);
    const auto commitAt = std::chrono::steady_clock::now();
    World.StructureBuilds.CommitsLandings(World.Stack, World.Stack.Footprints(), *ready);
    Cost.BakeCommit.Took(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - commitAt)
            .count());
  }
  const auto postingAt = std::chrono::steady_clock::now();
  (void)World.StructureBuilds.Posts(
      World.Stack, World.Stack.Footprints(), eye, heightAt, StructureCandidatesMost());
  Cost.BakePosting.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - postingAt)
          .count());
  Published.Places("buildings: tiles posted to the bake",
                   static_cast<double>(World.StructureBuilds.Posted()),
                   "tiles");
  Published.Places("buildings: tiles landed from it",
                   static_cast<double>(World.StructureBuilds.Landed()),
                   "tiles");
  Published.Places("buildings: tiles in the bake right now",
                   static_cast<double>(World.StructureBuilds.Queued()),
                   "tiles");
  Published.Places("buildings: tiles deferred for ground",
                   static_cast<double>(World.StructureBuilds.Deferred()),
                   "asks");
  Published.Places("buildings: stale tiles discarded",
                   static_cast<double>(World.StructureBuilds.Discarded()),
                   "tiles");
  return true;
}

bool Engine::State::UpdateCrowns(bool prepare) {
  if (!Session.Declared.Ground.VegetationEnabled) { return true; }
  if (!Picture.Standing || !World.Grown || !World.Shipping.Ready()) { return true; }
  if (World.GroundBuild) { return true; }
  if (!World.Vegetation) {
    VegetationStreaming::Config config;
    config.Cache.Store.Directory =
        Session.Under.Cache.empty() ? std::string{} : Session.Under.Cache + "/crowns";
    const auto frame =
        TangentFrame::At({.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                          .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg});
    World.Vegetation = VegetationStreaming::Create(
        Picture.Device, World.Shipping, World.Instances, frame, config, Error);
    if (!World.Vegetation) { return false; }
  }
  const auto &eye =
      Picture.Standing->Watched() ? Picture.Standing->Watching() : Picture.Standing->Aimed();
  const bool updated = World.Vegetation->Step(eye.EyeM, prepare, Error);
  Published.Places("flora: crown prototypes resident",
                   static_cast<double>(World.Vegetation->Resident()),
                   "prototypes");
  Published.Places("flora: crown prototypes wanted",
                   static_cast<double>(World.Vegetation->Wanted()),
                   "prototypes");
  return updated;
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
        static const Heap::Tag kRestandingTag("world-restand");
        const Heap::Tagged restanding(kRestandingTag);
        const auto handoffAt = std::chrono::steady_clock::now();
        HandsPiecesOver();
        Cost.PieceHandoff.Took(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - handoffAt)
                .count());
        const int vectorRing = World.GroundPublished.Current() ? Ground::kVectorRing : 0;
        const auto restandAt = std::chrono::steady_clock::now();
        const auto streamed = World.Stack.Restand(
            stands, {.IngestTilesMost = Ground::kFrameIngestTiles, .VectorRing = vectorRing});
        Cost.Restand.Took(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - restandAt)
                .count());
        if (!streamed) {
          Error = streamed.error();
          return false;
        }
        const auto bakesAt = std::chrono::steady_clock::now();
        const bool baked = Bakes(kBakesLandedPerFrame);
        Cost.Bakes.Took(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - bakesAt)
                .count());
        if (!baked) { return false; }
        {
          static const Heap::Tag kGrowingTag("world-grow");
          const Heap::Tagged growing(kGrowingTag);
          const auto growthAt = std::chrono::steady_clock::now();
          (void)Grows(stands.LatitudeDeg, stands.LongitudeDeg);
          Cost.Growth.Took(
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - growthAt)
                  .count());
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
    Cost.Streaming.Took(Cost.StreamedMs);
  }

  const auto simulationAt = std::chrono::steady_clock::now();
  const double simulationStepS =
      Session.Declared.Motion.StepS > 0.0 ? Session.Declared.Motion.StepS : 1.0 / 60.0;
  const double gravityMs2 = Session.Declared.Ground.GravityMs2;
  Simulation->Integrate(simulationStepS, {{0.0, -gravityMs2, 0.0}});
  Ticking.ElapsedS += simulationStepS;
  const bool triggered = UpdateTriggers();
  const bool watched = triggered && Watches();
  Cost.Simulation.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - simulationAt)
          .count());
  if (!triggered || !watched) { return false; }
  const GroundQuality quality =
      World.GroundPublished.Current() ? GroundQuality::Refined : GroundQuality::Playable;
  const auto groundAt = std::chrono::steady_clock::now();
  const bool grounded = Grounds(false, quality);
  Cost.Ground.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - groundAt)
          .count());
  if (!grounded) { return false; }
  const auto crownsAt = std::chrono::steady_clock::now();
  const bool crowned = UpdateCrowns(false);
  Cost.Crowns.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - crownsAt)
          .count());
  return crowned;
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
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  const auto began = std::chrono::steady_clock::now();
  S_->Published.Opens();
  const auto updateAt = std::chrono::steady_clock::now();
  const bool updated = S_->Updates();
  const double updateMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - updateAt)
          .count();
  S_->Cost.Update.Took(updateMs);
  if (!updated) { return std::unexpected(S_->Error); }
  S_->Cost.ObservesSuccessfulUpdate(updateMs, S_->Session.Declared.Ground.Declared);
  const auto tellingAt = std::chrono::steady_clock::now();
  S_->Tells();
  S_->Cost.Telling.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tellingAt)
          .count());
  const auto sceneAt = std::chrono::steady_clock::now();
  const bool drew = S_->Draws();
  S_->Cost.SceneAdvance.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sceneAt)
          .count());
  S_->Cost.Advance.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return drew ? Result{} : std::unexpected(S_->Error);
}

void Engine::State::Drew() {
  static const Heap::Tag kDrewTag("frame-drew");
  const Heap::Tagged drew(kDrewTag);
  static const Heap::Tag kTellingTag("frame-measures");
  const Heap::Tagged telling(kTellingTag);
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
                     static_cast<double>(Core::RuntimeScene::TookDrawing()),
                     "bytes");
  }
  Published.Places("its centre, east", Picture.Standing->ShadowCentreStanding()[0], "m");
  Published.Places("its centre, up", Picture.Standing->ShadowCentreStanding()[1], "m");
}

double Engine::stepSeconds() const {
  return S_->Session.Declared.Motion.StepS;
}

Result Engine::advance(double elapsedS) {
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
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

}
