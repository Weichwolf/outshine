#include "FBSiteModule.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace FlightBox::Modules {

namespace {

bool ParseDouble(const std::string &s, double &out) {
  if (s.empty()) return false;
  char *end = nullptr;
  errno = 0;
  double v = std::strtod(s.c_str(), &end);
  if (end != s.c_str() + s.size()) return false;
  if (errno == ERANGE || !std::isfinite(v)) return false;
  out = v;
  return true;
}

bool RejectSetup(const char *reason, const std::string &key, const std::string &value) {
  FBLog::Error("module", "SET_INVALID_VALUE", {{"key", key}, {"value", value}, {"reason", reason}});
  return false;
}

/* THE ONE STATION a position has. A launcher's rail is not a pylon: the offsets are zero because the
 * round leaves the mount, not a wing, and its attitude is the RAIL's (FBStoresSystem::SetRailAttitude)
 * rather than the mount's body attitude. */
constexpr int kRailStation = 1;

} // namespace

FBSiteModule::FBSiteModule(const FBSiteSpec &spec) : Spec_(spec) {
  /* THE ACQUISITION SET: 360 degrees of azimuth and one antenna revolution per FrameS. AutoAcquire is
   * false — this set FINDS, it does not lock; locking is the fire control's job and a separate antenna. */
  Sensors::FBRadarScanVolume sv;
  sv.AzCenterDeg = 0.0;
  sv.AzHalfDeg = Spec_.SearchAzHalfDeg;
  sv.ElCenterDeg = Spec_.SearchElCenterDeg;
  sv.ElHalfDeg = Spec_.SearchElHalfDeg;
  sv.RangeM = Spec_.SearchRangeM;
  sv.FrameS = Spec_.SearchFrameS;
  sv.AutoAcquire = false;
  sv.SingleTarget = false;
  sv.Active = Spec_.SearchRangeM > 0.0;
  Search_.SetSearchVolume(sv);
  Search_.Configure(FBEmitterKind::SurfaceEarlyWarning, Spec_.DopplerNotchMs, Spec_.NotchRejects);
  Search_.SetPowered(false);   /* the engagement machine decides when this position radiates */
  Search_.SetIffInterrogator(false);   /* a battery has no IFF interrogator in this tree */
  Search_.SetIffTransponder(false);

  /* THE FIRE-CONTROL SET: a slewed sector that collapses to a pencil once it has something, and it
   * AutoAcquires — a set trained on a bearing takes the first firm return in it. That is the whole of
   * "designation" here, and it is a DIRECTION crossing between two sensors, never an identity. */
  Sensors::FBRadarScanVolume tv;
  tv.AzCenterDeg = 0.0;
  tv.AzHalfDeg = Spec_.TrackAzHalfDeg;
  tv.ElCenterDeg = 0.0;
  tv.ElHalfDeg = Spec_.TrackElHalfDeg;
  tv.RangeM = Spec_.TrackRangeM;
  tv.FrameS = Spec_.TrackFrameS;
  tv.AutoAcquire = true;
  tv.SingleTarget = true;
  tv.Active = Spec_.TrackRangeM > 0.0;
  Track_.SetSearchVolume(tv);
  Track_.Configure(FBEmitterKind::SurfaceFireControl, Spec_.DopplerNotchMs, Spec_.NotchRejects);
  Track_.SetPowered(false);
  Track_.SetIffInterrogator(false);
  Track_.SetIffTransponder(false);

  /* THE EYE. Its reach is not a setting — it falls out of the inequality in sensors/FBVisualSystem —
   * but its FIELD is: a gunner turns his head, an antenna does not. */
  if (Spec_.OpticalAzHalfDeg > 0.0) Optics_.SetCone(Spec_.OpticalAzHalfDeg, Spec_.OpticalElHalfDeg);
  Optics_.SetPowered(Spec_.OpticalAzHalfDeg > 0.0);

  /* THE PASSIVE RECEIVER, and it is the whole of `emcon hold`: a position comes up on something it
   * MEASURED. Its elevation coverage is the generic one; a cue is a bearing and a power, never a range. */
  Esm_.SetPowered(true);

  Irst_.SetPowered(false);
  /* THE TERMINAL, off until a mission puts this position on a net. A ground unit hears another ground
   * unit only because the slot was told that a `Ground` kind carries one — the default is Aircraft
   * alone, which is why the class as built could not carry a ground net at all. */
  Datalink_.SetPowered(false);
  Datalink_.SetCarriesTerminal(/*aircraft*/ false, /*ground*/ true);

  if (Spec_.Store != FBStoreKind::None) {
    const FBStoreSpec *store = FBStoreSpecOf(Spec_.Store);
    Rails_.DeclareStation(kRailStation, 0.0, 0.0, 0.0);
    (void)Rails_.DeclareGroundLauncher();   /* it has no wheels: see FBStoresSystem::DeclareGroundLauncher */
    if (store) (void)Rails_.Load(kRailStation, *store);
  }
  if (Spec_.Gun != FBGunKind::None) {
    const FBGunSpec *gun = FBGunSpecOf(Spec_.Gun);
    if (gun) {
      Gun_.Install(*gun, 0.0, 0.0, 0.0, /*boreDownDeg*/ 0.0, /*boreRightDeg*/ 0.0);
      Gun_.DeclareGroundMount();
      (void)Gun_.SetRounds(Spec_.RoundsDefault <= gun->Capacity ? Spec_.RoundsDefault : gun->Capacity);
    }
  }
  Fc_.Bind(Spec_, TrackBus_);
}

