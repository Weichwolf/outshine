#include "FBSimUnit.h"
#include "FBElevationProvider.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cassert>

namespace FlightBox::Units {

FBSimUnit::FBSimUnit(int id, std::string name, FBUnitKind kind, FBUnitTeam team,
                     std::unique_ptr<Fdm::FBFdm> fdm, std::unique_ptr<Modules::FBModule> module,
                     const Fdm::fb_fdm_state &initialState, double groundAslM, FBFlightId flight)
    : FBUnit(id, std::move(name), kind, team, std::move(flight)),
      Fdm_(std::move(fdm)),
      Module_(std::move(module)),
      St_(initialState),
      GroundAslM_(groundAslM),
      FdmSrc_(Fdm_.get(), St_, GroundAslM_),
      BusSrc_(Module_->Telemetry()),
      HealthSrc_(Health_) {
  assert(Module_ && "FBSimUnit needs the module that flies the airframe");
  Module_->SetUnitIdentity(GetId(), GetTeam());   /* before the first Run */
  Module_->SetFlight(GetFlight());                /* ...and which flight it flies in, same wiring step */
  Module_->AttachHealth(Health_);   /* read-only: the module sees its damage, never writes it */
  PublishPose();   /* the declarative spawn is already a valid pose — nobody ever reads an empty one */
}

void FBSimUnit::PublishPose() {
  Pose_.LatDeg = St_.lat; Pose_.LonDeg = St_.lon; Pose_.ElevM = St_.elev;
  Pose_.RollDeg = St_.roll; Pose_.PitchDeg = St_.pitch; Pose_.YawDeg = St_.yaw;
  Pose_.SpeedMs = St_.speed;
  Pose_.HeadingDeg = St_.yaw;   /* no ground-track field on fb_fdm_state; yaw is the flown heading */
  /* The surface under this unit, from the owner's own elevation hook: a radio horizon is a height above
   * the SURFACE, and no receiver can measure the terrain under a sender it only hears. */
  Pose_.GroundAslM = GroundAslM_;
  /* The surfaces, off the FDM rather than off any command: the picture must show what the aeroplane is
   * doing, and only the engine knows that. A unit without an airframe keeps the zeroed default. */
  if (Fdm_) {
    FBUnitArticulation &a = Pose_.Art;
    a.AileronLRad = (float)Fdm_->GetLeftAileronRad();
    a.AileronRRad = (float)Fdm_->GetRightAileronRad();
    a.ElevonLRad = (float)Fdm_->GetLeftElevonRad();
    a.ElevonRRad = (float)Fdm_->GetRightElevonRad();
    a.RudderRad = (float)Fdm_->GetRudderRad();
    a.LefDeg = (float)Fdm_->GetLefDeg();
    a.SpeedbrakeDeg = (float)Fdm_->GetSpeedbrakeDeg();
    a.GearNorm = (float)Fdm_->GetGearPos();
    a.HookNorm = (float)Fdm_->GetHookNorm();
    a.CanopyNorm = (float)Fdm_->GetCanopyNorm();
  }
  /* A UNIT RADIATES ONLY WHAT ITS MODULE DECLARED. An undeclared slot is not a silent one: it does not
   * exist, so the field keeps its zero — which is the same signature a powered-down box produces. */
  Sensors::FBDatalinkSystem *dl = Module_->Datalink();
  Sensors::FBRadarSystem *radar = Module_->Radar();
  Weapons::FBStoresSystem *stores = Module_->Stores();
  Sig_.DatalinkXmt = dl && dl->Transmitting();
  Sig_.IffXpdr = radar && radar->IffTransponder();
  /* Not an emission but the same question: what may a foreign sensor notice? A plume is visible to an
   * infrared head and to nothing else in the tree, and it is read off the ENGINE rather than off a
   * throttle position, so no module can publish a plume it is not producing. */
  Sig_.Afterburner = Fdm_ && Fdm_->GetAugmentation();
  /* The beam is derived by the SET, so the emission cannot disagree with the pattern the antenna flies.
   * Combined here only because a tracking radar that is also supporting a shot is a different warning to
   * receive, and whether this jet supports one is the STORES system's knowledge, not the radar's. */
  Sig_.Radar[0] = radar ? radar->Emission() : FBEmitterSignature{};
  /* The SECOND antenna, silent for every airframe (FBModule's default) — a position radiates its
   * acquisition set and its fire control at the same time, and the RWR has to hear both. */
  Sig_.Radar[1] = Module_->TrackBeamEmission();
  if (stores)
    for (int i = 0; i < kMaxEmitterBeams; i++)
      if (Sig_.Radar[i].Mode == FBEmitterMode::Track && stores->Uplink().Active)
        Sig_.Radar[i].Mode = FBEmitterMode::Guidance;
  Sig_.RcsM2 = (float)Module_->RadarCrossSectionM2();
  if (Sensors::FBCountermeasureSystem *cm = Module_->Countermeasures()) {
    const FBChaffCloud *clouds = cm->Clouds();
    for (int i = 0; i < kMaxChaffClouds; i++) Sig_.Chaff[i] = clouds[i];
    const FBFlareCloud *flares = cm->Flares();
    for (int i = 0; i < kMaxFlareClouds; i++) Sig_.Flare[i] = flares[i];
  }
  if (stores) {
    Sig_.Uplink = stores->Uplink();
    /* ...and the spot it is holding, from the same box and at the same barrier: a semi-active LASER
     * round reads this exactly the way its radar cousin reads the uplink beside it. */
    Sig_.Designation = stores->Designation();
  }
  /* WHAT AN EYE GETS. The three dimensions are the damage layout read as geometry — the layout states
   * HALF-extents (how far the airframe reaches from the CG), an eye measures the WHOLE dimension of a
   * silhouette — so the gun and the eye cannot end up with two tables about one aeroplane. The type is
   * the module's registry key and nothing else; only a sensor that has EARNED it by angular resolution
   * may repeat it (doc/sensors.md §9.7). */
  const FBDamageLayout &lay = Module_->DamageLayout();
  Sig_.Visual.FrontalM = (float)(2.0 * lay.FrontalExtentM);
  Sig_.Visual.LateralM = (float)(2.0 * lay.LateralExtentM);
  Sig_.Visual.PlanM = (float)(2.0 * (lay.PlanExtentM > 0.0 ? lay.PlanExtentM : lay.LateralExtentM));
  {
    const std::string &tn = Module_->TypeName();
    std::size_t n = 0;
    while (n + 1 < (std::size_t)kVisualTypeNameLen && n < tn.size()) { Sig_.Visual.TypeName[n] = tn[n]; n++; }
    Sig_.Visual.TypeName[n] = '\0';
  }
  /* ...and what a HIT made of it. Read off the register rather than off any burst result, so the
   * picture cannot show a fire that the physics is not already flying with. */
  Sig_.Damage.Hits = (uint16_t)Health_.Hits();
  Sig_.Damage.CombatEffective = Health_.CombatEffective();
  Sig_.Damage.Destroyed = Health_.Destroyed();
  /* What this jet tells its own flight — published at the same barrier as everything else it radiates,
   * so no receiver ever reads half of it. Empty for a unit in no flight. */
  Sig_.Flight = GetFlight().Declared() ? Module_->FlightReport() : FBFlightReport{};
  /* ...and what a POSITION tells its air-defence net: a point, an age and a weapons control state, and
   * nothing that names the target. Silent for every module that is on no net. */
  Sig_.Net = Module_->NetReport();
  Sig_.CommJamM = (float)Module_->CommJamM();
}

void FBSimUnit::Run(double dt, const FBUnitRegistry *units, const World::FBWorld *world) {
  Module_->Run(St_, dt, units, world);
}

void FBSimUnit::UpdateGroundAsl(double sampleM) {
  if (FBElevationResolved(sampleM)) GroundAslM_ = sampleM;
  Module_->SetGroundAsl((float)GroundAslM_);
  /* The terminal's OWN end of the radio horizon, from the same sample and on the same tick — pushed
   * here rather than through every module, because it is the owner's terrain and not the module's. */
  if (Sensors::FBDatalinkSystem *dl = Module_->Datalink()) dl->SetOwnGroundAslM(GroundAslM_);
  if (Fdm_) Fdm_->SetGroundElevM(GetKind() == FBUnitKind::Weapon ? Fdm::FBFdm::kNoGroundElevM : GroundAslM_);
}

/* Written through only on a CHANGE: in still air (every mission that declares no weather) the airframe
 * is never told about wind at all, which is what keeps a calm run bit-identical to one from before this
 * channel existed. */
void FBSimUnit::UpdateWind(const FBWindNed &wind) {
  if (!Fdm_) return;
  if (wind.N == Wind_.N && wind.E == Wind_.E && wind.D == Wind_.D) return;
  Wind_ = wind;
  Fdm_->SetWindNedMs(wind.N, wind.E, wind.D);
}

/* The module publishes the platform block at its slot cadence; re-publishing at frame rate draws the
 * conformal HUD against the pose actually being rendered. Same block, same writer role, no second copy. */
FBState FBSimUnit::HudState() const {
  FBState hs = Module_->Telemetry();
  FBPlatformBlock &p = hs.Platform;
  p.RollDeg = (float)St_.roll; p.PitchDeg = (float)St_.pitch; p.YawDeg = (float)St_.yaw;
  p.AltM = (float)St_.elev; p.GsMs = (float)St_.gs; p.TasMs = (float)St_.speed; p.VsMs = (float)St_.vy;
  p.H.Publish(hs.NowS);
  return hs;
}

/* Two steps, in this order and nowhere else: the core decides what the geometry did (the module supplies
 * only WHERE its systems are), then the result is handed to the airframe. */
FBDamageResult FBSimUnit::TakeBurst(const FBBurst &burst) {
  FBDamageResult r = FBDamageModel::Apply(burst, Module_->DamageLayout(), Health_);
  ApplyDamageToAirframe();
  return r;
}

FBDamageResult FBSimUnit::TakeKineticBurst(const FBKineticBurst &burst) {
  FBDamageResult r = FBDamageModel::ApplyKinetic(burst, Module_->DamageLayout(), Health_);
  ApplyDamageToAirframe();
  return r;
}

/* Health -> physics. Idempotent and called only when the register changed: no per-frame work. A wrecked
 * ground target is wrecked in its register and nowhere else. */
void FBSimUnit::ApplyDamageToAirframe() {
  if (!Fdm_) return;
  /* PROPULSION, which on a twin is not the same question as "is engine 0 out": the register knows how
   * many this airframe declared, and only ALL of them being out is a cutoff. One of two is a thrust
   * loss the aircraft keeps flying with, which is the state a two-engine fighter is characterised by. */
  if (Health_.PropulsionOut()) {
    if (Systems::FBAirframeControls *c = Module_->Controls()) c->EngineCutoff();   /* the same controls path a pilot would use */
    Fdm_->SetThrottleLimit(0.0);
  } else if (Health_.Failed(FBSystemId::Engine) || Health_.Failed(FBSystemId::Engine2)) {
    Fdm_->SetThrottleLimit(kThrottleLimitOneEngineOut);
  } else if (Health_.Degraded(FBSystemId::Engine) || Health_.Degraded(FBSystemId::Engine2)) {
    Fdm_->SetThrottleLimit(kThrottleLimitDegraded);
  }
  switch (Health_.State(FBSystemId::FlightControls)) {
    case FBHealthState::Failed: Fdm_->SetControlAuthority(kAuthorityFailed); break;
    case FBHealthState::Degraded: Fdm_->SetControlAuthority(kAuthorityDegraded); break;
    case FBHealthState::Intact: break;
  }
  switch (Health_.State(FBSystemId::Structure)) {
    case FBHealthState::Failed: Fdm_->SetDamageDrag(kDamageDragFt2Failed); break;
    case FBHealthState::Degraded: Fdm_->SetDamageDrag(kDamageDragFt2Degraded); break;
    case FBHealthState::Intact: break;
  }
}

FBMissionMonitorSample FBSimUnit::BuildMissionSample(const FBMissionRoster &roster) const {
  FBMissionMonitorSample s;
  s.LatDeg = St_.lat; s.LonDeg = St_.lon; s.ElevM = St_.elev;
  /* No airframe = on the ground, UNLESS the module says otherwise: a kinematic air mover has no
   * FBFdm and is nonetheless flying, and a WOW bit that claimed otherwise would be wrong for
   * every consumer of it (the release interlock is only the first). */
  s.AnyWow = Fdm_ ? Fdm_->GetWow() : !Module_->Airborne();
  s.GroundSpeedKt = St_.gs * kMsToKt;
  s.CombatIneffective = !Health_.CombatEffective();
  s.ReleasedWeapon = ReleasedWeapon_;
  s.Roster = roster;
  return s;
}

bool FBSimUnit::FinalizeMission(double simT, const FBMissionRoster &roster) {
  return Mission_ && Mission_->Finalize(BuildMissionSample(roster), simT);
}

bool FBSimUnit::RunMonitors(double simT, const FBMissionRoster &roster) {
  /* THE PHYSICS JUDGE ONLY JUDGES PHYSICS: every input it takes is an FDM observation, so a unit that
   * does not fly is never shown to it. Its MISSION judge below is untouched. */
  if (Fdm_ && Flight_.Tick(FBBuildFlightMonitorSample(*Fdm_, St_, GroundAslM_), simT)) {
    /* The judge STATES, it does not decide: its verdict is entered in the damage register, which is
     * where every unit kind's "still alive" lives and what the simulation loop reads. It goes through
     * FBDamageModel because that is the register's only writer — a monitor writing past it would be a
     * second mutator on a type whose whole guarantee is that it has one. */
    FBDamageModel::ApplyPhysicalKo(Health_);
    if (Systems::FBAirframeControls *c = Module_->Controls()) c->EngineCutoff();
    return true;
  }
  if (Mission_ && Mission_->Tick(BuildMissionSample(roster), simT)) {
    /* Touched down off the assigned runway — NOT for a shootdown, where cutting the engine would be the
     * verdict acting on the aircraft instead of the damage doing so. */
    if (Mission_->Verdict() == FBMissionVerdict::Fail && Health_.CombatEffective()) {
      if (Systems::FBAirframeControls *c = Module_->Controls()) c->EngineCutoff();
    }
    return true;
  }
  return false;
}

void FBSimUnit::CheckEnvelope() {
  /* AoA is numerically undefined at near-zero airspeed — the gate avoids a settle-transient warning. */
  bool haveAirspeed = St_.cas > 15.0;
  if (haveAirspeed && St_.alphaDeg > 25.0 && !WarnedStall_) {
    FBLog::Warn("pilot", "WARN", {{"kind", "stall"}, {"aoaDeg", St_.alphaDeg}});
    WarnedStall_ = true;
  } else if (!haveAirspeed || St_.alphaDeg < 20.0) WarnedStall_ = false;
  if (St_.mach > 1.2 && !WarnedOverspeed_) {
    FBLog::Warn("pilot", "WARN", {{"kind", "overspeed"}, {"mach", St_.mach}});
    WarnedOverspeed_ = true;
  } else if (St_.mach < 1.1) WarnedOverspeed_ = false;
  if (AglM() < 150.0 && St_.vy < -15.0 && !WarnedSink_) {
    FBLog::Warn("pilot", "WARN", {{"kind", "sink"}, {"vsMs", St_.vy}, {"aglM", AglM()}});
    WarnedSink_ = true;
  } else if (St_.vy > -5.0) WarnedSink_ = false;
}

/* THE COLUMN LAYOUT IS THE MODULE'S DECLARATION READ IN ORDER. Every module in the tree declares all
 * fifteen of these slots, so every telemetry.csv is what it was; a module that declares fewer writes
 * fewer columns, which is the price of the slots being optional at all. doc/architecture.md §Gaps. */
void FBSimUnit::StartTelemetry(FBTelemetrySink *sink) {
  Pilot::FBPilot *pilot = Module_->PilotSystem();
  Bus_.Register(&FdmSrc_);
  RegisterIf(Module_->AirDataSystem());
  RegisterIf(pilot);
  RegisterIf(Module_->FlightControl());
  RegisterIf(Module_->Controls());
  RegisterIf(Module_->Datalink());
  RegisterIf(Module_->Radar());
  /* THE APPEND RULE, from here down: a new source appends columns, it never shifts old ones — every
   * column ever measured keeps its position, because baselines and analysis read by position. */
  if (pilot) Bus_.Register(&pilot->BfmTrack());
  RegisterIf(Module_->WarningSystem());
  RegisterIf(Module_->Commands());
  Bus_.Register(&BusSrc_);
  RegisterIf(Module_->Stores());
  RegisterIf(Module_->Rwr());
  RegisterIf(Module_->Countermeasures());
  if (pilot) Bus_.Register(&pilot->Engagement());
  Bus_.Register(&HealthSrc_);
  RegisterIf(Module_->Guns());
  RegisterIf(Module_->Irst());
  if (pilot) Bus_.Register(&pilot->FlightPicture());
  RegisterIf(Module_->Visual());
  /* THE TWO CONDITIONAL SOURCES, and they are conditional so that a mission which declares neither a
   * net nor a zone writes the identical file it wrote before either existed: a module that is on no net
   * returns null, and a judge with no declared zones returns null. */
  if (FBTelemetrySource *net = Module_->NetTelemetry()) Bus_.Register(net);
  if (Mission_) if (FBTelemetrySource *zones = Mission_->ZoneTelemetry()) Bus_.Register(zones);
  Bus_.SetSink(sink);
  Bus_.Start();
}

} // namespace FlightBox::Units
