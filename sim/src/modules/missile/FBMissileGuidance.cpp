#include "FBMissileGuidance.h"
#include "FBAtmosphere.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBMissileArSeeker.h"
#include "FBMissileIrSeeker.h"
#include "FBMissileSeeker.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox::Modules {

namespace {
constexpr double kG = 9.80665;

double Clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

const char *FBMissileGuidance::PhaseName(Phase p) {
  switch (p) {
    case Phase::Inertial: return "INERTIAL";
    case Phase::Midcourse: return "MIDCOURSE";
    case Phase::Terminal: return "TERMINAL";
  }
  return "?";
}

void FBMissileGuidance::Bind(const FBStoreSpec &spec, FBMissileSeeker &seeker,
                             FBMissileIrSeeker &irSeeker, FBMissileArSeeker &arSeeker) {
  Spec_ = &spec;
  Seeker_ = &seeker;
  IrSeeker_ = &irSeeker;
  ArSeeker_ = &arSeeker;
  Kind_ = spec.Seeker;
  Seeker_->SetRangeM(spec.Perf.SeekerRangeM);
  Seeker_->Configure(spec.SeekerFovHalfDeg, spec.SeekerGimbalHalfDeg);
  IrSeeker_->Configure(spec.SeekerFovHalfDeg, spec.SeekerGimbalHalfDeg, spec.Perf.SeekerRangeM);
  ArSeeker_->Configure(spec.SeekerFovHalfDeg);
  /* Exactly ONE detector exists on this round, and every other one stays unpowered so its block can
   * never claim a sensor the weapon does not carry. */
  IrSeeker_->SetPowered(Kind_ == FBSeekerKind::Infrared);
  ArSeeker_->SetPowered(Kind_ == FBSeekerKind::AntiRadiation);
}

/* The cue is a RECORD and not a state: nothing steers this receiver, so what the shooter had at the
 * moment of release is written down and the round then finds its own emitter or does not. */
void FBMissileGuidance::ProgramCue(bool valid, double azDeg, double elDeg, FBArTargetClass cls) {
  if (ArSeeker_) ArSeeker_->SetTargetClass(cls);
  if (Kind_ != FBSeekerKind::AntiRadiation) return;
  FBLog::Info("missile", "PROGRAMMED", {{"launcher", LauncherId_}, {"cue", valid},
      {"cueAzDeg", azDeg}, {"cueElDeg", elDeg}, {"class", FBArTargetClassStr(cls)},
      {"memoryS", Spec_ ? Spec_->SeekerMemoryS : 0.0}});
}

void FBMissileGuidance::Program(const FBWeaponTargetState &target, int launcherId, double launchS) {
  LauncherId_ = launcherId;
  LaunchS_ = launchS;
  HaveTarget_ = target.Valid;
  TgtLatDeg_ = target.LatDeg; TgtLonDeg_ = target.LonDeg; TgtAltM_ = target.AltM;
  TgtVelE_ = target.VelE; TgtVelN_ = target.VelN; TgtVelU_ = target.VelU;
  /* The LAUNCHER'S look time in the mission-wide clock, so the programming's age is real from the first
   * tick rather than zero because it was just handed over. */
  TgtStampS_ = target.Valid ? target.StampS : -1e9;
  /* An anti-radiation round has no target state to programme at all — its one line is ProgramCue's,
   * with the angular cue in place of a position. */
  if (Kind_ == FBSeekerKind::AntiRadiation) return;
  FBLog::Info("missile", "PROGRAMMED", {{"launcher", launcherId}, {"haveTarget", HaveTarget_},
      {"tgtLat", TgtLatDeg_}, {"tgtLon", TgtLonDeg_}, {"tgtAltM", TgtAltM_},
      {"tgtVelE", TgtVelE_}, {"tgtVelN", TgtVelN_}, {"tgtVelU", TgtVelU_}});
}

void FBMissileGuidance::EnterPhase(Phase p, const char *why) {
  if (p == Phase_) return;
  FBLog::Info("missile", "PHASE", {{"from", PhaseName(Phase_)}, {"to", PhaseName(p)}, {"why", why},
      {"tofS", NowS_ - LaunchS_}, {"rangeM", RangeM_}, {"closureMs", ClosureMs_},
      {"losAzDeg", LosAzDeg_}, {"losElDeg", LosElDeg_}});
  Phase_ = p;
}

/* Strict priority: own seeker > uplink > last known. Every branch writes the SAME four fields, so the
 * law below never asks where its numbers came from — only how old they are. */
void FBMissileGuidance::UpdateTarget(const FBState &state, const Fdm::fb_fdm_state &st) {
  /* --- INFRARED. A different block, a different kind of answer: the head reports an ANGLE and the law
   * above flies the rate of it. There is no position to fuse, no velocity to estimate and no uplink to
   * fall back to — a heat-homing round is lock-before-launch and then alone. --- */
  if (Kind_ == FBSeekerKind::Infrared) {
    const FBIrstBlock &ir = state.Irst;
    bool locked = ir.H.Readable() && ir.LockIndex >= 0 && ir.LockIndex < ir.ContactCount;
    IrTerminal_ = locked;
    if (locked) {
      EnterPhase(Phase::Terminal, "own seeker acquired");
      return;
    }
    /* Nothing on the detector: fly the programming (or, if it was decoyed away from the aircraft and
     * the cartridge burnt out, whatever it last had). No reacquisition path is invented. */
    EnterPhase(Phase::Inertial, Phase_ == Phase::Terminal ? "seeker lost track" : "no lock yet");
    return;
  }

  /* --- ANTI-RADIATION. The receiver's own table, in the receiver's own priority order, filtered by
   * what this round was PROGRAMMED against. The latch is the first admissible symbol: a "loudest wins"
   * rule per tick would make the round a lottery over whichever site happens to be closest this
   * instant. A symbol that leaves the table is a transmitter that stopped, and the round may take
   * another one — that is re-acquisition, and it is the reason the doctrine is "stay dark". --- */
  if (Kind_ == FBSeekerKind::AntiRadiation) {
    const FBRwrBlock &rwr = state.Rwr;
    const FBRwrThreat *held = nullptr;
    const FBRwrThreat *fresh0 = nullptr;   /* the first admissible symbol, whatever it is */
    if (rwr.H.Readable())
      for (int i = 0; i < rwr.ThreatCount; i++) {
        const FBRwrThreat &t = rwr.Threats[i];
        if (ArSymbol_ != 0 && t.Id == ArSymbol_) { held = &t; break; }
        if (!fresh0 && ArSeeker_ && ArSeeker_->Admissible(t.Kind)) fresh0 = &t;
      }
    /* RE-ACQUISITION, and it is the third distinct behaviour in the seeker table: the latched symbol is
     * GONE from the receiver's own table, which means the transmitter behind it stopped. The round
     * switched nothing off and neither did the shooter, so it goes back to looking — and a crew that
     * blinks back on inside the cone gets a new symbol and is taken again. That is why the doctrine is
     * "go dark, and STAY dark until it is over". While the latched symbol is present it wins, which is
     * what "never re-target" means: no per-tick lottery over whichever site is loudest.
     * doc/air-to-ground.md §2.2. */
    if (!held && fresh0) { ArSymbol_ = 0; held = fresh0; }
    /* A HELD symbol is not a LOOK: the receiver keeps a threat alive for its own hold time after the
     * beam stops, and flying that stale bearing as if it were fresh would hand the round the memory the
     * whole design refuses it. Only AgeS == 0 is a measurement. */
    bool fresh = held && held->AgeS <= 0.0;
    if (held && ArSymbol_ == 0) {
      ArSymbol_ = held->Id;
      /* The FIRST acquisition is SEEKER_ACTIVE; every later one is EMITTER_REACQUIRED, logged in the
       * fresh-look branch below with the length of the silence. */
      if (!ArLost_) FBLog::Info("missile", "SEEKER_ACTIVE", {{"mode", "anti-radiation"},
          {"symbol", held->Id}, {"kind", FBEmitterKindStr(held->Kind)},
          {"brgDeg", (double)held->BearingDeg}, {"elDeg", (double)held->ElDeg},
          {"signal", (double)held->SignalNorm}, {"tofS", NowS_ - LaunchS_}});
    }
    const double memoryS = Spec_ && Spec_->SeekerMemoryS > 0.0 ? Spec_->SeekerMemoryS : kLosRateHoldS;
    if (fresh) {
      ArSigNorm_ = held->SignalNorm;
      if (ArLost_) {
        ArLost_ = false;
        FBLog::Info("missile", "EMITTER_REACQUIRED", {{"symbol", held->Id},
            {"tofS", NowS_ - LaunchS_}, {"darkS", NowS_ - ArLastLookS_},
            {"brgDeg", (double)held->BearingDeg}, {"signal", (double)held->SignalNorm}});
      }
      EnterPhase(Phase::Terminal, "own seeker acquired");
      return;
    }
    /* Inside the memory the round still flies the HELD rate, so the phase stays Terminal and nothing is
     * logged: a beam that swept off this bearing for a tick has not stopped transmitting. The EVENT is
     * the memory RUNNING OUT, because that is the moment the round stops steering. */
    double darkS = NowS_ - ArLastLookS_;
    if (ArSymbol_ != 0 && ArLastLookS_ > -1e8 && darkS <= memoryS) return;
    if (ArSymbol_ != 0 && !ArLost_ && ArLastLookS_ > -1e8) {
      ArLost_ = true;
      /* Range-to-go is NOT in this line, and its absence is the contract: this round never measured a
       * range and does not acquire one by being written about. */
      FBLog::Info("missile", "EMITTER_LOST", {{"symbol", ArSymbol_}, {"tofS", NowS_ - LaunchS_},
          {"heldS", memoryS}, {"lastSignal", ArSigNorm_}, {"altM", st.elev}, {"speedMs", st.speed}});
    }
    EnterPhase(Phase::Inertial, Phase_ == Phase::Terminal ? "emitter memory expired" : "no emitter yet");
    return;
  }

  /* --- SEMI-ACTIVE LASER. The spot travels the SAME published channel the midcourse uplink does and is
   * read the same way — the shooter's own designation, as old as the shooter's last look. It comes back
   * if the shooter comes back, which is the one place this head differs from its radar cousin. --- */
  if (Kind_ == FBSeekerKind::SemiActiveLaser) {
    bool lit = state.Datalink.H.Readable() && state.Datalink.TrackCount > 0 &&
               NowS_ - state.Datalink.Tracks[0].ReportTimeS <= kUplinkTimeoutS;
    if (lit) {
      const FBDatalinkTrack &m = state.Datalink.Tracks[0];
      SalSpotLatDeg_ = m.LatDeg;
      SalSpotLonDeg_ = m.LonDeg;
      SalSpotElevM_ = m.AltM;
      TgtStampS_ = m.ReportTimeS;
      if (!SalTerminal_) {
        SalTerminal_ = true;
        FBLog::Info("missile", "SEEKER_ACTIVE", {{"mode", "semi-active laser"},
            {"tofS", NowS_ - LaunchS_}, {"spotLat", SalSpotLatDeg_}, {"spotLon", SalSpotLonDeg_}});
      }
      EnterPhase(Phase::Terminal, "spot acquired");
      return;
    }
    if (SalTerminal_) {
      SalTerminal_ = false;
      FBLog::Info("missile", "ILLUMINATION_LOST", {{"tofS", NowS_ - LaunchS_},
          {"altM", st.elev}, {"mode", "semi-active laser"}});
    }
    /* No spot: the canards TRAIL and the bomb falls ballistically. Not a failure path — it is the
     * documented behaviour of the guidance kit and the reason a broken lase produces a miss. */
    EnterPhase(Phase::Inertial, "no spot");
    return;
  }

  /* --- Terminal. The velocity comes from the tracker every pilot owns, fed from the seeker's own radar
   * block: a contact is an echo without velocity, and the law needs one. --- */
  BfmTrack().Update(state, st, NowS_);
  const FBBfmBlock &trk = BfmTrack().Block();
  SeekerLocked_ = state.Radar.H.Readable() && state.Radar.LockIndex >= 0;
  if (SeekerLocked_ && trk.H.Readable() && trk.Locked) {
    double coslat = std::cos(st.lat * kDeg2Rad);
    TgtLatDeg_ = st.lat + trk.NorthM / kMPerDeg;
    TgtLonDeg_ = st.lon + (coslat > 1e-6 ? trk.EastM / (kMPerDeg * coslat) : 0.0);
    TgtAltM_ = st.elev + trk.UpM;
    TgtVelE_ = trk.VelE; TgtVelN_ = trk.VelN; TgtVelU_ = trk.VelU;
    TgtStampS_ = trk.H.StampS;
    HaveTarget_ = true;
    EnterPhase(Phase::Terminal, "own seeker acquired");
    return;
  }
  /* Once terminal, always terminal: a seeker that has held the target does not go back to asking. */
  if (Phase_ == Phase::Terminal) return;

  /* --- Midcourse: the uplink, read like any other instrument — head first, age included. --- */
  if (state.Datalink.H.Readable() && state.Datalink.TrackCount > 0) {
    const FBDatalinkTrack &m = state.Datalink.Tracks[0];
    if (NowS_ - m.ReportTimeS <= kUplinkTimeoutS) {
      TgtLatDeg_ = m.LatDeg; TgtLonDeg_ = m.LonDeg; TgtAltM_ = m.AltM;
      double hdg = m.HeadingDeg * kDeg2Rad;
      TgtVelE_ = m.SpeedMs * std::sin(hdg);
      TgtVelN_ = m.SpeedMs * std::cos(hdg);
      TgtVelU_ = 0.0;   /* the message carries a ground track: there is no climb rate in it */
      TgtStampS_ = m.ReportTimeS;
      HaveTarget_ = true;
      EnterPhase(Phase::Midcourse, "uplink from launcher");
      return;
    }
  }
  /* --- Inertial: nobody is telling this round anything, so it flies what it last knew. --- */
  EnterPhase(Phase::Inertial, Phase_ == Phase::Midcourse ? "uplink lost" : "no uplink yet");
}

/* THE ANGLE-ONLY LAW — everything an infrared round has, and the reason it is a different weapon.
 *
 * WHAT IS MEASURED: two angles per look, and nothing else. The head publishes a WORLD-referenced
 * bearing and elevation (sensors/FBIrstSystem, the same pair the radar publishes and for the same
 * reason — a body pair would smear this round's own rotation into the target geometry). From two
 * consecutive looks the line-of-sight RATE follows directly:
 *
 *     u        = unit vector along the line of sight, from the reported bearing/elevation
 *     Omega    = (u_prev x u_now) / dt        [rad/s, along the rotation axis of the LOS]
 *
 * which is literally the quantity a real seeker's rate gyro puts out, without a position ever existing.
 *
 * WHAT IS COMMANDED: PURE proportional navigation. True PN is `N*Vc*(Omega x u)` and needs the closing
 * SPEED; an angle-only head has none, so the round's own speed takes its place — the classical PPN form
 *
 *     a = N * V_own * (Omega x u)
 *
 * which drives the same LOS rate to zero and differs from true PN only in how hard it pulls when the
 * closure is not the own speed (a tail chase). Modelling it any other way would mean inventing a range
 * this seeker does not measure, which is exactly the cheat the whole perception boundary exists to stop.
 *
 * THE RATE IS HELD BETWEEN LOOKS. The head reports at its frame time, the loop runs at 100 Hz;
 * differentiating inside a frame would read zero rate four times out of five and then a step. So a new
 * look updates the rate and the loop flies the held one until the next, giving up after kLosRateHoldS.
 *
 * AND THIS IS WHERE A FLARE WINS: the angles come from whatever the head reports. If it has walked onto
 * a cartridge, this law flies at the cartridge, with full authority and no idea anything happened. */
bool FBMissileGuidance::AngleOnlyCommand(const FBState &state, const Fdm::fb_fdm_state &st, double &aE,
                                         double &aN, double &aU) {
  const FBIrstBlock &ir = state.Irst;
  if (!ir.H.Readable() || ir.LockIndex < 0 || ir.LockIndex >= ir.ContactCount) return false;
  const FBIrstContact &c = ir.Contacts[ir.LockIndex];

  double brg = c.BearingDeg * kDeg2Rad, elev = c.ElevAngleDeg * kDeg2Rad;
  double uE = std::sin(brg) * std::cos(elev);
  double uN = std::cos(brg) * std::cos(elev);
  double uU = std::sin(elev);

  /* A LOOK, not a tick: the block's own stamp is the head's frame time. */
  double lookS = ir.H.StampS - c.LookAgeS;
  if (lookS > IrLosStampS_ + 1e-9) {
    double dtLook = lookS - IrLosStampS_;
    if (IrLosStampS_ > -1e8 && dtLook > 1e-6 && dtLook <= kLosRateHoldS * 4.0) {
      IrOmE_ = (IrLosN_ * uU - IrLosU_ * uN) / dtLook;
      IrOmN_ = (IrLosU_ * uE - IrLosE_ * uU) / dtLook;
      IrOmU_ = (IrLosE_ * uN - IrLosN_ * uE) / dtLook;
      IrOmStampS_ = lookS;
    }
    IrLosE_ = uE; IrLosN_ = uN; IrLosU_ = uU;
    IrLosStampS_ = lookS;
  }
  if (NowS_ - IrOmStampS_ > kLosRateHoldS) { IrOmE_ = IrOmN_ = IrOmU_ = 0.0; }

  double gain = kNavConstant * (st.speed > 1.0 ? st.speed : 1.0);
  aE = gain * (IrOmN_ * uU - IrOmU_ * uN);
  aN = gain * (IrOmU_ * uE - IrOmE_ * uU);
  aU = gain * (IrOmE_ * uN - IrOmN_ * uE);
  aU += kG;   /* the same gravity bias: an unbiased law arrives low, whatever measures the angle */

  /* The trace, in the honest shape for this seeker: -1 means THIS ROUND CANNOT MEASURE IT. */
  RangeM_ = -1.0;
  ClosureMs_ = 0.0;
  LosRateDegS_ = std::sqrt(IrOmE_ * IrOmE_ + IrOmN_ * IrOmN_ + IrOmU_ * IrOmU_) * kRad2Deg;
  LosAzDeg_ = c.AzDeg;
  LosElDeg_ = c.ElDeg;
  return true;
}

/* THE ANTI-RADIATION LAW. Structurally AngleOnlyCommand with two differences, and both come out of what
 * a warning receiver is rather than out of anything written for this weapon:
 *
 *   1. THE ANGLES ARE BODY-REFERENCED. An infrared head reports a WORLD bearing because it is pointed
 *      and its gimbal angles are known; a passive receiver reports the direction a signal arrived from
 *      relative to its own nose, because that is all four antennas can say. So the line of sight is
 *      rotated into the local horizon with the round's OWN attitude — an instrument it carries, not a
 *      truth it is handed — before any rate is taken. Differentiating body angles instead would smear
 *      the round's own pitching into the target geometry.
 *
 *   2. THE MEMORY IS A RATE. After the last real measurement the held Omega stays valid for the row's
 *      SeekerMemoryS and is then set to ZERO. Zero Omega is not "fly to the last position": PN's whole
 *      command is N*V*(Omega x u), so it commands nothing lateral at all. What survives is the 1 g
 *      gravity bias, because that comes from an accelerometer and needs no target. The round therefore
 *      COASTS STRAIGHT, which is literally "stop steering now" — and the miss that produces is exactly
 *      ZEM(t) = ZEM(0)*(t_go/t_f)^N of the law already running here. doc/air-to-ground.md §Knowledge 1. */
bool FBMissileGuidance::AntiRadiationCommand(const FBState &state, const Fdm::fb_fdm_state &st,
                                             double &aE, double &aN, double &aU) {
  const FBRwrBlock &rwr = state.Rwr;
  const FBRwrThreat *held = nullptr;
  if (rwr.H.Readable() && ArSymbol_ != 0)
    for (int i = 0; i < rwr.ThreatCount; i++)
      if (rwr.Threats[i].Id == ArSymbol_) { held = &rwr.Threats[i]; break; }

  if (held && held->AgeS <= 0.0) {
    double uE = 0.0, uN = 0.0, uU = 0.0;
    FBBodyLosToEnu(st.roll, st.pitch, st.yaw, held->BearingDeg, held->ElDeg, uE, uN, uU);
    double dtLook = NowS_ - ArLastLookS_;
    /* A rate may only be taken between two looks at the SAME symbol: after a re-acquisition the
     * previous line belongs to a transmitter this round is no longer following. */
    if (ArLosSymbol_ == ArSymbol_ && ArLastLookS_ > -1e8 && dtLook > 1e-6) {
      ArOmE_ = (ArLosN_ * uU - ArLosU_ * uN) / dtLook;
      ArOmN_ = (ArLosU_ * uE - ArLosE_ * uU) / dtLook;
      ArOmU_ = (ArLosE_ * uN - ArLosN_ * uE) / dtLook;
    }
    ArLosE_ = uE; ArLosN_ = uN; ArLosU_ = uU;
    ArLosSymbol_ = ArSymbol_;
    ArLastLookS_ = NowS_;
    LosAzDeg_ = held->BearingDeg;
    LosElDeg_ = held->ElDeg;
  }
  double memoryS = Spec_ && Spec_->SeekerMemoryS > 0.0 ? Spec_->SeekerMemoryS : kLosRateHoldS;
  if (NowS_ - ArLastLookS_ > memoryS) { ArOmE_ = ArOmN_ = ArOmU_ = 0.0; }

  /* The line the command is taken about is the last one MEASURED, aged by nothing: without a rate the
   * cross products below are zero anyway, so a stale direction cannot steer the round. */
  double uE = ArLosE_, uN = ArLosN_, uU = ArLosU_;
  double gain = kNavConstant * (st.speed > 1.0 ? st.speed : 1.0);
  aE = gain * (ArOmN_ * uU - ArOmU_ * uN);
  aN = gain * (ArOmU_ * uE - ArOmE_ * uU);
  aU = gain * (ArOmE_ * uN - ArOmN_ * uE);
  aU += kG;   /* the accelerometer's own bias: it survives the memory because nothing measures it */

  RangeM_ = -1.0;   /* THE COLUMN SAYS WHAT THE SEEKER CANNOT MEASURE, for the whole flight */
  ClosureMs_ = 0.0;
  LosRateDegS_ = std::sqrt(ArOmE_ * ArOmE_ + ArOmN_ * ArOmN_ + ArOmU_ * ArOmU_) * kRad2Deg;
  return true;
}

/* THE PAVEWAY LAW — a PURSUIT law with a bang-bang actuator, and the reason it is not proportional
 * navigation is §Knowledge 4: this kit has neither the sensor for a rate nor the actuator for a
 * proportional command. The documented behaviour is "align the velocity vector with the instantaneous
 * line of sight to the spot; when aligned, trail the canards and fall ballistically, gravity-biased".
 *
 * IT DOES NOT GO THROUGH THE MISSILE'S ACCELERATION AUTOPILOT AT ALL, and that is the point rather than
 * a shortcut: FlyCommand closes two g-loops whose gains are a MISSILE'S fin authority, and a bomb kit
 * with a third of that authority drives them straight into their stops and tumbles (measured: pitch
 * -80 deg, roll -165 deg, LOC 4.7 s after release). A bang-bang actuator has two positions and no
 * third, so the law writes the FINS: full deflection in the direction that closes the error, or
 * trailing. What acceleration that produces is the DECK's answer and nobody's setting — measured at
 * ~2.6 g on arrival, which is why an LGB chases the spot instead of leading it.
 *
 * THE ROLL HOLDER STAYS, because a cruciform round that rolls applies its pitch command sideways. It is
 * the missile's, unchanged, and it is the only proportional loop on this weapon. */
bool FBMissileGuidance::LaserCommand(const FBState &state, const Fdm::fb_fdm_state &st,
                                     Pilot::FBPilotCommands &c) {
  (void)state;
  double rE = 0.0, rN = 0.0;
  FBEnuOffsetM(st.lat, st.lon, SalSpotLatDeg_, SalSpotLonDeg_, rE, rN);
  double rU = SalSpotElevM_ - st.elev;
  double rMag = std::sqrt(rE * rE + rN * rN + rU * rU);
  if (rMag < 1.0) rMag = 1.0;
  RangeM_ = rMag;
  double uE = rE / rMag, uN = rN / rMag, uU = rU / rMag;

  /* fb_fdm_state's velocity is X-Plane local (+x east, +y up, +z south). */
  double vE = st.vx, vN = -st.vz, vU = st.vy;
  double vMag = std::sqrt(vE * vE + vN * vN + vU * vU);
  ClosureMs_ = vMag;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, rE, rN, rU, LosAzDeg_, LosElDeg_);
  if (vMag < 1.0) return false;
  double wE = vE / vMag, wN = vN / vMag, wU = vU / vMag;