/* One decision tick per Run(): the sets keep their OWN absolute frame grids, so how often this module
 * cycles them changes nothing they report. Order is data flow — receiver, then the two antennas, then
 * the eye, then the machine that reads all four, then the boxes it commanded. */
void FBSiteModule::Run(Fdm::fb_fdm_state &st, double dt, const Units::FBUnitRegistry *units,
                       const World::FBWorld *world) {
  (void)world;
  SimTimeS_ += dt;
  State_.NowS = SimTimeS_;
  TrackBus_.NowS = SimTimeS_;

  /* THE DAMAGE PATTERN, identical to every airframe's: a FAILED system is not cycled at all and its
   * block goes Invalid, so its emission stops by the same coupling and not by a line written here. */
  bool radarOk = SystemWorking(FBSystemId::Radar);
  double rangeFactor = SystemDegraded(FBSystemId::Radar) ? kRadarRangeDegraded : 1.0;
  Search_.SetRangeFactor(rangeFactor);
  Track_.SetRangeFactor(rangeFactor);

  if (SystemWorking(FBSystemId::Rwr)) Esm_.Run(State_, st, units, SimTimeS_);
  else State_.Rwr.H.Invalidate();

  /* THE NET, before the antennas that a cue may move. A position on no net never touches this slot at
   * all, which is what keeps its telemetry and its events identical to the pre-net binary's. */
  if (NetTerminal_) {
    Datalink_.SetPowered(SystemWorking(FBSystemId::Datalink) && SystemWorking(FBSystemId::Structure));
    Datalink_.Run(State_, st, units, SimTimeS_);
  }

  Search_.SetPowered(radarOk && Fc_.SearchRadiating());
  Track_.SetPowered(radarOk && Fc_.TrackRadiating());
  if (Fc_.HaveAim()) Track_.SlewTo(Fc_.AimAzDeg(), Fc_.AimElDeg());
  if (radarOk) {
    Search_.Run(State_, st, units, SimTimeS_);
    Track_.Run(TrackBus_, st, units, SimTimeS_);
  } else {
    State_.Radar.H.Invalidate();
    TrackBus_.Radar.H.Invalidate();
  }

  Optics_.Run(State_, st, units, SimTimeS_);

  if (SystemWorking(FBSystemId::FireControl)) {
    /* The one number `emcon react` runs on, handed across rather than reached for: this position's own
     * hit count, out of the register the module may read and nobody may write. */
    Fc_.NoteOwnHits(OwnHits());
    (void)Fc_.Run(State_, Cmds_, Ctrl_, st, Plan_, nullptr, dt);
    /* WHERE THE RAIL POINTS and WHERE THE BARRELS POINT: the fire control's aim, applied to the
     * hardware it commands. A mount publishes roll = pitch = 0, so the launcher's elevation cannot be
     * read off the unit's pose and travels with the release instead. */
    if (Fc_.HaveAim()) {
      /* A FIXED LAUNCHER has a rail elevation and the catalogue states it; a SHOULDER-FIRED TUBE is
       * pointed where the gunner is looking, which is a different fact about a different weapon. The
       * discriminator is whether the position has a tracking radar at all: if it does not, the man IS
       * the fire control. */
      double railPitch = Spec_.TrackRangeM > 0.0 ? Spec_.LaunchElevDeg : Fc_.AimElDeg();
      Rails_.SetRailAttitude(railPitch, st.yaw + Fc_.AimAzDeg());
      Gun_.SetBore(-Fc_.AimElDeg(), Fc_.AimAzDeg());
    }
    Rails_.SetTargetState(Fc_.Target());
  }

  ServiceCommands(FBCommandGroup::Sensors);    /* the two entries a net cue costs */
  ServiceCommands(FBCommandGroup::Avionics);   /* the master arm */
  ServiceCommands(FBCommandGroup::Stores);     /* the pickle and the trigger */

  /* THE NEXT ROUND ON THE RAIL. The rail is ONE station and a magazine is many rounds; how many can
   * leave before the crew has to work is the row's RailCount, held by the engagement machine. Reloading
   * here rather than in the machine keeps the machine free of the SMS's station arithmetic. */
  if (Spec_.Store != FBStoreKind::None && Rails_.LoadedCount() == 0 && Fc_.RoundsLeft() > 0 &&
      Fc_.RailsLeft() > 0) {
    const FBStoreSpec *store = FBStoreSpecOf(Spec_.Store);
    if (store) (void)Rails_.Load(kRailStation, *store);
  }

  /* LAST, and that order is load-bearing: the SMS publishes the UPLINK a command-guided or semi-active
   * round lives on, and a round released this tick is stepped in the NEXT one — so the uplink has to be
   * live before this tick's pose barrier, or every round would be born with its support already lost
   * (measured: `missile ILLUMINATION_LOST` at tofS = 0.01). */
  if (SystemWorking(FBSystemId::Stores)) Rails_.Run(State_, dt);
  else State_.Stores.H.Invalidate();
  if (SystemWorking(FBSystemId::Gun)) Gun_.Run(State_, st, SimTimeS_, dt);
  else State_.Gun.H.Invalidate();
}

