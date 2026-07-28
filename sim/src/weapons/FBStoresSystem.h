/* FlightBox — FBStoresSystem: der Stores/SMS-Slot — Stationsinventar, Master-Arm-Verriegelung, der EINE
 * Abwurfpfad. Traegereffekt (Masse/Schwerpunkt/Widerstand) ist Physik der Engine; die Klasse SPAWNT
 * nichts (sie legt einen FBStoreRelease in eine Warteschlange, der BESITZER spawnt); der Pickle ist ein
 * KOMMANDO und damit ablehnbar. doc/weapons-and-damage.md, Abschnitt 2. */
#ifndef FBSTORESSYSTEM_H
#define FBSTORESSYSTEM_H

#include "FBAvionicsCommand.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox::Modules { class FBSiteModule; }

namespace FlightBox::Weapons {

class FBStoresSystem : public FBTelemetrySource {
public:
  static constexpr int kMaxStations = kMaxStoreStations;
  /* Eine volle Ripple aller Stationen passt hinein — die Queue kann nie der Grund sein, dass ein Store
   * verlorengeht. */
  static constexpr int kMaxPendingReleases = kMaxStations;

  ~FBStoresSystem() override = default;

  /* Station `number` (1-basiert wie der Jet zaehlt) im STRUKTURRAHMEN des Modells (Zoll). Stationen
   * muessen VOR AttachFdm deklariert sein. */
  void DeclareStation(int number, double xIn, double yIn, double zIn);
  /* Bindet die Zelle und legt je deklarierter Station eine JSBSim-Punktmasse an (Gewicht 0 = leer). */
  void AttachFdm(Fdm::FBFdm &fdm);

  bool Load(int station, const FBStoreSpec &spec);   /* false: unbekannte oder belegte Station */
  int  StationCount() const { return Count_; }
  int  LoadedCount() const;
  double LoadedMassLbs() const;
  FBStoreKind StoreAt(int station) const;

  void SetMasterArm(FBArmState s) { Arm_ = s; }
  FBArmState MasterArm() const { return Arm_; }

  /* Die Zielschaetzung der Feuerleitung, je Tick vom Modul. Der SMS rechnet sie weder noch hinterfragt
   * er sie — er kopiert sie beim Start auf die Runde und strahlt sie danach als Uplink ab. */
  void SetTargetState(const FBWeaponTargetState &t) { Target_ = t; }
  /* Das ungelenkte Gegenstueck, aus demselben Grund: die Vorhersage muss den Jet MIT der Waffe
   * verlassen, damit der gemessene Aufschlag danebengelegt werden kann. */
  void SetReleaseSolution(const FBReleaseSolution &s) { Solution_ = s; }
  /* Wer dieses Flugzeug ist, damit eine gestartete Runde weiss, wessen Uplink sie hoert. */
  void SetUnitId(int id) { SelfId_ = id; }

  /* DER SUCHER-CUE einer Antiradiationsrunde: zwei Winkel vom eigenen Warnempfaenger und die Klasse,
   * gegen die die Mission sie programmiert hat. Vom MODUL je Tick gesetzt (nur das Modul kennt
   * `arm_class`), hier nur kopiert — der SMS klassifiziert nichts. doc/air-to-ground.md §7. */
  void SetSeekerCue(bool valid, double azDeg, double elDeg, FBArTargetClass cls) {
    CueValid_ = valid; CueAzDeg_ = azDeg; CueElDeg_ = elDeg; ArClass_ = cls;
  }

  /* WAS DIESER SCHUETZE BELEUCHTET — dasselbe Konstrukt wie Uplink(): ein publizierter ZUSTAND, keine
   * Nachricht. Aktiv, solange eine halbaktive Laserrunde unterwegs ist, der Rechner eine Loesung hat
   * und der Zielpunkt im Blickfeld des Bezeichners liegt. Der Punkt ist der STEERPOINT, also Wissen des
   * Missionsautors und kein Sensor. doc/air-to-ground.md §3.2. */
  const FBLaserDesignation &Designation() const { return Designation_; }
  /* Der Schalter der BESATZUNG, gebrieft wie jede andere Handlung: ab `offS` wird nicht mehr
   * beleuchtet, ab `onS` wieder (0 = nie). Missionselapsierte Sekunden. */
  void BriefLase(double offS, double onS) { LaseOffS_ = offS; LaseOnS_ = onS; }
  /* WOHIN der Bezeichner zeigen kann. Kein Zielbehaelter (C7): das Geraet ist bugfest, also bricht die
   * Ausweichkurve die Beleuchtung — was die reale Einsatzbedingung dieser Waffe IST. */
  static constexpr double kDesignatorHalfDeg = 60.0;

  /* Aktiv nur, solange eine lock-fordernde Runde gestartet wurde UND die Feuerleitung noch ein Ziel
   * hat. Sie hoert NICHT auf, wenn der Flug der Runde endet — das meldet dem Schuetzen niemand. */
  const FBWeaponUplink &Uplink() const { return Uplink_; }
  bool SelectStation(int station);   /* false: unbekannte oder leere Station */
  int  SelectedStation() const { return Selected_; }

  /* Wirft den Store der GEWAEHLTEN Station ab oder lehnt mit dem Grund ab, den der Jet geben wuerde. */
  bool Release(double nowS, FBCommandOutcome &outcome, FBCommandReason &reason);