  double dot = Clamp(wE * uE + wN * uN + wU * uU, -1.0, 1.0);
  double errDeg = std::acos(dot) * kRad2Deg;
  LosRateDegS_ = errDeg;   /* what the law drives to zero, in this weapon's own currency */

  /* The component of the line of sight PERPENDICULAR to the velocity, in the ROUND's body frame: the
   * direction full canard deflection actually pushes it. */
  double pE = uE - dot * wE, pN = uN - dot * wN, pU = uU - dot * wU;
  double pFwd = 0.0, pRight = 0.0, pDown = 0.0;
  FBEnuToBodyVec(st.roll, st.pitch, st.yaw, pE, pN, pU, pFwd, pRight, pDown);

  /* THE RELAY IS RATE-STABILISED, and that is not a softening of the bang-bang: a relay switching on
   * position error alone is an oscillator, and every real bang-bang autopilot — the CCG included — takes
   * its switching signal off a RATE GYRO as well. The error is therefore projected one actuator lag
   * ahead with the round's own q and r, which are the two instruments it has. Without it the measured
   * result was a limit cycle that bled the round from 231 m/s to 92 m/s in ten seconds. */
  double ePitch = std::asin(Clamp(-pDown, -1.0, 1.0)) - kLaserRateLeadS * (st.q * kDeg2Rad);
  double eYaw = std::asin(Clamp(pRight, -1.0, 1.0)) - kLaserRateLeadS * (st.r * kDeg2Rad);
  double band = kLaserDeadBandDeg * kDeg2Rad;
  double pitch = 0.0, yaw = 0.0;
  /* EACH CHANNEL AGAINST ITS OWN DEAD BAND, and there is deliberately no test on the TOTAL misalignment
   * over the top of them. One used to sit here, and it vetoed both channels whenever `errDeg` dipped
   * inside the same 1.5 deg — a different quantity (the raw angle) against the same threshold the two
   * channels test their LED signals against, so it fired on a state neither channel could see and
   * silenced the one still asking for the stop. The cost was not the vetoed ticks but the FREQUENCY it
   * imposed: it dropped the relay out of its limit cycle about once per airframe short period (measured
   * on a constant-canard step: zeta 0.14, period 5 s, 12 s to settle), the round unloaded to alpha -8 deg
   * and every re-entry re-excited the mode, so the limit cycle sat ON the airframe instead of an order
   * above it. Per-channel it runs at ~3 Hz and the airframe integrates it: alpha 9..26 instead of
   * -8..26 deg, the standing pursuit error 3.00 instead of 3.92 deg, `lgb-designate` 14.45 m instead of
   * 52.78. doc/air-to-ground.md `C30`.
   * Body +up is -down, and a positive pitch command pulls +nz. Nothing between the stops. */
  if (ePitch > band) pitch = 1.0;
  else if (ePitch < -band) pitch = -1.0;
  if (eYaw > band) yaw = 1.0;
  else if (eYaw < -band) yaw = -1.0;
  FinPitch_ = pitch;
  FinYaw_ = yaw;
  c.ManualPitch = pitch;
  c.ManualYaw = yaw;
  c.ManualRoll = Clamp(-kRollGain * st.roll - kRollRateGain * st.p, -1.0, 1.0);
  NzCmdG_ = pitch;   /* the trace carries the COMMAND, and for this weapon the command IS the fin */
  NyCmdG_ = yaw;
  return true;
}