void FBSiteModule::ServiceCommands(FBCommandGroup group) {
  FBAvionicsCommand c{};
  while (Cmds_.TakeDue(group, SimTimeS_, c)) {
    FBCommandOutcome outcome = FBCommandOutcome::Accepted;
    FBCommandReason reason = FBCommandReason::None;
    ApplyCommand(c, outcome, reason);
    Cmds_.Complete(c, outcome, reason, SimTimeS_);
  }
}

/* Four targets and no more: a position has a master arm, a rail and a trigger. Everything else a jet's
 * router answers is a box this thing does not have, and saying so is the point. */
void FBSiteModule::ApplyCommand(const FBAvionicsCommand &c, FBCommandOutcome &outcome,
                                FBCommandReason &reason) {
  switch (c.Target) {
    /* THE CUE, arriving as what it is: an antenna azimuth and an antenna elevation, typed in, each with
     * its own latency and each refusable. Nothing about a track crosses this seam. */
    case FBCommandTarget::RadarSlewAz:
    case FBCommandTarget::RadarSlewEl:
      if (!NetTerminal_) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::OutOfContext;
        return;
      }
      Fc_.NoteNetAimApplied(c.Target == FBCommandTarget::RadarSlewAz, c.Value);
      return;
    case FBCommandTarget::MasterArm:
      Rails_.SetMasterArm(c.Value != 0.0 ? FBArmState::Arm : FBArmState::Sim);
      Gun_.SetMasterArm(Rails_.MasterArm());
      return;
    case FBCommandTarget::WeaponRelease:
      if (!SystemWorking(FBSystemId::Stores)) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::SystemFailed;
        return;
      }
      Rails_.Release(SimTimeS_, outcome, reason);
      return;
    case FBCommandTarget::GunTrigger:
      if (!SystemWorking(FBSystemId::Gun)) {
        outcome = FBCommandOutcome::Rejected;
        reason = FBCommandReason::SystemFailed;
        return;
      }
      Gun_.Trigger(c.Value, SimTimeS_, outcome, reason);
      return;
    default:
      outcome = FBCommandOutcome::Rejected;
      reason = FBCommandReason::NotImplemented;
      return;
  }
}