  /* WOHIN DIE SCHIENE ZEIGT, koerperfest in Grad. Fuer eine Zelle gibt es das nicht — ein Pylon zeigt
   * nach vorn, und gezielt wird mit dem Flugzeug —, also bleibt es ungesetzt und die Runde separiert
   * mit der Lage des Traegers, exakt wie bisher. Ein Werfer richtet statt dessen die Schiene. */
  void SetRailAttitude(double pitchDeg, double yawDeg) {
    RailPitchDeg_ = pitchDeg; RailYawDeg_ = yawDeg; HaveRail_ = true;
  }
  /* Der Drain des Besitzers: der aelteste noch nicht genommene Abwurf, FIFO. */
  bool TakeRelease(FBStoreRelease &out);

  /* Die Zelle veroeffentlicht ihre Lage ohnehin; der SMS braucht sie fuer genau EINE Frage — liegt der
   * bezeichnete Punkt noch vor dem Bug. Vom Modul gereicht, damit diese Klasse keinen FDM anfasst. */
  void SetOwnPose(double latDeg, double lonDeg, double yawDeg) {
    OwnLatDeg_ = latDeg; OwnLonDeg_ = lonDeg; OwnYawDeg_ = yawDeg;
  }

  virtual void Run(FBState &state, double dt);

  const char *TelemetryName() const override { return "sms"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  /* DIE EINZIGE AUSNAHME VON DER GEWICHTS-AUF-RAEDERN-VERRIEGELUNG, und sie ist keine Lockerung: die
   * Verriegelung fragt nach RAEDERN. Eine Stellung hat keine Zelle, meldet daher per Definition
   * Bodenkontakt (missions/runtime.md §3) und wuerde 100 % aller Bodenstarts abgelehnt bekommen.
   *
   * WARUM SO ENG: privat, mit genau EINEM Freund — dasselbe Schreibtor, mit dem core/FBSystemHealth
   * seine Monotonie haelt. Ein Flugzeugmodul kann diese Zeile nicht einmal AUFRUFEN, es kompiliert
   * nicht; und wer sie doch erreichte, bekaeme false, sobald je eine Zelle gebunden wurde. Die zwei
   * Zustaende schliessen sich in beide Richtungen aus (s. AttachFdm). */
  friend class Modules::FBSiteModule;
  bool DeclareGroundLauncher() {
    if (Fdm_) return false;   /* diese Rampe haengt an einer Zelle: sie hat Raeder */
    GroundLauncher_ = true;
    return true;
  }

  struct Station {
    double XIn = 0.0, YIn = 0.0, ZIn = 0.0;
    int    PointMass = -1;                       /* Index im FDM, -1 = nicht gebunden */
    FBStoreKind Kind = FBStoreKind::None;
    double MassLbs = 0.0, DragAreaFt2 = 0.0;
  };

  /* Sagt dem FDM die ganze Zuladung neu an. Nur bei Load und Release — zwischen diesen zwei Ereignissen
   * aendert sich an einer Zuladung nichts. */
  void PublishLoadout();
  void SelectNextLoaded();
  int  IndexOf(int station) const;   /* Stationsnummer -> Arrayindex, -1 wenn undeklariert */

  Station Stations_[kMaxStations];
  int StationNumber_[kMaxStations]{};   /* die Pylonnummern des Jets, in Deklarationsreihenfolge */
  int Count_ = 0;
  FBArmState Arm_ = FBArmState::Sim;   /* ein Jet faehrt mit Master Arm SAFE hoch */
  int Selected_ = -1;
  bool Wow_ = true;                    /* bis die Luftdaten anderes sagen: am Boden */
  bool GroundLauncher_ = false;        /* s. DeclareGroundLauncher */
  bool HaveRail_ = false;
  double RailPitchDeg_ = 0.0, RailYawDeg_ = 0.0;
  Fdm::FBFdm *Fdm_ = nullptr;               /* geborgt, nie besessen */

  /* Die DLZ-Felder sind die in Run() gecachte Antwort der FEUERLEITUNG: Release() wird vom Kommandobus
   * zwischen zwei Ticks gerufen und darf nicht selbst nach dem Bus greifen. */
  int SelfId_ = 0;
  FBWeaponTargetState Target_{};
  FBReleaseSolution Solution_{};
  FBWeaponUplink Uplink_{};
  bool   HaveZone_ = false, InZone_ = false;
  double CasKt_ = 0.0;   /* fuer Pruefung 8, aus demselben Grund gecached wie die DLZ-Felder */
  double ZoneRangeM_ = 0.0, ZoneMinM_ = 0.0, ZoneMaxM_ = 0.0, ZoneRtrM_ = 0.0;
  double ZoneTtaS_ = -1.0, ZoneTtiS_ = -1.0;
  bool   RadarLocked_ = false;
  int    GuidedInFlight_ = 0;          /* lock-fordernde Runden, die dieser Jet gestartet hat */

  /* ---- LUFT-BODEN. Alles hier ist "dieser Jet traegt nichts dergleichen", solange nichts es setzt. ---- */
  bool   CueValid_ = false;
  double CueAzDeg_ = 0.0, CueElDeg_ = 0.0;
  FBArTargetClass ArClass_ = FBArTargetClass::AnySurface;
  int    LaserInFlight_ = 0;
  double LaseOffS_ = 0.0, LaseOnS_ = 0.0;
  double SpotLatDeg_ = 0.0, SpotLonDeg_ = 0.0, SpotElevM_ = 0.0;
  bool   HaveSpot_ = false;
  double OwnLatDeg_ = 0.0, OwnLonDeg_ = 0.0, OwnYawDeg_ = 0.0;
  FBLaserDesignation Designation_{};

  FBStoreRelease Pending_[kMaxPendingReleases]{};
  int PendingCount_ = 0;
  int Released_ = 0;
};

} // namespace FlightBox::Weapons
#endif