Pilot::FBPilotCommands FBMissileGuidance::Run(const FBState &state, FBCommandBus &avionics,
                                       const Systems::FBAirframeControls &airframe, const Fdm::fb_fdm_state &st,
                                       const FBFlightPlan &plan, const FBRunway *runway, double dt) {
  (void)avionics; (void)airframe; (void)plan; (void)runway; (void)dt;
  Pilot::FBPilotCommands c;
  c.Guidance = Pilot::FBPilotGuidance::Manual;
  /* A solid motor has no throttle: full command from separation, and FBFdm's own throttle slew is the
   * safe-separation delay before ignition. */
  c.ManualThr = 1.0;

  UpdateTarget(state, st);
  if (IrTerminal_) {
    double aE = 0.0, aN = 0.0, aU = 0.0;
    if (!AngleOnlyCommand(state, st, aE, aN, aU)) { FinPitch_ = FinYaw_ = 0.0; return c; }
    FlyCommand(c, st, aE, aN, aU, dt);
    return c;
  }
  /* THE ANTI-RADIATION ROUND FLIES ITS LAW EVEN WITH NOTHING ON THE RECEIVER, and that is the design:
   * with the rate zeroed the law commands the gravity bias alone, i.e. a straight coast. Branching out
   * to "fins centred" here would be the same trajectory by a second route. */
  if (Kind_ == FBSeekerKind::AntiRadiation) {
    /* A round that never acquired anything has no line at all; it flies the rail direction, which is
     * what an unguided coast IS. */
    if (ArSymbol_ == 0) { FinPitch_ = FinYaw_ = 0.0; c.ManualThr = 1.0; return c; }
    double aE = 0.0, aN = 0.0, aU = 0.0;
    (void)AntiRadiationCommand(state, st, aE, aN, aU);
    FlyCommand(c, st, aE, aN, aU, dt);
    return c;
  }
  if (Kind_ == FBSeekerKind::SemiActiveLaser) {
    /* A bomb has no motor: the throttle command is meaningless and the model has no engine to answer
     * it. Without a spot the canards TRAIL — which is a ballistic fall, not a failure path, and it is
     * exactly what a broken designation produces. */
    c.ManualThr = 0.0;
    if (!SalTerminal_ || !LaserCommand(state, st, c)) {
      FinPitch_ = FinYaw_ = 0.0;
      c.ManualPitch = c.ManualYaw = 0.0;
      c.ManualRoll = Clamp(-kRollGain * st.roll - kRollRateGain * st.p, -1.0, 1.0);
    }
    return c;
  }
  if (!HaveTarget_) {
    /* Fins centred, motor lit: it flies straight and the lifetime cap ends it. Not a special case,
     * just an absence of guidance. */
    FinPitch_ = FinYaw_ = 0.0;
    return c;
  }

  /* ---- geometry: the estimate, extrapolated to NOW at its last known velocity ---- */
  double ageS = NowS_ - TgtStampS_;
  if (ageS < 0.0) ageS = 0.0;
  double coslat = std::cos(st.lat * kDeg2Rad);
  double predLat = TgtLatDeg_ + TgtVelN_ * ageS / kMPerDeg;
  double predLon = TgtLonDeg_ + (coslat > 1e-6 ? TgtVelE_ * ageS / (kMPerDeg * coslat) : 0.0);
  double predAlt = TgtAltM_ + TgtVelU_ * ageS;

  double rE = 0.0, rN = 0.0;
  FBEnuOffsetM(st.lat, st.lon, predLat, predLon, rE, rN);
  double rU = predAlt - st.elev;
  double rMag = std::sqrt(rE * rE + rN * rN + rU * rU);
  if (rMag < 1.0) rMag = 1.0;
  RangeM_ = rMag;

  /* Own velocity in ENU. fb_fdm_state's vx/vy/vz are X-Plane local (+x east, +y up, +z south). */
  double vE = st.vx, vN = -st.vz, vU = st.vy;
  double relE = TgtVelE_ - vE, relN = TgtVelN_ - vN, relU = TgtVelU_ - vU;

  /* Vc = -(r . v_rel)/|r| ; Omega = (r x v_rel)/|r|^2 ; a_cmd = N*Vc*(Omega x r_hat). */
  double closure = -(rE * relE + rN * relN + rU * relU) / rMag;
  ClosureMs_ = closure;
  double omE = (rN * relU - rU * relN) / (rMag * rMag);
  double omN = (rU * relE - rE * relU) / (rMag * rMag);
  double omU = (rE * relN - rN * relE) / (rMag * rMag);
  LosRateDegS_ = std::sqrt(omE * omE + omN * omN + omU * omU) * kRad2Deg;

  double uE = rE / rMag, uN = rN / rMag, uU = rU / rMag;
  double gain = kNavConstant * (closure > 0.0 ? closure : 0.0);
  double aE = gain * (omN * uU - omU * uN);
  double aN = gain * (omU * uE - omE * uU);
  double aU = gain * (omE * uN - omN * uE);
  aU += kG;   /* gravity bias (class banner): PN says nothing about weight */

  FlyCommand(c, st, aE, aN, aU, dt);

  /* ---- the seeker: slave it to the estimate, switch it on at the activation range ---- */
  double losAz = 0.0, losEl = 0.0;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, rE, rN, rU, losAz, losEl);
  LosAzDeg_ = losAz; LosElDeg_ = losEl;
  if (Seeker_ && Spec_) {
    if (!SeekerLocked_) Seeker_->SlewTo(losAz, losEl);   /* SLAVE until it has its own track */
    /* SEMI-ACTIVE: no activation range, because there is nothing to activate. The head lives on the
     * SHOOTER's illumination and dies with it, once and for good — that is the entire tactical
     * difference between this round and an active one, and it is these four lines. */
    if (Kind_ == FBSeekerKind::SemiActiveRadar) {
      bool illuminated = state.Datalink.H.Readable() && state.Datalink.TrackCount > 0 &&
                         NowS_ - state.Datalink.Tracks[0].ReportTimeS <= kUplinkTimeoutS;
      if (!illuminated && !SarhDead_) {
        SarhDead_ = true;
        Seeker_->SetActive(false);
        FBLog::Info("missile", "ILLUMINATION_LOST", {{"tofS", NowS_ - LaunchS_}, {"rangeM", rMag},
            {"tgtAgeS", ageS}});
      } else if (illuminated && !SarhDead_ && !Seeker_->Active()) {
        Seeker_->SetActive(true);
        FBLog::Info("missile", "SEEKER_ACTIVE", {{"rangeM", rMag}, {"tofS", NowS_ - LaunchS_},
            {"losAzDeg", losAz}, {"losElDeg", losEl}, {"tgtAgeS", ageS}, {"mode", "semi-active"}});
      }
    } else if (!Seeker_->Active() && rMag <= Spec_->Perf.ActivationRangeM) {
      Seeker_->SetActive(true);
      FBLog::Info("missile", "SEEKER_ACTIVE", {{"rangeM", rMag}, {"tofS", NowS_ - LaunchS_},
          {"losAzDeg", losAz}, {"losElDeg", losEl}, {"tgtAgeS", ageS}});
    }
  }
  /* INFRARED: uncaged from the rail and slaved to the programming until it has its own mark. There is
   * no activation range — the detector was already looking when the pilot got his tone. */
  if (IrSeeker_ && Kind_ == FBSeekerKind::Infrared) {
    if (!IrSeeker_->Uncaged()) {
      IrSeeker_->SetUncaged(true);
      FBLog::Info("missile", "SEEKER_ACTIVE", {{"rangeM", rMag}, {"tofS", NowS_ - LaunchS_},
          {"losAzDeg", losAz}, {"losElDeg", losEl}, {"mode", "infrared"}});
    }
    if (!IrSeeker_->Locked()) IrSeeker_->SlewTo(losAz, losEl);
  }
  return c;
}

