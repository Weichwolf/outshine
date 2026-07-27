#include "FBStoresSystem.h"
#include "FBLog.h"
#include "FBUnits.h"

namespace FlightBox {

namespace {
constexpr double kInToM = 0.0254;
} // namespace

void FBStoresSystem::DeclareStation(int number, double xIn, double yIn, double zIn) {
  if (Count_ >= kMaxStations || number < 1) return;
  Station &s = Stations_[Count_++];
  s.XIn = xIn; s.YIn = yIn; s.ZIn = zIn;
  StationNumber_[Count_ - 1] = number;
}

int FBStoresSystem::IndexOf(int station) const {
  for (int i = 0; i < Count_; i++)
    if (StationNumber_[i] == station) return i;
  return -1;
}

void FBStoresSystem::AttachFdm(FBFdm &fdm) {
  Fdm_ = &fdm;
  for (int i = 0; i < Count_; i++) {
    char name[24];
    snprintf(name, sizeof name, "station%d", StationNumber_[i]);
    Stations_[i].PointMass = fdm.AddStorePointMass(name, Stations_[i].XIn, Stations_[i].YIn,
                                                   Stations_[i].ZIn);
  }
}

bool FBStoresSystem::Load(int station, const FBStoreSpec &spec) {
  int i = IndexOf(station);
  if (i < 0 || Stations_[i].Kind != FBStoreKind::None) return false;
  Stations_[i].Kind = spec.Kind;
  Stations_[i].MassLbs = spec.MassLbs;
  Stations_[i].DragAreaFt2 = spec.DragAreaFt2;
  if (Selected_ < 0) Selected_ = station;
  PublishLoadout();
  return true;
}

int FBStoresSystem::LoadedCount() const {
  int n = 0;
  for (int i = 0; i < Count_; i++)
    if (Stations_[i].Kind != FBStoreKind::None) n++;
  return n;
}

double FBStoresSystem::LoadedMassLbs() const {
  double m = 0.0;
  for (int i = 0; i < Count_; i++)
    if (Stations_[i].Kind != FBStoreKind::None) m += Stations_[i].MassLbs;
  return m;
}

FBStoreKind FBStoresSystem::StoreAt(int station) const {
  int i = IndexOf(station);
  return i < 0 ? FBStoreKind::None : Stations_[i].Kind;
}

bool FBStoresSystem::SelectStation(int station) {
  int i = IndexOf(station);
  if (i < 0 || Stations_[i].Kind == FBStoreKind::None) return false;
  Selected_ = station;
  return true;
}

/* Station step: the next LOADED station after the one just emptied, wrapping — the same "weapon/station
 * step" a real SMS does so a ripple does not need the pilot to reselect between releases. */
void FBStoresSystem::SelectNextLoaded() {
  int from = IndexOf(Selected_);
  for (int k = 1; k <= Count_; k++) {
    int i = (from + k) % (Count_ > 0 ? Count_ : 1);
    if (Stations_[i].Kind != FBStoreKind::None) { Selected_ = StationNumber_[i]; return; }
  }
  Selected_ = -1;
}

/* The whole loadout, restated to the engine: each station's point mass, and the aggregate drag area at
 * the loaded stations' CENTROID (an empty jet: zero area, no force). Weight-weighted centroid rather
 * than a fixed point, so an asymmetric load's drag produces the yaw/roll moment it physically should —
 * out of JSBSim's own force model, not out of a term invented here. */
void FBStoresSystem::PublishLoadout() {
  if (!Fdm_) return;
  double cda = 0.0, wx = 0.0, wy = 0.0, wz = 0.0, w = 0.0;
  for (int i = 0; i < Count_; i++) {
    const Station &s = Stations_[i];
    double lbs = s.Kind != FBStoreKind::None ? s.MassLbs : 0.0;
    Fdm_->SetStorePointMassLbs(s.PointMass, lbs);
    if (s.Kind == FBStoreKind::None) continue;
    cda += s.DragAreaFt2;
    wx += s.XIn * lbs; wy += s.YIn * lbs; wz += s.ZIn * lbs; w += lbs;
  }
  if (w > 0.0) Fdm_->SetStoresDrag(cda, wx / w, wy / w, wz / w);
  else Fdm_->SetStoresDrag(0.0, 0.0, 0.0, 0.0);
}

bool FBStoresSystem::Release(double nowS, FBCommandOutcome &outcome, FBCommandReason &reason) {
  auto refuse = [&](FBCommandReason r, const char *why) {
    outcome = FBCommandOutcome::Rejected;
    reason = r;
    FBLog::Warn("sms", "RELEASE_REJECTED", {{"reason", FBCommandReasonStr(r)}, {"detail", why},
                                            {"station", Selected_}, {"arm", MasterArm() == FBArmState::Arm}});
    return false;
  };
  /* The two hardware interlocks first, in the order the jet checks them: the master arm switch is the
   * pilot's own safety, weight-on-wheels is the airframe's. Neither is something software may talk its
   * way past (doc/f16/controls-commands.md §6.2). */
  if (Arm_ != FBArmState::Arm)
    return refuse(FBCommandReason::HardwarePrecedence, "master arm not in ARM");
  if (Wow_)
    return refuse(FBCommandReason::HardwarePrecedence, "weight on wheels");
  int i = IndexOf(Selected_);
  if (i < 0 || Stations_[i].Kind == FBStoreKind::None)
    return refuse(FBCommandReason::OutOfContext, "no loaded station selected");
  if (PendingCount_ >= kMaxPendingReleases)
    return refuse(FBCommandReason::ChannelBusy, "release queue not drained");

  /* The WEAPON-SPECIFIC interlocks, after the two hardware ones and before anything leaves the rail. A
   * guided round is not a bomb: it needs a target to be programmed with and a launch zone to be inside,
   * and both are the fire control's answer (cached in Run from the bus block it publishes), not the
   * SMS's opinion. Refusing here rather than launching a round that cannot arrive is the same
   * valid-command-wrong-context case an unselected station is (doc/f16/controls-commands.md §6.5). */
  const FBStoreSpec *rspec = FBStoreSpecOf(Stations_[i].Kind);
  if (rspec && rspec->RequiresLock) {
    if (!RadarLocked_ || !Target_.Valid)
      return refuse(FBCommandReason::OutOfContext, "no fire-control lock");
    if (!HaveZone_)
      return refuse(FBCommandReason::OutOfContext, "no launch-zone solution");
    if (!InZone_) {
      FBLog::Warn("sms", "LAUNCH_OUT_OF_ZONE", {{"rangeM", ZoneRangeM_}, {"rminM", ZoneMinM_},
                                                {"raeroM", ZoneMaxM_}});
      return refuse(FBCommandReason::OutOfContext,
                    ZoneRangeM_ > ZoneMaxM_ ? "target beyond Raero" : "target inside Rmin");
    }
  }

  Station &s = Stations_[i];
  FBStoreRelease &rel = Pending_[PendingCount_++];
  rel.Station = Selected_;
  rel.Kind = s.Kind;
  rel.MassLbs = s.MassLbs;
  rel.SimTimeS = nowS;
  /* Structural (x aft, y right, z up, inches, origin at the model's datum) -> body (fwd/right/down,
   * metres) RELATIVE TO THE CG, which is the frame the release point is built in: JSBSim's own
   * structural-to-body convention (FGMassBalance::StructuralToBody), applied here because this class is
   * the only one that knows where the pylon is. */
  double cgXIn = Fdm_ ? Fdm_->GetCgXIn() : s.XIn;
  rel.OffFwdM = (cgXIn - s.XIn) * kInToM;
  rel.OffRightM = s.YIn * kInToM;
  rel.OffDownM = -s.ZIn * kInToM;
  /* The launch programming (core/FBStore.h's FBStoreRelease): a guided round leaves the rail knowing
   * where the shooter's fire control last saw the target and whose uplink to listen to for corrections.
   * An unguided store carries an invalid target and launcher 0, which is what it is. */
  if (rspec && rspec->Guided) {
    rel.LauncherId = SelfId_;
    rel.Target = Target_;
    GuidedInFlight_++;
  } else {
    /* The unguided half of the same programming: an UNGUIDED round is aimed by the computer, not by a
     * seeker, so what leaves with it is the solution it was aimed with (see SetReleaseSolution). */
    rel.Solution = Solution_;
  }

  /* The gross weight AT the release, i.e. before this change reaches the engine: FGMassBalance sums the
   * point masses in its own Run, so the new weight exists one step later and is read from the telemetry
   * column (sms_gw_lbs), not from here. Logging a "before" that the next step contradicts would be a
   * measurement of nothing. */
  double gwLbs = Fdm_ ? Fdm_->GetWeightLbs() : 0.0;
  double storesBefore = LoadedMassLbs();
  s.Kind = FBStoreKind::None;
  s.MassLbs = 0.0;
  s.DragAreaFt2 = 0.0;
  Released_++;
  SelectNextLoaded();
  PublishLoadout();   /* the mass is off the jet from this step on — see the class banner */
  outcome = FBCommandOutcome::Accepted;
  reason = FBCommandReason::None;
  const FBStoreSpec *spec = FBStoreSpecOf(rel.Kind);
  FBLog::Info("sms", "RELEASE", {{"station", rel.Station}, {"store", spec ? spec->Key : "?"},
      {"storeLbs", rel.MassLbs}, {"gwLbs", gwLbs},
      {"storesLbsBefore", storesBefore}, {"storesLbsAfter", LoadedMassLbs()},
      {"remaining", LoadedCount()}, {"nextStation", Selected_}});
  /* A guided round's launch record carries the SOLUTION it was fired on — the fire control's own launch
   * zone and its two countdowns, at the moment of launch. It is a separate line rather than more fields
   * on RELEASE because it exists only for a guided round, and because it is the prediction the flown
   * result is later measured against (the DLZ's error is a real property, not a bug to hide). */
  if (rspec && rspec->Guided)
    FBLog::Info("sms", "LAUNCH_SOLUTION", {{"store", spec ? spec->Key : "?"},
        {"rangeM", ZoneRangeM_}, {"rminM", ZoneMinM_}, {"raeroM", ZoneMaxM_}, {"rtrM", ZoneRtrM_},
        {"ttaS", ZoneTtaS_}, {"ttiS", ZoneTtiS_},
        {"tgtLat", Target_.LatDeg}, {"tgtLon", Target_.LonDeg}, {"tgtAltM", Target_.AltM},
        {"tgtAgeS", nowS - Target_.StampS}});
  /* ...and the unguided one's, for the same reason and in the same shape: what the computer PREDICTED
   * at the instant of the pickle. The runner puts it beside the impact it then measures. */
  if (rspec && !rspec->Guided && rel.Solution.Valid)
    FBLog::Info("sms", "RELEASE_SOLUTION", {{"store", spec ? spec->Key : "?"},
        {"mode", FBDeliveryModeStr(rel.Solution.Mode)},
        {"predLat", rel.Solution.ImpactLatDeg}, {"predLon", rel.Solution.ImpactLonDeg},
        {"planeM", rel.Solution.ImpactElevM}, {"predTofS", rel.Solution.TofS},
        {"aimLat", rel.Solution.AimLatDeg}, {"aimLon", rel.Solution.AimLonDeg},
        {"aimMissM", rel.Solution.AimMissM}, {"armMarginS", rel.Solution.ArmMarginS},
        {"solAgeS", nowS - rel.Solution.StampS}});
  return true;
}

bool FBStoresSystem::TakeRelease(FBStoreRelease &out) {
  if (PendingCount_ <= 0) return false;
  out = Pending_[0];
  for (int i = 1; i < PendingCount_; i++) Pending_[i - 1] = Pending_[i];
  PendingCount_--;
  return true;
}

void FBStoresSystem::Run(FBState &state, double dt) {
  (void)dt;
  /* The weight-on-wheels interlock reads the airframe block like any other consumer — an unpublished
   * one leaves the last known value rather than inventing "airborne". */
  if (state.Airframe.H.Readable()) Wow_ = state.Airframe.WeightOnWheels;

  /* The fire control's answer, cached for the pickle (which arrives from the command bus between ticks,
   * not inside one) — read like any other consumer reads a block, head first. */
  RadarLocked_ = state.Radar.H.Readable() && state.Radar.LockIndex >= 0;
  HaveZone_ = state.FireControl.H.Readable() && state.FireControl.DlzValid;
  if (HaveZone_) {
    InZone_ = state.FireControl.InZone;
    ZoneRangeM_ = state.FireControl.TargetRangeM;
    ZoneMinM_ = state.FireControl.RminM;
    ZoneMaxM_ = state.FireControl.RaeroM;
    ZoneRtrM_ = state.FireControl.RtrM;
    ZoneTtaS_ = state.FireControl.TimeToActiveS;
    ZoneTtiS_ = state.FireControl.TimeToImpactS;
  } else {
    InZone_ = false;
  }

  /* WHAT THIS AIRCRAFT RADIATES to a round in flight. A launcher supports its shot for exactly as long
   * as its fire control still holds the target: lose the lock and the uplink stops mid-flight, which is
   * the case the whole midcourse/terminal split exists for. Nothing here decides what the missile then
   * does — the missile does (modules/missile/FBMissileGuidance). */
  Uplink_.Active = GuidedInFlight_ > 0 && Target_.Valid;
  Uplink_.LauncherId = SelfId_;
  Uplink_.Target = Target_;
  FBStoresBlock &b = state.Stores;
  b.Arm = Arm_;
  b.StationCount = Count_;
  b.SelectedStation = Selected_;
  b.LoadedCount = LoadedCount();
  b.LoadedLbs = (float)LoadedMassLbs();
  b.ReleasedCount = Released_;
  for (int i = 0; i < kMaxStations; i++)
    b.Station[i] = i < Count_ ? (uint8_t)Stations_[i].Kind : (uint8_t)FBStoreKind::None;
  b.H.Publish(state.NowS);
}

void FBStoresSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("sms_arm");            /* 1 = master arm ARM */
  schema.Add("sms_station");        /* selected station, -1 = none */
  schema.Add("sms_loaded");         /* stores still on the jet */
  schema.Add("sms_lbs", "lb");      /* their total weight — the SMS's own books */
  schema.Add("sms_released");
  /* The airframe's gross weight, in the SAME row as the books above: this is the one place the mass a
   * release TOOK OFF and the mass the AIRFRAME then has can be read against each other, which is what
   * makes the carriage effect measurable rather than asserted. */
  schema.Add("sms_gw_lbs", "lb");
}

void FBStoresSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(Arm_ == FBArmState::Arm ? 1 : 0);
  row.Push(Selected_);
  row.Push(LoadedCount());
  row.Push(LoadedMassLbs());
  row.Push(Released_);
  row.Push(Fdm_ ? Fdm_->GetWeightLbs() : 0.0);
}

} // namespace FlightBox
