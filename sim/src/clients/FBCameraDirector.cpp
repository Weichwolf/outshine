#include "FBCameraDirector.h"

#include <algorithm>
#include <cmath>
#include "FBCamera.h"
#include "FBGeodesy.h"
#include "FBLog.h"

namespace FlightBox::Clients {

namespace {

/* THE FRAMING LAW, and every range below is solved out of it: at a 60 deg vertical field
 * (core/FBCamera.h kSceneVerticalFovDeg) a feature of height h at range D fills
 *   h / (2 D tan 30 deg) = h / (1.1547 D)
 * of the frame. Nothing here is a taste setting except where it says [SET]. */

/* A TRIPOD IS PLACED OFF THE FIRE, not off the wreck, because the fire is the thing one has to be
 * able to READ: world/FBWorld.cpp draws a flame 0.45x the wreck's published silhouette and never
 * shorter than 4 m, so that height is the subject and its size is known before the camera moves.
 * 22 x that height puts the flame at 1/(1.1547*22) = 3.9 % of the frame = 28 of 720 lines, which is
 * where a flicker still reads as a flame; the same distance makes the column's 45 m end radius 43 %
 * of the frame height, i.e. a column and not a smear. Measured against the first attempt, which
 * framed off the WRECK at 9x its silhouette: a ground target publishes NO silhouette at all
 * (modules/ground declares zero extents), so every wreck landed on the 150 m floor and its 4 m flame
 * came out 5 px wide. */
const double kTripodPerFireH = 22.0;
const double kWreckFirePerDimM = 0.45;   /* the drawing side's own law, quoted so the two agree */
const double kWreckFireMinM = 4.0;
/* ...clamped where the placement stops being about the fire: nearer than 90 m the camera stands
 * INSIDE the column's own end radius (45 m) and past 400 m a 4 m flame is under 8 px whatever else
 * is in frame. */
const double kTripodMinM = 90.0, kTripodMaxM = 400.0;
/* Camera height = 0.20 D, floor 20 m -> a depression of 11.3 deg: enough that the column rises
 * against the sky rather than lying on the ground, low enough that the fire is not looked down on.
 * The floor is the only guard against a camera inside a hillside; a slope steeper than 1:4.5 toward
 * the camera still buries it. [SET] */
const double kTripodHeightFrac = 0.20, kTripodHeightMinM = 20.0;
/* Aim 0.30 D above the wreck. The boresight then points atan(0.30 - 0.20) = 5.7 deg UP while the fire
 * itself lies atan(0.20) = 11.3 deg DOWN, i.e. 17.0 deg below the boresight = 57 % of the lower half
 * = 78 % down the frame, with the column filling everything above it. */
const double kTripodAimFrac = 0.30;
/* Half a quadrant over a wreck's shot: 3.5 deg/s * 14 s = 49 deg of parallax, so the column is seen
 * from two sides and the frame is never a photograph. [SET] */
const double kOrbitDegS = 3.5;

/* The chase rig, unchanged from the one the watched view was measured on (doc/clients/clients.md). */
const double kChaseBackM = 62.0, kChaseUpM = 13.0, kChaseSideM = 18.0;
const double kChaseLeadM = 150.0;
const double kChaseLagS = 0.45;

/* HOW LONG A SHOT RUNS, each one solved against the effect it has to show (world/FBWorld.cpp's own
 * constants), because "long enough that one grasps it" is a duration and not an opinion:
 *   Impact  the ball emits for 1.6 s and its smoke for 11 s -> 7 s is the whole ball plus the column
 *           forming under it.
 *   Wreck   the standing column places one puff every 1.1 s over a 26 s life -> 14 s is 12 puffs and
 *           54 % of full height, which is where it reads as a column rather than as a fire. That is a
 *           GROUND number: a burning airframe still flying has no column, only a burst (1.6 s) and a
 *           trail, so it gets the Impact duration instead — see CutTo.
 *   Launch  a motor burns 10.7 s (measured, doc/render/units-visual.md Gap 1b) -> 5 s is the light and
 *           the trail forming, without holding the camera on a receding dot for the other six.
 *   Takeoff/Landing are [SET]: a rotation and a rollout have no published clock. */
double HoldFor(FBShotKind k) {
  switch (k) {
    case FBShotKind::Impact:  return 7.0;
    case FBShotKind::Wreck:   return 14.0;
    case FBShotKind::Launch:  return 5.0;
    case FBShotKind::Takeoff: return 8.0;
    case FBShotKind::Landing: return 12.0;
    case FBShotKind::Home:    return 1e18;
  }
  return 1e18;
}

int Prio(FBShotKind k) { return (int)k; }

/* A CUT UNDER TWO SECONDS READS AS A GLITCH, not as an edit — the one number in here that comes from
 * film rather than from the simulation. [SET] */
const double kMinShotS = 2.0;
/* The same threshold world/FBWorld.cpp calls a wreck DOWN, so the picture and the director cannot
 * disagree about whether a thing is still flying. */
const double kDownAglM = 8.0;
/* Airborne, and it is deliberately the same line: a rotation is the moment the fire risk ends. */
const double kAirborneAglM = 8.0;
const double kRollMs = 30.0;      /* [SET] below this a wheels-off is a bounce, not a take-off */
const double kLandRollMs = 15.0;  /* [SET] above this a touchdown is a landing, not a unit sitting still */
const double kMoveM = 1.0;        /* a published position that moved less has not moved */
const double kStaleS = 0.6;       /* [SET] a subject whose pose stopped publishing has nothing left to show */
/* HOW LONG A POSTED EVENT IS STILL WORTH GOING TO. One second more than the longest hold above (14 s),
 * so no event can go stale merely because another shot was running when it happened — which is what
 * loses a wreck: the bomb that kills the target lands while the camera is still on the hit before it. */
const double kEventFreshS = 15.0;

double Norm3(const double v[3]) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

} // namespace

const char *FBShotName(FBShotKind k) {
  switch (k) {
    case FBShotKind::Home:    return "home";
    case FBShotKind::Takeoff: return "takeoff";
    case FBShotKind::Launch:  return "launch";
    case FBShotKind::Landing: return "landing";
    case FBShotKind::Wreck:   return "wreck";
    case FBShotKind::Impact:  return "impact";
  }
  return "?";
}

FBCameraDirector::Track *FBCameraDirector::Find(int id) {
  for (Track &t : Tracks_)
    if (t.Id == id) return &t;
  return nullptr;
}

void FBCameraDirector::Observe(const std::vector<FBStageUnit> &stage, double simT) {
  const bool first = !HaveSim_;
  SimS_ = simT;
  HaveSim_ = true;

  for (const FBStageUnit &u : stage) {
    Track *t = Find(u.Id);
    if (!t) {
      Tracks_.push_back(Track{});
      t = &Tracks_.back();
      t->Id = u.Id;
      t->Kind = u.Kind;
      t->Team = u.Team;
      t->Name = u.Name ? u.Name : "";
      t->Prev = t->Cur = u.Pose;
      t->Hits = u.Damage.Hits;
      t->LastMoveS = simT;
      t->DimM = u.DimM;
      t->Airborne = u.Pose.ElevM - u.Pose.GroundAslM > kAirborneAglM;
      /* A ROUND THAT WAS NOT THERE LAST TICK LEFT A RAIL. It is the only event the director takes from
       * existence rather than from a published field, and it is the one a spectator sees first. */
      if (!first && u.Kind == Units::FBUnitKind::Weapon) Post(*t, FBShotKind::Launch);
      continue;
    }
    t->DimM = u.DimM;
    t->Prev = t->Cur;
    t->Cur = u.Pose;
    double a[3], b[3];
    FBGeoToEcef(t->Prev.LatDeg, t->Prev.LonDeg, t->Prev.ElevM, a);
    FBGeoToEcef(t->Cur.LatDeg, t->Cur.LonDeg, t->Cur.ElevM, b);
    const double d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    if (Norm3(d) > kMoveM) t->LastMoveS = simT;

    const double aglM = u.Pose.ElevM - u.Pose.GroundAslM;
    const bool down = aglM < kDownAglM || u.Kind == Units::FBUnitKind::Ground;
    const bool burning = !u.Damage.CombatEffective || u.Damage.Destroyed;

    /* THE FLANK OF THE BURST COUNT — one detonation, never a level and never a clock. */
    if (u.Damage.Hits > t->Hits) Post(*t, FBShotKind::Impact);
    /* ...and what a hit LEAVES, in its two stages: a holed airframe still flying is chased and lays
     * its smoke behind it; the same airframe on the ground is a fire that stands there for the rest of
     * the run. Both post ONCE — the fire never goes out, so a level test would pin the camera on the
     * first wreck forever — and the second is why a wreck is ever seen at all: the attack egresses
     * away from what it destroyed. */
    if (burning && !t->BurnSeen) {
      t->BurnSeen = true;
      Post(*t, FBShotKind::Wreck);
    }
    if (burning && down && !t->WreckSeen) {
      t->WreckSeen = true;
      Post(*t, FBShotKind::Wreck);
    }
    if (u.Kind == Units::FBUnitKind::Aircraft && !burning) {
      if (!t->Airborne && aglM > kAirborneAglM && u.Pose.SpeedMs > kRollMs) {
        t->Airborne = true;
        Post(*t, FBShotKind::Takeoff);
      } else if (t->Airborne && aglM < kAirborneAglM && u.Pose.SpeedMs > kLandRollMs) {
        t->Airborne = false;
        Post(*t, FBShotKind::Landing);
      }
    }
    t->Hits = u.Damage.Hits;
  }

  if (Held_) return;

  /* THE PICK, and it is deliberately not "the first unit in the list that had something happen": two
   * detonations land in the same tick often enough (one salvo, one bomb, two targets 150 m apart), and
   * taking them in registration order would let a graze outrank a kill. A posted event survives the
   * shot that was already running — losing it there is exactly how the wreck went unseen. */
  Track *best = nullptr;
  for (Track &t : Tracks_) {
    if (!t.HavePend) continue;
    if (simT - t.PendS > kEventFreshS) { t.HavePend = false; continue; }
    if (!best || Prio(t.PendKind) > Prio(best->PendKind) ||
        (Prio(t.PendKind) == Prio(best->PendKind) && t.PendS > best->PendS))
      best = &t;
  }
  if (best && Consider(best->PendKind, *best)) best->HavePend = false;

  /* THE RETURN. A shot ends when its hold is spent, or early when its subject stopped moving — a round
   * that detonated publishes the same position forever and there is nothing left in that frame. */
  const Track *sub = Shot_.Subject >= 0 ? Find(Shot_.Subject) : nullptr;
  const bool stale = sub && sub->Kind == Units::FBUnitKind::Weapon && !Shot_.Tripod &&
                     simT - sub->LastMoveS > kStaleS;
  if (Shot_.Kind != FBShotKind::Home && (simT - Shot_.StartS >= Shot_.HoldS || stale)) {
    if (const Track *h = Find(Home_)) CutTo(FBShotKind::Home, *h);
  }
  if (Shot_.Subject < 0)
    if (const Track *h = Find(Home_)) CutTo(FBShotKind::Home, *h);
}

void FBCameraDirector::Post(Track &t, FBShotKind kind) {
  if (kind == FBShotKind::Wreck) {
    Event_ = t.Name;
    EventTeam_ = FBUnitTeamStr(t.Team);
    EventS_ = SimS_;
  }
  if (t.HavePend && Prio(t.PendKind) > Prio(kind)) return;
  t.HavePend = true;
  t.PendKind = kind;
  t.PendS = SimS_;
}

bool FBCameraDirector::Consider(FBShotKind kind, Track &t) {
  if (Held_) return false;
  /* THE SAME SUBJECT GOING FROM HIT TO WRECK IS ONE SHOT, not two: re-cutting would throw away the
   * frame the fireball was caught in and start the wreck over from a new side. The hold is extended
   * instead, which is what makes "ball -> fire -> column" a single continuous take. */
  const bool down = t.Cur.ElevM - t.Cur.GroundAslM < kDownAglM || t.Kind == Units::FBUnitKind::Ground;
  if (kind == FBShotKind::Wreck && Shot_.Subject == t.Id && Shot_.Tripod == down &&
      (Shot_.Kind == FBShotKind::Impact || Shot_.Kind == FBShotKind::Wreck)) {
    Shot_.Kind = FBShotKind::Wreck;
    const double add = down ? HoldFor(FBShotKind::Wreck) : HoldFor(FBShotKind::Impact);
    Shot_.HoldS = std::max(Shot_.HoldS, SimS_ - Shot_.StartS + add);
    FBLog::Info("director", "HOLDS", {{"shot", FBShotName(Shot_.Kind)}, {"subject", t.Name},
                                      {"frame", Shot_.Tripod ? "tripod" : "chase"},
                                      {"holdS", Shot_.HoldS}});
    return true;
  }
  if (SimS_ - Shot_.StartS < kMinShotS) return false;
  const bool higher = Prio(kind) > Prio(Shot_.Kind);
  if (!higher && SimS_ - Shot_.StartS < Shot_.HoldS) return false;
  CutTo(kind, t);
  return true;
}

void FBCameraDirector::CutTo(FBShotKind kind, const Track &t) {
  const double aglM = t.Cur.ElevM - t.Cur.GroundAslM;
  const bool down = aglM < kDownAglM || t.Kind == Units::FBUnitKind::Ground;

  Shot_.Kind = kind;
  Shot_.Subject = t.Id;
  Shot_.StartS = SimS_;
  Shot_.HoldS = kind == FBShotKind::Wreck && !down ? HoldFor(FBShotKind::Impact) : HoldFor(kind);
  Shot_.DimM = t.DimM;
  /* A CAMERA STANDS STILL ONLY WHERE THE EVENT DOES. An airborne target is chased — a tripod at
   * 6 000 m loses a jet doing 200 m/s inside three seconds — and when that target reaches the ground
   * the wreck event above puts the tripod where it belongs. */
  Shot_.Tripod = down && (kind == FBShotKind::Wreck || kind == FBShotKind::Impact);
  Shot_.EventLatDeg = t.Cur.LatDeg;
  Shot_.EventLonDeg = t.Cur.LonDeg;
  Shot_.EventGroundM = t.Cur.GroundAslM;
  FBGeoToEcef(t.Cur.LatDeg, t.Cur.LonDeg, t.Cur.ElevM, Shot_.EventEcef);

  if (Shot_.Tripod) {
    /* THE 180-DEGREE RULE, and it is free here: the tripod goes on the side the camera was ALREADY on,
     * so the cut does not mirror the scene. With no previous camera (the first frame of a run) the
     * fallback is a beam view of the wreck's own heading, which is the readable one. */
    double az = t.Cur.HeadingDeg + 90.0;
    if (HaveEye_) {
      double E[3], N[3], U[3];
      FBEnuAxesEcef(Shot_.EventLatDeg, Shot_.EventLonDeg, E, N, U);
      const double d[3] = {LastEye_[0] - Shot_.EventEcef[0], LastEye_[1] - Shot_.EventEcef[1],
                           LastEye_[2] - Shot_.EventEcef[2]};
      const double e = d[0] * E[0] + d[1] * E[1] + d[2] * E[2];
      const double n = d[0] * N[0] + d[1] * N[1] + d[2] * N[2];
      if (std::sqrt(e * e + n * n) > 1.0) az = std::atan2(e, n) / kDeg2Rad;
    }
    Shot_.AzDeg = az;
  }
  ChaseHave_ = false;   /* a cut SNAPS; the lag swinging in would read as a pan that nobody made */
  FBLog::Info("director", "CUT",
              {{"shot", FBShotName(kind)}, {"subject", t.Name}, {"team", FBUnitTeamStr(t.Team)},
               {"frame", Shot_.Tripod ? "tripod" : "chase"}, {"holdS", Shot_.HoldS},
               {"aglM", aglM}, {"dimM", (double)t.DimM}, {"held", Held_}});
}

void FBCameraDirector::ToggleHold() {
  Held_ = !Held_;
  if (!Held_) {
    Shot_.StartS = SimS_;
    Shot_.HoldS = HoldFor(Shot_.Kind);
  }
  FBLog::Info("director", "HOLD", {{"held", Held_}, {"shot", FBShotName(Shot_.Kind)},
                                   {"subject", Shot_.Subject}});
}

void FBCameraDirector::Step(int dir) {
  if (Tracks_.empty()) return;
  int at = 0;
  for (size_t i = 0; i < Tracks_.size(); i++)
    if (Tracks_[i].Id == Shot_.Subject) { at = (int)i; break; }
  const int n = (int)Tracks_.size();
  int next = at;
  /* A SPENT ROUND IS NOT A SUBJECT. It keeps publishing the position it detonated at, so stepping onto
   * one parks the camera on a frozen point — and after a salvo those outnumber everything else in the
   * list. A ground unit that never moved is NOT skipped: standing still is what it does. */
  for (int k = 1; k <= n; k++) {
    const int i = ((at + dir * k) % n + n) % n;
    if (Tracks_[i].Kind == Units::FBUnitKind::Weapon && SimS_ - Tracks_[i].LastMoveS > kStaleS) continue;
    next = i;
    break;
  }
  /* STEPPING PINS. A subject picked by hand that the next detonation overrides was not picked by hand,
   * which is the whole point of the key — so it holds, and the same key that holds releases it. */
  Held_ = true;
  const Track &t = Tracks_[next];
  Shot_ = Shot{};
  Shot_.Kind = FBShotKind::Home;
  Shot_.Subject = t.Id;
  Shot_.StartS = SimS_;
  Shot_.HoldS = HoldFor(FBShotKind::Home);
  Shot_.DimM = t.DimM;
  ChaseHave_ = false;
  FBLog::Info("director", "STEP", {{"subject", t.Name}, {"team", FBUnitTeamStr(t.Team)},
                                   {"kind", t.Kind == Units::FBUnitKind::Weapon ? "weapon"
                                            : t.Kind == Units::FBUnitKind::Ground ? "ground" : "aircraft"},
                                   {"held", Held_}});
}

FBCameraPose FBCameraDirector::Camera(double dtS, double alpha) {
  FBCameraPose out;
  const Track *t = Shot_.Subject >= 0 ? Find(Shot_.Subject) : nullptr;
  if (!t) t = Find(Home_);
  if (!t) return out;

  /* EXTRAPOLATED, not interpolated — the same rule the cockpit eye follows, so the two views never
   * disagree about which instant the frame is of (clients/FBAppWasm.cpp EyeAt). */
  Units::FBUnitPose p = t->Cur;
  p.LatDeg = t->Cur.LatDeg + alpha * (t->Cur.LatDeg - t->Prev.LatDeg);
  p.LonDeg = t->Cur.LonDeg + alpha * FBWrap180(t->Cur.LonDeg - t->Prev.LonDeg);
  p.ElevM = t->Cur.ElevM + alpha * (t->Cur.ElevM - t->Prev.ElevM);
  p.RollDeg = t->Cur.RollDeg + alpha * FBWrap180(t->Cur.RollDeg - t->Prev.RollDeg);
  p.PitchDeg = t->Cur.PitchDeg + alpha * FBWrap180(t->Cur.PitchDeg - t->Prev.PitchDeg);
  p.YawDeg = t->Cur.YawDeg + alpha * FBWrap180(t->Cur.YawDeg - t->Prev.YawDeg);

  if (Shot_.Tripod) TripodPose(p, dtS, out);
  else ChasePose(p, dtS, out);

  for (int i = 0; i < 3; i++) LastEye_[i] = out.Eye[i];
  HaveEye_ = true;
  return out;
}

void FBCameraDirector::ChasePose(const Units::FBUnitPose &p, double dtS, FBCameraPose &out) {
  const double a = ChaseHave_ ? 1.0 - std::exp(-dtS / kChaseLagS) : 1.0;
  ChaseHave_ = true;
  ChaseYawDeg_ += a * FBWrap180(p.YawDeg - ChaseYawDeg_);
  ChasePitchDeg_ += a * FBWrap180(p.PitchDeg - ChasePitchDeg_);

  double jet[3], ofwd[3], oright[3], oup[3];
  FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, jet);
  FBCameraBasisEcef(ChaseYawDeg_, ChasePitchDeg_, 0.0, p.LatDeg, p.LonDeg, ofwd, oright, oup);
  for (int i = 0; i < 3; i++)
    out.Eye[i] = jet[i] - kChaseBackM * ofwd[i] + kChaseUpM * oup[i] + kChaseSideM * oright[i];