/* Everything BELOW the guidance law, shared by both of them: body-frame resolution of the commanded
 * acceleration, the two lateral loops on accelerometer + gyro, the roll holder, the fins. Which law
 * produced the command is not a question anything here asks. */
void FBMissileGuidance::FlyCommand(Pilot::FBPilotCommands &c, const Fdm::fb_fdm_state &st, double aE,
                                   double aN, double aU, double dt) {
  /* Into the body frame through the ONE shared rotation primitive — no second Euler spelling. */
  double aMag = std::sqrt(aE * aE + aN * aN + aU * aU);
  double aAz = 0.0, aEl = 0.0;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, aE, aN, aU, aAz, aEl);
  double aUpBody = aMag * std::sin(aEl * kDeg2Rad);        /* body +up = -Z */
  double aRightBody = aMag * std::cos(aEl * kDeg2Rad) * std::sin(aAz * kDeg2Rad);

  double nzCmd = Clamp(aUpBody / kG, -kMaxCommandG, kMaxCommandG);
  double nyCmd = Clamp(aRightBody / kG, -kMaxCommandG, kMaxCommandG);
  NzCmdG_ = nzCmd; NyCmdG_ = nyCmd;

  /* THE GATHERING PHASE, and it is the ONE thing a surface launch has that an air launch does not: for
   * the row's GatherS the fins TRAIL and the round flies the rail direction on thrust alone. The law
   * above still ran and its ask is already in the trace — `msl_nz_cmd` nonzero beside a zero
   * `msl_fin_pitch` IS the phase — but nothing steers, and the two integrators below therefore cannot
   * wind up against an airframe with no dynamic pressure to answer them. GatherS is zero for every
   * air-launched row, so this test is false for every store that ever left a pylon.
   * doc/modules/ground/module.md §4. */
  if (Spec_ && Spec_->GatherS > 0.0 && NowS_ - LaunchS_ < Spec_->GatherS) {
    FinPitch_ = FinYaw_ = 0.0;
    c.ManualPitch = c.ManualYaw = c.ManualRoll = 0.0;
    return;
  }

  /* ---- The two lateral-acceleration loops + the roll holder, closed on the round's own accelerometer
   * (st.nz/st.ny) and rate gyros (st.q/st.r): exactly the two instruments a missile has. ---- */
  double qbar = FBDynamicPressure(st.speed, st.elev);
  double scale = qbar > 1.0 ? kQRefPa / qbar : kGainScaleMax;
  scale = Clamp(scale, kGainScaleMin, kGainScaleMax);
  double kp = kFinPerG * kLoopP * scale;
  double ki = kFinPerG * kLoopI * scale;
  double kd = kRateGain * scale;
  double ez = nzCmd - st.nz, ey = nyCmd - st.ny;
  /* Integrate in FIN units and clamp there, so the anti-windup limit is the PHYSICAL one rather than a
   * number in error units that would have to be re-derived per gain. And CONDITIONALLY: a fin already
   * on its stop cannot answer more integral, so integrating into the stop only buys a reversal that
   * has to be unwound afterwards. doc/weapons.md 10.2. */
  double pitchCmd = kp * ez + NzInt_ - kd * (st.q * kDeg2Rad);
  double yawCmd = kp * ey + NyInt_ - kd * (st.r * kDeg2Rad);
  if (std::fabs(pitchCmd) < 1.0 || ez * pitchCmd < 0.0) {
    NzInt_ = Clamp(NzInt_ + ki * ez * dt, -kIntegralClamp, kIntegralClamp);
    pitchCmd = kp * ez + NzInt_ - kd * (st.q * kDeg2Rad);
  }
  if (std::fabs(yawCmd) < 1.0 || ey * yawCmd < 0.0) {
    NyInt_ = Clamp(NyInt_ + ki * ey * dt, -kIntegralClamp, kIntegralClamp);
    yawCmd = kp * ey + NyInt_ - kd * (st.r * kDeg2Rad);
  }
  double rollCmd = -kRollGain * st.roll - kRollRateGain * st.p;

  FinPitch_ = Clamp(pitchCmd, -1.0, 1.0);
  FinYaw_ = Clamp(yawCmd, -1.0, 1.0);
  c.ManualPitch = FinPitch_;
  c.ManualYaw = FinYaw_;
  c.ManualRoll = Clamp(rollCmd, -1.0, 1.0);
}

