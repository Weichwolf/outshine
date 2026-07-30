#include "FBStoresSystem.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBUnits.h"
#include <cassert>
#include <cmath>

namespace FlightBox::Weapons {

namespace {
constexpr double kInToM = 0.0254;
} // namespace

void FBStoresSystem::DeclareStation(int number, double xIn, double yIn, double zIn) {
  if (Count_ >= kMaxStations || number < 1) return;
  Station &s = Stations_[Count_++];
  s.XIn = xIn; s.YIn = yIn; s.ZIn = zIn;
  StationNumber_[Count_ - 1] = number;
}

void FBStoresSystem::DeclareFuelPlumbing(int station, int fdmTankIndex) {
  int i = IndexOf(station);
  if (i < 0 || fdmTankIndex < 0) return;
  Stations_[i].TankIdx = fdmTankIndex;
}

bool FBStoresSystem::TankIsPlumbed(int fdmTankIndex) const {
  for (int i = 0; i < Count_; i++)
    if (Stations_[i].TankIdx == fdmTankIndex) return true;
  return false;
}

int FBStoresSystem::IndexOf(int station) const {
  for (int i = 0; i < Count_; i++)
    if (StationNumber_[i] == station) return i;
  return -1;
}

/* Die Gegenrichtung der Ausnahme in DeclareGroundLauncher: was eine Zelle bekommt, kann keine Stellung
 * sein. Assertion statt Ausnahme — kontrollierte Umgebung, und der Fall ist ein Programmierfehler. */
void FBStoresSystem::AttachFdm(Fdm::FBFdm &fdm) {
  assert(!GroundLauncher_ && "a ground launcher has no airframe to attach");
  Fdm_ = &fdm;
  for (int i = 0; i < Count_; i++) {
    char name[24];
    snprintf(name, sizeof name, "station%d", StationNumber_[i]);
    Stations_[i].PointMass = fdm.AddStorePointMass(name, Stations_[i].XIn, Stations_[i].YIn,
                                                   Stations_[i].ZIn);
  }
  /* Die Entnahme-Reihenfolge des Modells LESEN, damit sie spaeter zurueckgeschrieben werden kann. Nur
   * wo ueberhaupt verrohrt wurde — sonst faellt der ganze Mechanismus weg. */
  bool plumbed = false;
  for (int i = 0; i < Count_; i++) plumbed = plumbed || Stations_[i].TankIdx >= 0;
  if (!plumbed) return;
  TankCount_ = fdm.GetFuelTankCount();
  if (TankCount_ > kMaxFuelTanks) TankCount_ = kMaxFuelTanks;
  for (int t = 0; t < TankCount_; t++) TankPriority0_[t] = fdm.GetFuelTankPriority(t);
  for (int i = 0; i < Count_; i++)
    assert(Stations_[i].TankIdx < TankCount_ && "a station is plumbed to a tank the model has not got");
}

bool FBStoresSystem::AnyFuelStore() const {
  for (int i = 0; i < Count_; i++)
    if (Stations_[i].Fuel) return true;
  return false;
}

void FBStoresSystem::PublishFuelPlumbing() {
  if (!Fdm_ || TankCount_ <= 0) return;
  if (!AnyFuelStore()) {
    for (int t = 0; t < TankCount_; t++) Fdm_->SetFuelTankPriority(t, TankPriority0_[t]);
    return;
  }
  for (int t = 0; t < TankCount_; t++) Fdm_->SetFuelTankPriority(t, 2);
  for (int i = 0; i < Count_; i++) {
    const Station &s = Stations_[i];
    if (s.TankIdx < 0) continue;
    if (s.Fuel) {
      Fdm_->SetFuelTankPriority(s.TankIdx, 1);
    } else {
      Fdm_->SetFuelTankLbs(s.TankIdx, 0.0);
      Fdm_->SetFuelTankPriority(s.TankIdx, 0);   /* dort haengt kein Tank: der Motor sieht ihn nicht */
    }
  }
}

bool FBStoresSystem::Load(int station, const FBStoreSpec &spec) {
  int i = IndexOf(station);
  if (i < 0 || Stations_[i].Kind != FBStoreKind::None) return false;
  /* EIN TANK BRAUCHT EINE LEITUNG. Sein Sprit ist FGTank-Inhalt und nichts sonst, also gibt es ihn nur
   * dort, wo das Modell einen Tank fuer diese Station deklariert hat. */
  bool fuel = spec.FuelLbs > 0.0;
  if (fuel && (Stations_[i].TankIdx < 0 || !Fdm_)) return false;
  Stations_[i].Kind = spec.Kind;
  Stations_[i].MassLbs = spec.MassLbs;   /* TROCKEN: der Sprit wiegt im Tank des Modells */
  Stations_[i].DragAreaFt2 = spec.DragAreaFt2;
  Stations_[i].Fuel = fuel;
  if (fuel) {
    double cap = Fdm_->GetFuelTankCapacityLbs(Stations_[i].TankIdx);
    double lbs = cap > 0.0 && spec.FuelLbs > cap ? cap : spec.FuelLbs;
    if (lbs < spec.FuelLbs)
      FBLog::Warn("sms", "TANK_CLAMPED", {{"station", station}, {"store", spec.Key},
                                          {"wantLbs", spec.FuelLbs}, {"capacityLbs", cap}});
    Fdm_->SetFuelTankLbs(Stations_[i].TankIdx, lbs);
    FBLog::Info("sms", "TANK_LOADED", {{"station", station}, {"store", spec.Key},
                                       {"fuelLbs", lbs}, {"dryLbs", spec.MassLbs},
                                       {"tank", Stations_[i].TankIdx}});
  }
  if (Selected_ < 0) Selected_ = station;
  PublishLoadout();
  PublishFuelPlumbing();
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

/* Station-Step: die naechste BELEGTE Station, umlaufend — eine Ripple braucht so keine Neuwahl. */
void FBStoresSystem::SelectNextLoaded() {
  int from = IndexOf(Selected_);
  for (int k = 1; k <= Count_; k++) {
    int i = (from + k) % (Count_ > 0 ? Count_ : 1);
    if (Stations_[i].Kind != FBStoreKind::None) { Selected_ = StationNumber_[i]; return; }
  }
  Selected_ = -1;
}

/* Gewichtsgewichteter SCHWERPUNKT der belegten Stationen statt eines festen Punktes: so erzeugt eine
 * asymmetrische Zuladung ihr Gier-/Rollmoment aus JSBSims eigenem Kraftmodell. */
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
  /* Die zwei HARDWARE-Verriegelungen zuerst: ein physischer Schalter sperrt den Softwarepfad, und kein
   * Software-Zustand darf sich daran vorbeireden. */
  if (Arm_ != FBArmState::Arm)
    return refuse(FBCommandReason::HardwarePrecedence, "master arm not in ARM");
  if (Wow_ && !GroundLauncher_)
    return refuse(FBCommandReason::HardwarePrecedence, "weight on wheels");
  int i = IndexOf(Selected_);
  if (i < 0 || Stations_[i].Kind == FBStoreKind::None)
    return refuse(FBCommandReason::OutOfContext, "no loaded station selected");
  if (PendingCount_ >= kMaxPendingReleases)
    return refuse(FBCommandReason::ChannelBusy, "release queue not drained");

  /* Die WAFFENSPEZIFISCHEN Verriegelungen: die Antwort der FEUERLEITUNG (in Run() gecached), nicht die
   * Meinung des SMS. */
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
  /* PRUEFUNG 8 — die Huelle des STORES, und sie steht NACH den zwei Hardware-Verriegelungen und nach
   * der Antwort der Feuerleitung, weil sie eine Eigenschaft der WAFFE ist und nicht der Zelle und nicht
   * des Rechners. Eine Zeile mit Min = Max = 0 kennt keine Huelle, also aendert diese Pruefung fuer
   * jede vor ihr geschriebene Katalogzeile nichts. doc/air-to-ground.md §3.4. */
  if (rspec && (rspec->ReleaseMinKt > 0.0 || rspec->ReleaseMaxKt > 0.0)) {
    if ((rspec->ReleaseMinKt > 0.0 && CasKt_ < rspec->ReleaseMinKt) ||
        (rspec->ReleaseMaxKt > 0.0 && CasKt_ > rspec->ReleaseMaxKt)) {
      FBLog::Warn("sms", "RELEASE_ENVELOPE", {{"store", rspec->Key}, {"casKt", CasKt_},
          {"minKt", rspec->ReleaseMinKt}, {"maxKt", rspec->ReleaseMaxKt}});
      return refuse(FBCommandReason::OutOfContext, "store release envelope");
    }
  }

  Station &s = Stations_[i];
  FBStoreRelease &rel = Pending_[PendingCount_++];
  rel.Station = Selected_;
  rel.Kind = s.Kind;
  rel.MassLbs = s.MassLbs;
  rel.SimTimeS = nowS;
  /* Struktur (x achtern, y rechts, z auf, Zoll) -> Koerper (vorn/rechts/unten, Meter) RELATIV ZUM CG:
   * JSBSims eigene Konvention, hier angewandt, weil nur diese Klasse weiss, wo der Pylon sitzt. */
  double cgXIn = Fdm_ ? Fdm_->GetCgXIn() : s.XIn;
  rel.OffFwdM = (cgXIn - s.XIn) * kInToM;
  rel.OffRightM = s.YIn * kInToM;
  rel.OffDownM = -s.ZIn * kInToM;
  /* Die SCHIENE, falls es eine gibt: eine Stellung richtet ihren Werfer, ein Pylon zeigt nach vorn. */
  rel.HaveRail = HaveRail_;
  rel.RailPitchDeg = RailPitchDeg_;
  rel.RailYawDeg = RailYawDeg_;
  /* Die Startprogrammierung: eine gelenkte Runde nimmt die Zielschaetzung des Schuetzen mit, eine
   * ungelenkte die Abwurfloesung, mit der sie gezielt wurde. */
  if (rspec && rspec->Guided) {
    rel.LauncherId = SelfId_;
    rel.Target = Target_;
    GuidedInFlight_++;
  }
  /* Die Abwurfloesung reist mit JEDER Runde, die eine hat: fuer die ungelenkte ist sie die Vorhersage,
   * gegen die der Aufschlag gemessen wird, fuer die LENKBOMBE zusaetzlich der Punkt, den der Schuetze
   * ab jetzt beleuchtet. Fuer jede Luft-Luft-Runde ist sie ungueltig (SolveGroundAttack loest fuer sie
   * nicht), also aendert die Verallgemeinerung nichts an einer bestehenden Zeile. */
  rel.Solution = Solution_;
  if (rspec && rspec->Seeker == FBSeekerKind::AntiRadiation) {
    rel.CueValid = CueValid_;
    rel.CueAzDeg = CueAzDeg_;
    rel.CueElDeg = CueElDeg_;
    rel.ArClass = ArClass_;
  }
  /* DER SCHUETZE BINDET SICH: ab hier beleuchtet er den Punkt, gegen den er ausgeloest hat, bis seine
   * Besatzung aufhoert oder die Geometrie es ihm nimmt. */
  if (rspec && rspec->Seeker == FBSeekerKind::SemiActiveLaser && Solution_.Valid) {
    LaserInFlight_++;
    HaveSpot_ = true;
    SpotLatDeg_ = Solution_.AimLatDeg;
    SpotLonDeg_ = Solution_.AimLonDeg;
    SpotElevM_ = Solution_.ImpactElevM;
  }

  /* Das Gewicht VOR dem Wirksamwerden: FGMassBalance summiert die Punktmassen in seinem eigenen Run, das
   * neue Gewicht existiert einen Schritt spaeter und wird aus sms_gw_lbs gelesen. */
  double gwLbs = Fdm_ ? Fdm_->GetWeightLbs() : 0.0;
  double storesBefore = LoadedMassLbs();
  /* DER UNTERSCHIED ZUR BOMBE: ein Tank nimmt seinen Inhalt mit. Der Sprit wird nicht uebertragen und
   * nicht gutgeschrieben — er ist vom Flugzeug herunter, und der fallende Koerper hat die eine trockene
   * Masse seines Decks. doc/modules/stores.md §Spec. */
  double fuelGoneLbs = 0.0;
  if (s.Fuel && Fdm_ && s.TankIdx >= 0) {
    fuelGoneLbs = Fdm_->GetFuelTankLbs(s.TankIdx);
    Fdm_->SetFuelTankLbs(s.TankIdx, 0.0);
  }
  bool wasFuel = s.Fuel;
  s.Kind = FBStoreKind::None;
  s.MassLbs = 0.0;
  s.DragAreaFt2 = 0.0;
  s.Fuel = false;
  Released_++;
  SelectNextLoaded();
  PublishLoadout();   /* die Masse ist ab diesem Schritt vom Jet herunter */
  if (wasFuel) {
    PublishFuelPlumbing();
    FBLog::Info("sms", "TANK_JETTISON", {{"station", rel.Station}, {"dryLbs", rel.MassLbs},
                                         {"fuelGoneLbs", fuelGoneLbs},
                                         {"fuelLeftLbs", Fdm_ ? Fdm_->GetFuelTotalLbs() : 0.0}});
  }
  outcome = FBCommandOutcome::Accepted;
  reason = FBCommandReason::None;
  const FBStoreSpec *spec = FBStoreSpecOf(rel.Kind);
  FBLog::Info("sms", "RELEASE", {{"station", rel.Station}, {"store", spec ? spec->Key : "?"},
      {"storeLbs", rel.MassLbs}, {"gwLbs", gwLbs},
      {"storesLbsBefore", storesBefore}, {"storesLbsAfter", LoadedMassLbs()},
      {"remaining", LoadedCount()}, {"nextStation", Selected_}});
  /* Eigene Zeile statt weiterer RELEASE-Felder: die Vorhersage, gegen die das geflogene Ergebnis spaeter
   * gemessen wird (der Fehler der DLZ ist eine echte Eigenschaft, kein zu versteckender Fehler). */
  if (rspec && rspec->Guided)
    FBLog::Info("sms", "LAUNCH_SOLUTION", {{"store", spec ? spec->Key : "?"},
        {"rangeM", ZoneRangeM_}, {"rminM", ZoneMinM_}, {"raeroM", ZoneMaxM_}, {"rtrM", ZoneRtrM_},
        {"ttaS", ZoneTtaS_}, {"ttiS", ZoneTtiS_},
        {"tgtLat", Target_.LatDeg}, {"tgtLon", Target_.LonDeg}, {"tgtAltM", Target_.AltM},
        {"tgtAgeS", nowS - Target_.StampS}});
  /* ...und dasselbe fuer die ungelenkte Runde: was der Rechner im Moment des Pickles VORHERSAGTE. */
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
  /* Ein unpublizierter Airframe-Block laesst den letzten bekannten Wert stehen, statt „in der Luft" zu
   * erfinden. */
  if (state.Airframe.H.Readable()) Wow_ = state.Airframe.WeightOnWheels;
  /* Die Fahrt fuer die Huellenpruefung, aus derselben Quelle gecached und aus demselben Grund: Release()
   * kommt zwischen zwei Ticks vom Bus und darf nicht selbst nach dem Bus greifen. */
  if (state.AirData.H.Readable()) CasKt_ = state.AirData.CasKt;

  /* Die Antwort der Feuerleitung, fuer den Pickle gecached (der zwischen zwei Ticks vom Bus kommt). */
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

  /* Der Schuetze stuetzt seinen Schuss genau solange, wie seine Feuerleitung das Ziel haelt: Lock weg,
   * Uplink weg — mitten im Flug. Was die Rakete dann tut, entscheidet die Rakete. */
  Uplink_.Active = GuidedInFlight_ > 0 && Target_.Valid;
  Uplink_.LauncherId = SelfId_;
  Uplink_.Target = Target_;

  /* ---- DIE BELEUCHTUNG, in derselben Form wie der Uplink darueber und aus demselben Grund: ein
   * publizierter ZUSTAND, den der Empfaenger liest, keine Nachricht, die jemand zustellt. Drei
   * Bedingungen, und alle drei sind Tatsachen ueber den SCHUETZEN — dass eine Runde unterwegs ist, was
   * seine Besatzung geschaltet hat, und wohin sein Bug zeigt. ---- */
  bool crewLasing = LaseOffS_ <= 0.0 || state.NowS < LaseOffS_ ||
                    (LaseOnS_ > 0.0 && state.NowS >= LaseOnS_);
  bool inField = true;
  if (HaveSpot_) {
    double brg = FBBearingDeg(OwnLatDeg_, OwnLonDeg_, SpotLatDeg_, SpotLonDeg_);
    inField = std::fabs(FBWrap180(brg - OwnYawDeg_)) <= kDesignatorHalfDeg;
  }
  Designation_ = FBLaserDesignation{};
  Designation_.Active = LaserInFlight_ > 0 && HaveSpot_ && crewLasing && inField;
  Designation_.DesignatorId = SelfId_;
  Designation_.LatDeg = SpotLatDeg_;
  Designation_.LonDeg = SpotLonDeg_;
  Designation_.ElevM = SpotElevM_;
  Designation_.StampS = state.NowS;

  FBStoresBlock &b = state.Stores;
  b.Designating = Designation_.Active;
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
  schema.Add("sms_arm");            /* 1 = Master Arm ARM */
  schema.Add("sms_station");        /* gewaehlte Station, -1 = keine */
  schema.Add("sms_loaded");
  schema.Add("sms_lbs", "lb");      /* die Buecher des SMS */
  schema.Add("sms_released");
  /* Das Bruttogewicht der ZELLE in DERSELBEN Zeile wie die Buecher darueber — nur so ist der
   * Traegereffekt mess- statt behauptbar. */
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

} // namespace FlightBox::Weapons
