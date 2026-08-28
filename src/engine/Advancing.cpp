#include "Heap.h"
#include <chrono>

#include "EngineHeld.h"

namespace outshine {

bool Engine::State::Rides(void) {
  return Carries(Ticking.Drive.State.Body, Ticking.Drive.Stood.ModelShiftM);
}

bool Engine::State::Carries(const Physics::Rigid &body, const double shiftM[3]) {
  return Carries(0, body, shiftM);
}

bool Engine::State::Carries(size_t which, const Physics::Rigid &body, const double shiftM[3]) {
  double bodyFromWorld[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  {
    const double *const q = body.OrientationQ;
    const double w = q[0], x = q[1], y = q[2], z = q[3];
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
  const double stillM[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  if (!Picture.Standing->Carry(which, bodyFromWorld, stillM, Error)) { return false; }
  if (which > 0) { return true; }
  Published.Places("the body, east", body.PositionM[0], "m");
  Published.Places("the body, up", body.PositionM[1], "m");
  Published.Places("the body, south", body.PositionM[2], "m");
  Published.Places("the mesh it carries, east", bodyFromWorld[12], "m");
  Published.Places("the mesh it carries, up", bodyFromWorld[13], "m");
  Published.Places("the mesh it carries, south", bodyFromWorld[14], "m");
  if (Session.Volumes) {
    Session.Volumes->Probe(0, body.PositionM, (double)Picture.Standing->At() * Session.Declared.Motion.StepS);
    for (const TriggerField::Fired &fired : Session.Volumes->Drain()) {
      ++Session.Fired;
      Published.Places("events a declared volume has fired", (double)Session.Fired, "events");
      Session.Carried.push_back("a volume fired event " + std::to_string(fired.Event) +
                             " for body " + std::to_string(fired.Body));
    }
  }
  if (!Session.Views) { return true; }

  const View &seen = Session.Views->Active();
  if (seen.Placed) {
    double station[3] = {seen.Stands.AtM[0] + seen.OffsetM[0], seen.Stands.AtM[1] + seen.OffsetM[1],
                         seen.Stands.AtM[2] + seen.OffsetM[2]};
    Published.Places("the eye, east", station[0], "m");
    Published.Places("the eye, up", station[1], "m");
    Published.Places("the eye, south", station[2], "m");
    const double *const q = seen.Stands.FacingXyzw;
    const double ahead[3] = {
        2.0 * (q[0] * q[2] + q[3] * q[1]),
        2.0 * (q[1] * q[2] - q[3] * q[0]),
        -(1.0 - 2.0 * (q[0] * q[0] + q[1] * q[1]))};
    const double onto[3] = {station[0] + ahead[0], station[1] + ahead[1], station[2] + ahead[2]};
    Gltf::Viewpoint standing;
    if (!Gltf::Viewpoint::LookAt(station, onto, 0.0, standing)) { return true; }
    standing.YfovRad = (seen.FovDeg > 0.0 ? seen.FovDeg : 55.0) * std::numbers::pi / 180.0;
    if (Picture.Standing) { Picture.Standing->Eye(standing); }
    return true;
  }
  const double *const centreM = Ticking.Drive.Stood.CentreM;
  const double seatM[3] = {seen.OffsetM[0] - centreM[0], seen.OffsetM[1] - centreM[1],
                           seen.OffsetM[2] - centreM[2]};
  double at[3];
  for (int axis = 0; axis < 3; ++axis) {
    at[axis] = body.PositionM[axis] + bodyFromWorld[0 + axis] * seatM[0] +
               bodyFromWorld[4 + axis] * seatM[1] + bodyFromWorld[8 + axis] * seatM[2];
  }
  const double ahead[3] = {at[0] - bodyFromWorld[8], at[1] - bodyFromWorld[9],
                           at[2] - bodyFromWorld[10]};
  double eye[3] = {at[0], at[1], at[2]};
  if (seen.DistanceM > 0.0) {
    const double back = seen.DistanceM;
    for (int axis = 0; axis < 3; ++axis) {
      eye[axis] = at[axis] + bodyFromWorld[8 + axis] * back +
                  bodyFromWorld[4 + axis] * back * seen.RisesBy;
    }
  }
  Published.Places("the eye, east", eye[0], "m");
  Published.Places("the eye, up", eye[1], "m");
  Published.Places("the eye, south", eye[2], "m");
  Gltf::Viewpoint from;
  if (!Gltf::Viewpoint::LookAt(eye, seen.DistanceM > 0.0 ? at : ahead, 0.0, from)) {
    return true;
  }
  from.YfovRad = (seen.FovDeg > 0.0 ? seen.FovDeg : 55.0) * std::numbers::pi / 180.0;
  if (Picture.Standing) { Picture.Standing->Eye(from); }
  return true;
}

bool Engine::State::Updates(void) {
  if (Ticking.Drove) {
    const Heap::Tagged restanding("world-restand");
    World.Stack.Restand(Ticking.Drive.Way.FrameLat, Ticking.Drive.Way.FrameLon);
    {
      const Heap::Tagged growing("world-grow");
      (void)Grows(Ticking.Drive.Way.FrameLat, Ticking.Drive.Way.FrameLon);
    }
  }

  if (Ticking.Drove) {
    if (Ticking.Steps >= Ticking.MostSteps) {
      Error = "the drive has taken " + Said((double)Ticking.Steps) +
                  " steps and its own plan allows " + Said((double)Ticking.MostSteps) +
                  " at the slowest station on it, so it is not arriving";
      return false;
    }
    ++Ticking.Steps;
    const Heap::Tagged ticking("drive-tick");
    const Sim::Ridden &rode =
        Sim::DriveTick(Ticking.Drive.Way, Ticking.Drive.Stood, *Ticking.Surface, Ticking.Drive.State,
                       Session.Declared.Motion.StepS, nullptr);
    if (!rode.Found || rode.Lost) {
      Error = "the drive left its corridor at " + Said(rode.ReachedM) + " m";
      return false;
    }
    if (rode.Arrived) {
      Published.Places("wheel-steps that asked the ground what it is", (double)rode.GroundAsked, "steps");
      Published.Places("steps it could answer", (double)rode.GroundAnswered, "steps");
      return false;
    }
    {
      const Heap::Tagged riding("drive-ride");
      if (!Rides()) { return false; }
    }
  }
  if (Ticking.Drove) {
    Published.Places("how far along it the body has come", Ticking.Drive.State.Tally.ReachedM, "m");
    Published.Places("ticks the one lane task has kept", (double)Ticking.Drive.State.Kept, "ticks");
    Published.Places("bytes the world holds while it drives", (double)HeapProbe::LiveBytes(), "bytes");
  }
  Falls();
  return true;
}

bool Engine::State::Draws(void) {
  if (!Ticking.Drove && !Ticking.Freestanding.empty() && Picture.Standing &&
      Picture.Standing->Stands()) {
    const double unshifted[3] = {0.0, 0.0, 0.0};
    if (!Picture.Standing->Carries(Ticking.Freestanding.size(), Error)) { return false; }
    for (size_t which = 0; which < Ticking.Freestanding.size(); ++which) {
      if (!Carries(which, Ticking.Freestanding[which], unshifted)) { return false; }
    }
  }
  if (Picture.Standing && !Picture.Standing->Advance(Error)) { return false; }
  return true;
}

void Engine::Keeps(size_t steps) {
  S_->Cost.Advance.Keeps(steps);
  S_->Cost.Render.Keeps(steps);
}

void Engine::StepTimesMs(std::vector<double> &out) const { S_->Cost.Advance.Into(out); }

void Engine::PictureTimesMs(std::vector<double> &out) const { S_->Cost.Render.Into(out); }

bool Engine::Advance() {
  const auto began = std::chrono::steady_clock::now();
  if (!S_->Updates()) { return false; }
  S_->Tells();
  const bool drew = S_->Draws();
  S_->Cost.Advance.Took(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return drew;
}

void Engine::State::Falls(void) {
  if (Ticking.Freestanding.empty()) { return; }
  const double stepS = Session.Declared.Motion.StepS > 0.0 ? Session.Declared.Motion.StepS : 1.0 / 60.0;
  const double gravityMs2 =
      Session.Declared.Ground.GravityMs2 > 0.0 ? Session.Declared.Ground.GravityMs2 : 9.80665;
  for (Physics::Rigid &held : Ticking.Freestanding) {
    Physics::Wrench pulled;
    pulled.ForceN[1] = -held.MassKg * gravityMs2;
    Physics::Step(held, pulled, stepS);
  }
  Published.Places("bodies standing on no route", (double)Ticking.Freestanding.size(), "bodies");
  Published.Places("the first of them, up", Ticking.Freestanding.front().PositionM[1], "m");
  Published.Places("and how fast it falls", Ticking.Freestanding.front().VelocityMs[1], "m/s");
}

void Engine::State::Drew(void) {
  const Heap::Tagged drew("frame-drew");
  const Heap::Tagged telling("frame-measures");
  Published.Places("bodies the world's generators placed", (double)World.Placed,
                   "bodies");
  Published.Places("instances its draw sources made", (double)World.Instanced,
                   "instances");
  Published.Places("how far the placement chain reached", (double)World.Reached, "steps");
  Published.Places("streets the world holds", (double)World.Stack.Ways().Ways().size(), "ways");
  Published.Places("water surfaces it holds", (double)World.Stack.WaterBodies().Surfaces().size(),
                   "surfaces");
  Published.Places("building footprints it holds",
                   (double)World.Stack.Footprints().Footprints().size(), "footprints");
  Published.Places("batches the picture draws", (double)Picture.Device.SubjectBatchCount(), "batches");
  Published.Places("stages the compiled plan runs", (double)Picture.Standing->PlanStages(),
                   "stages");
  Published.Places("passes it runs them in", (double)Picture.Standing->PlanPasses(), "passes");
  Published.Places("vertex uniform pushes the subject stages make",
                   (double)Picture.Device.SubjectUniformPushes(), "pushes");
  Published.Places("batches the shadow casts", (double)Picture.Device.ShadowCastCount(), "batches");
  Published.Places("placement rows the renderer has been sent", (double)Picture.Device.SubjectPlacementsMoved(),
         "rows");
  Published.Places("frames the subject drew shadowed", (double)Picture.Device.ShadowedFrames(), "frames");
  Published.Places("bytes the frame's drawing left behind", (double)Core::Live::TookDrawing(),
         "bytes");
  Published.Places("its centre, east", Picture.Standing->ShadowCentreStanding()[0], "m");
  Published.Places("its centre, up", Picture.Standing->ShadowCentreStanding()[1], "m");
}

void Engine::State::Inspected(void) {
  if (!Picture.Standing) { return; }
  const Heap::Tagged asking("frame-measures");
  {
    std::vector<float> depth;
    if (Picture.Device.ReadShadowAtlas(depth) == Render::ReadState::Ready) {
      double least = 1.0e30, most = -1.0e30, written = 0.0;
      for (const float one : depth) {
        if ((double)one < least) { least = (double)one; }
        if ((double)one > most) { most = (double)one; }
        if (one > 0.0f) { written += 1.0; }
      }
      Published.Places("the shadow atlas, least depth", least, "");
      Published.Places("its most", most, "");
      Published.Places("texels above the clear", written, "texels");
      Published.Places("the shadow radius it stood on", Picture.Standing->ShadowRadiusStanding(), "m");
    }
  }
  {
    std::vector<float> velocity;
    if (Picture.Device.ReadSceneVelocity(velocity) == Render::ReadState::Ready) {
      double moving = 0.0, furthest = 0.0;
      for (size_t at = 0; at + 1 < velocity.size(); at += 2) {
        const double across = (double)velocity[at], down = (double)velocity[at + 1];
        if (across <= -1.0e3 || down <= -1.0e3) { continue; }
        const double moved = std::sqrt(across * across + down * down);
        if (moved > 0.0) { moving += 1.0; }
        if (moved > furthest) { furthest = moved; }
      }
      Published.Places("pixels the velocity target says moved", moving, "px");
      Published.Places("the furthest any of them moved", furthest, "ndc");
    }
  }
  Published.Places("the exposure the picture applied", (double)Picture.Device.ExposureApplied(),
                   "1/(cd/m2)");
  {
    std::vector<float> linear;
    if (Picture.Device.ReadSceneLinear(linear) == Render::ReadState::Ready) {
      double brightest = 0.0;
      for (size_t at = 0; at + 3 < linear.size(); at += 4) {
        for (int channel = 0; channel < 3; ++channel) {
          brightest = (double)linear[at + channel] > brightest ? (double)linear[at + channel]
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
          peak = (double)shown[at + channel] > peak ? (double)shown[at + channel] : peak;
        }
      }
      Published.Places("the brightest the presented frame shows", peak, "of 255");
    }
  }
}

double Engine::StepS(void) const { return S_->Session.Declared.Motion.StepS; }

bool Engine::Advance(double elapsedS) {
  if (elapsedS > 0.0) { S_->Ticking.OwedS += elapsedS; }
  bool stood = true;
  for (int step = 0; step < S_->Session.Declared.Motion.MostStepsInArrears && S_->Ticking.OwedS >= S_->Session.Declared.Motion.StepS; ++step) {
    S_->Ticking.OwedS -= S_->Session.Declared.Motion.StepS;
    stood = Advance();
    if (!stood) { break; }
  }
  if (S_->Ticking.OwedS > S_->Session.Declared.Motion.MostStepsInArrears * S_->Session.Declared.Motion.StepS) { S_->Ticking.OwedS = 0.0; }
  return stood;
}

bool Engine::Run() {
  if (!S_->Picture.Standing) {
    S_->Error = "no scenario is standing, so there is nothing to run";
    return false;
  }
  while (Advance()) {
  }
  return S_->Error.empty();
}

}