void FBMissileGuidance::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("msl_phase");            /* FBMissileGuidance::Phase ordinal */
  schema.Add("msl_range", "m");       /* to the ESTIMATE the law is flying at, never to the truth */
  schema.Add("msl_closure", "m/s");
  schema.Add("msl_losrate", "deg/s"); /* what PN is driving to zero */
  schema.Add("msl_los_az", "deg");    /* the estimate off the round's own nose */
  schema.Add("msl_los_el", "deg");
  schema.Add("msl_nz_cmd", "g");
  schema.Add("msl_ny_cmd", "g");
  schema.Add("msl_fin_pitch");        /* -1..1, what actually reached the fins */
  schema.Add("msl_fin_yaw");
  schema.Add("msl_seeker");           /* 0 = off, 1 = active/searching, 2 = locked */
  schema.Add("msl_tgt_age", "s");     /* since the last real measurement of the target */
  /* NO COLUMN FOR THE RECEIVED POWER, and the reason is a conservation collision rather than a
   * preference: `msl_*` is not the LAST source on a round's bus, so a column added here shifts every
   * column right of it in 95 already-measured files — which the append rule (units/FBSimUnit.cpp)
   * forbids. The number is not lost: this round's own head IS a warning receiver, so it publishes the
   * whole `rwr_*` group on the same trace, and the power is exactly (rwr_leth - base(rwr_mode))/0.15.
   * doc/air-to-ground.md §Gaps. */
}