bool FBSiteModule::ApplySetup(const std::string &key, const std::string &value) {
  if (key == "emcon") {
    /* `<free|hold|react> [offS [onS]]` — one key, one value word and an OPTIONAL briefed plan behind
     * it. The C1 contract's six author-facing keys stay six. doc/air-to-ground.md §5.1. */
    std::istringstream ls(value);
    std::string mode;
    double offS = 0.0, onS = 0.0;
    if (!(ls >> mode) || (mode != "free" && mode != "hold" && mode != "react"))
      return RejectSetup("want free|hold|react [offS [onS]]", key, value);
    if (ls >> offS) {
      if (offS <= 0.0) return RejectSetup("the emission plan's stop must be positive", key, value);
      if ((ls >> onS) && onS <= offS)
        return RejectSetup("the emission plan's resume must come after its stop", key, value);
    }
    Fc_.SetEmconHold(mode == "hold");
    Fc_.SetEmconReact(mode == "react");
    Fc_.SetEmconPlan(offS, onS);
    return true;
  }
  if (key == "alert") {
    if (value != "ready" && value != "cold") return RejectSetup("want ready|cold", key, value);
    Fc_.SetColdStart(value == "cold");
    return true;
  }
  if (key == "rounds") {
    double n = 0.0;
    if (!ParseDouble(value, n)) return RejectSetup("not a number", key, value);
    if (n < 0.0 || n != std::floor(n)) return RejectSetup("want a whole, non-negative count", key, value);
    if (Spec_.Store == FBStoreKind::None && Spec_.Gun == FBGunKind::None && n > 0.0)
      return RejectSetup("this position has no weapon", key, value);
    Fc_.SetRounds((int)n);
    if (Spec_.Gun != FBGunKind::None && !Gun_.SetRounds((int)n))
      return RejectSetup("more rounds than the gun holds", key, value);
    return true;
  }
  if (key == "engage_max_m") {
    double m = 0.0;
    if (!ParseDouble(value, m)) return RejectSetup("not a number", key, value);
    if (m <= 0.0) return RejectSetup("want a positive range", key, value);
    if (m > Spec_.EnvMaxM)
      return RejectSetup("a mission may clamp the published envelope DOWN, never up", key, value);
    Fc_.SetEngageMaxM(m);
    return true;
  }
  if (key == "reaction_s") {
    double s = 0.0;
    if (!ParseDouble(value, s)) return RejectSetup("not a number", key, value);
    if (s < 0.0) return RejectSetup("negative reaction time", key, value);
    Fc_.SetReactionS(s);
    return true;
  }
  if (key == "scoot_s") {
    double s = 0.0;
    if (!ParseDouble(value, s)) return RejectSetup("not a number", key, value);
    if (s < 0.0) return RejectSetup("negative scoot time", key, value);
    Fc_.SetScootS(s);
    return true;
  }

  /* ---- THE RUNNER-GENERATED NET KEYS. Not author-written and not part of the SIX the C1 contract
   * declares: they are produced from the mission's `net` block, because membership, sector, fallback
   * and link kind are properties of the NET and not of the position.
   * doc/air-defence-network.md §8. ---- */
  if (key == "net_link") {
    std::istringstream ls(value);
    std::string kind;
    double rangeM = 0.0, mastM = 0.0;
    if (!(ls >> kind)) return RejectSetup("want wire|radio", key, value);
    if (kind == "wire") {
      if (!(ls >> mastM)) return RejectSetup("want 'wire <mastM>'", key, value);
      Datalink_.SetLinkMode(Sensors::FBDatalinkSystem::LinkMode::Wire, mastM);
    } else if (kind == "radio") {
      if (!(ls >> rangeM >> mastM) || rangeM <= 0.0)
        return RejectSetup("want 'radio <rangeM> <mastM>'", key, value);
      Datalink_.SetLinkMode(Sensors::FBDatalinkSystem::LinkMode::Radio, mastM);
      Datalink_.SetMaxRangeM(rangeM);
    } else {
      return RejectSetup("want wire|radio", key, value);
    }
    NetTerminal_ = true;
    return true;
  }
  if (key == "net_period_s") {
    double s = 0.0;
    if (!ParseDouble(value, s) || s <= 0.0) return RejectSetup("want positive seconds", key, value);
    Datalink_.SetNetPeriodS(s);
    return true;
  }
  if (key == "net_hold") {
    double n = 0.0;
    if (!ParseDouble(value, n) || n <= 0.0) return RejectSetup("want a positive cycle count", key, value);
    Datalink_.SetHoldCycles(n);
    return true;
  }
  if (key == "net_sector") {
    std::istringstream ls(value);
    double centre = 0.0, half = 0.0;
    if (!(ls >> centre >> half) || half <= 0.0)
      return RejectSetup("want '<centreDeg> <halfDeg>'", key, value);
    Fc_.SetSector(centre, half);
    return true;
  }
  if (key == "net_autonomy") {
    FBWeaponsControl w = FBWeaponsControl::Hold;
    if (!FBWeaponsControlFromString(value.c_str(), w)) return RejectSetup("want free|tight|hold", key, value);
    Fc_.SetAutonomy(w);
    return true;
  }
  if (key == "net_control") {
    Fc_.SetControlNode(value);
    Datalink_.SetControlNode(value);   /* logging only — WHY the node went quiet, never a decision */
    return true;
  }
  if (key == "net_wcs") {
    FBWeaponsControl w = FBWeaponsControl::Free;
    if (!FBWeaponsControlFromString(value.c_str(), w)) return RejectSetup("want free|tight|hold", key, value);
    Fc_.SetNodeWcs(w);
    return true;
  }
  return false;   /* unknown key: the boot path logs SET_REJECTED with the key AND value */
}

} // namespace FlightBox::Modules