  const double reach = kChaseBackM + kChaseLeadM;
  const double pitchC = ChasePitchDeg_ - std::atan2(kChaseUpM, reach) / kDeg2Rad;
  const double yawC = ChaseYawDeg_ - std::atan2(kChaseSideM, reach) / kDeg2Rad;
  FBCameraBasisEcef(yawC, pitchC, 0.0, p.LatDeg, p.LonDeg, out.Fwd, out.Right, out.Up);
  out.LatDeg = p.LatDeg;
  out.LonDeg = p.LonDeg;
}

void FBCameraDirector::TripodPose(const Units::FBUnitPose &p, double dtS, FBCameraPose &out) {
  Shot_.AzDeg += kOrbitDegS * dtS;

  const double fireH = std::max(kWreckFireMinM, kWreckFirePerDimM * (double)Shot_.DimM);
  const double D = std::min(kTripodMaxM, std::max(kTripodMinM, kTripodPerFireH * fireH));
  const double H = std::max(kTripodHeightMinM, kTripodHeightFrac * D);
  const double aim = kTripodAimFrac * D;

  double E[3], N[3], U[3];
  FBEnuAxesEcef(Shot_.EventLatDeg, Shot_.EventLonDeg, E, N, U);
  /* The event's ECEF is refreshed from the LIVE pose, because a wreck that is still settling on its
   * gear moves a few metres and the camera has to keep looking at where it actually is. */
  double ev[3];
  FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, ev);
  const double az = Shot_.AzDeg * kDeg2Rad;
  const double eOff = D * std::sin(az), nOff = D * std::cos(az);
  /* Never below the surface the event itself reported, whatever the slope does between here and there. */
  const double up = std::max(H, Shot_.EventGroundM + H - p.ElevM);
  for (int i = 0; i < 3; i++) out.Eye[i] = ev[i] + E[i] * eOff + N[i] * nOff + U[i] * up;

  const double horiz = std::sqrt(eOff * eOff + nOff * nOff);
  const double yaw = std::atan2(-eOff, -nOff) / kDeg2Rad;
  const double pitch = std::atan2(aim - up, horiz) / kDeg2Rad;
  FBCameraBasisEcef(yaw, pitch, 0.0, Shot_.EventLatDeg, Shot_.EventLonDeg, out.Fwd, out.Right, out.Up);
  /* The STREAMER refines around the camera, so it is handed the camera's own place: at a tripod the
   * jet may be twenty kilometres away and the tiles under the wreck are the ones in frame. */
  out.LatDeg = Shot_.EventLatDeg + nOff / kMPerDeg;
  out.LonDeg = Shot_.EventLonDeg +
               eOff / (kMPerDeg * std::max(0.01, std::cos(Shot_.EventLatDeg * kDeg2Rad)));
}

} // namespace FlightBox::Clients