void FBMissileGuidance::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push((int)Phase_);
  row.Push(RangeM_);
  row.Push(ClosureMs_);
  row.Push(LosRateDegS_);
  row.Push(LosAzDeg_);
  row.Push(LosElDeg_);
  row.Push(NzCmdG_);
  row.Push(NyCmdG_);
  row.Push(FinPitch_);
  row.Push(FinYaw_);
  /* One column, five seekers: whichever one this round has answers it. */
  if (Kind_ == FBSeekerKind::Infrared)
    row.Push(IrTerminal_ ? 2 : (IrSeeker_ && IrSeeker_->Uncaged() ? 1 : 0));
  else if (Kind_ == FBSeekerKind::AntiRadiation)
    row.Push(ArSymbol_ != 0 ? (ArLost_ ? 1 : 2) : 1);   /* a passive head is never OFF */
  else if (Kind_ == FBSeekerKind::SemiActiveLaser)
    row.Push(SalTerminal_ ? 2 : 1);
  else
    row.Push(SeekerLocked_ ? 2 : (Seeker_ && Seeker_->Active() ? 1 : 0));
  /* THE MEMORY CLOCK, read straight out of the column that has always meant "since the last real
   * measurement" — for this round it is the number the whole escape window is made of. */
  if (Kind_ == FBSeekerKind::Infrared) row.Push(IrTerminal_ ? NowS_ - IrLosStampS_ : -1.0);
  else if (Kind_ == FBSeekerKind::AntiRadiation) row.Push(ArLastLookS_ > -1e8 ? NowS_ - ArLastLookS_ : -1.0);
  else row.Push(HaveTarget_ ? NowS_ - TgtStampS_ : -1.0);
}

} // namespace FlightBox::Modules
