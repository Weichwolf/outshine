/* FlightBox — FBRadarSystem: der Sensors-Slot, das generische AKTIVE Luft-Luft-Radar und der bewusste
 * Gegenentwurf zum kooperativen Datalink (kooperativ = Identitaet geschenkt, aktiv = nur ein Echo).
 * Scanvolumen, Kontaktaufbau/-verlust, Anonymitaet, Abstrahlung, Doppler-Notch und die bewusst NICHT
 * modellierte Terrain-Maskierung: doc/flightbox/sensors.md, Abschnitt 4. */
#ifndef FBRADARSYSTEM_H
#define FBRADARSYSTEM_H

#include "FBCountermeasure.h"
#include "FBEmitter.h"
#include "FBRadarContact.h"
#include "FBState.h"
#include "FBTeam.h"
#include "FBTelemetry.h"
#include "FBUnits.h"
#include "FBFdm.h"

namespace FlightBox {

class FBUnit;
class FBUnitRegistry;

/* Ein vollstaendiges Antennenmuster — ein MODUS IST eines davon. Elevation als Mitte+Halb statt
 * symmetrischem Halbwinkel, weil ein Vertical-Scan-Muster nicht um die Rumpfreferenzlinie zentriert ist
 * und dasselbe Feld die Cursorposition eines schwenkbaren Musters traegt. */
struct FBRadarScanVolume {
  double AzCenterDeg = 0.0;    /* Mustermitte relativ zur NASE, + = rechts */
  double AzHalfDeg = 30.0;
  double ElCenterDeg = 0.0;
  double ElHalfDeg = 10.5;
  double RangeM = 40.0 * kNmToM;
  double FrameS = 2.0;         /* ein vollstaendiger Sweep dieses Volumens */
  bool   AutoAcquire = false;  /* lockt den naechsten festen Track ungefragt (die ACM-Eigenschaft) */
  bool   Active = true;        /* false = das Set strahlt nicht */
  /* Alle Leistung auf EIN Ziel: jedes andere Trackfile wird nicht mehr aufgefrischt und laeuft aus.
   * Als Eigenschaft des MUSTERS ausgedrueckt bleibt der ACM->STT-Uebergang im einen Volumen-Hook. */
  bool   SingleTarget = false;
};

class FBRadarSystem : public FBTelemetrySource {
public:
  /* Ausdrueckliche PLATZHALTER „irgendein Jaeger-Luft-Luft-Set", keine echten Radarzahlen. */
  static constexpr double kGenericRangeNm = 40.0;
  static constexpr double kGenericAzHalfDeg = 30.0;
  static constexpr double kGenericElHalfDeg = 10.5;
  static constexpr double kGenericFrameS = 2.0;

  /* Zwei aufeinanderfolgende Looks: die kleinste Zahl, die Erfassung noch messbare ZEIT kosten laesst. */
  static constexpr int kHitsToFirm = 2;
  /* Coast = max(kMinCoastS, kCoastFrames · FrameS) — Frames fuer einen Suchsweep, SEKUNDEN fuer ein
   * Starren: ein STT mit 0,1-s-Frame fiele sonst 0,3 s nach der Gimbal-Grenze. */
  static constexpr double kCoastFrames = 3.0;
  static constexpr double kMinCoastS = 1.0;
  /* Eine Abfrage ist eine Aussendung — jeden Kontakt bei jedem Look zu challengen straehlte zu viel. */
  static constexpr double kIffPeriodS = 5.0;
  /* [SET] Die Keule im Single-Target-Track: ein BLEISTIFT, nicht die Gimbal-Huelle. Folge: ein
   * Verfolgungsstrahl warnt genau einen, ein Suchsweep alles in seinem Volumen. */
  static constexpr double kTrackBeamHalfDeg = 3.0;
  /* [SET] Der Doppler-Notch, die EINE Zahl, an der das ganze Chaff-Modell haengt: eine Wolke ohne
   * Eigengeschwindigkeit liegt im Clutterfilter, ein Ziel ist genau solange unterscheidbar, wie seine
   * eigene Radialgeschwindigkeit ausserhalb liegt. doc/flightbox/sensors.md, Abschnitt 4.7. */
  static constexpr double kDopplerNotchMs = 40.0;
  /* [SET] Das Dwell, ueber das der Notch-Test misst. Zwingend, weil Posen 10 Hz publiziert werden und
   * ein Sucher mit 20 Hz schaut — die Differenz zweier Looks alterniert sonst (gemessen: 446 m/s gegen
   * wahre 654 m/s). Getrennt von FBRadarContact::ClosureMs, das die look-zu-look-Zahl bleibt. */
  static constexpr double kDopplerDwellS = 0.2;

  ~FBRadarSystem() override = default;

  /* Boot-Wiring: wer dieses Flugzeug IST — eigenes Echo ueberspringen, eigene IFF-Crypto kennen. */
  void SetIdentity(int selfId, FBUnitTeam team) { SelfId_ = selfId; SelfTeam_ = team; }

  void SetPowered(bool on);
  bool Powered() const { return Powered_; }

  /* Kampfschaden: halbe Apertur ueber die Radargleichung (core/FBDamageModel::kRadarRangeDegraded).
   * Der Faktor gilt fuer JEDES Muster UND fuer die gemeldete Emission — was das Set sieht und was es
   * ankuendigt bleibt eine Sache. 1 = unbeschaedigt. */
  void SetRangeFactor(double f) { RangeFactor_ = f > 0.0 ? f : 0.0; }
  double RangeFactor() const { return RangeFactor_; }

  /* Die IFF-Box (AN/APX-113) sitzt hier statt in einem eigenen Slot, weil das Einzige, was in diesem
   * Simulator je etwas abfragt, ein Radarkontakt ist. Die Transponderhaelfte wird in der
   * Einheiten-Signatur publiziert wie der XMT-Schalter: beobachtbar, nicht privat. */
  void SetIffTransponder(bool on) { IffXpdr_ = on; }
  void SetIffInterrogator(bool on) { IffInterrogator_ = on; }
  bool IffTransponder() const { return IffXpdr_; }
  bool IffInterrogator() const { return IffInterrogator_; }

  /* Das konfigurierte SUCH-Muster — was ActiveVolume() ohne Override liefert. */
  void SetSearchVolume(const FBRadarScanVolume &v) { Search_ = v; }
  const FBRadarScanVolume &SearchVolume() const { return Search_; }

  bool Locked() const { return LockedNum_ != 0; }
  int LockedTrackNum() const { return LockedNum_; }

  /* Der PILOTEN-Lock (TMS vorwaerts). `trackNum` = die VEROEFFENTLICHTE Track-Nummer, nie eine Unit-Id;
   * 0 = loesen. Ein DESIGNIERTER Lock, der verlorengeht, faellt in die SUCHE zurueck statt sich den
   * naechsten Kontakt zu greifen. doc/flightbox/sensors.md, Abschnitt 4.5. */
  bool Designate(int trackNum, double simTimeS);

  /* `simTimeS` ist ABSOLUT, damit das Bild nicht am Slot-Takt haengt; `net` null = nichts zu sehen. */
  virtual void Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net, double simTimeS);

  /* Aus dem gerade geflogenen Muster ABGELEITET — ein Radar kann nicht das eine strahlen und das andere
   * melden, also kann kein Modul die beiden auseinanderlaufen lassen. */
  FBEmitterSignature Emission() const;

  const char *TelemetryName() const override { return "fcr"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

protected:
  /* DER Override-Punkt: das Muster, das JETZT gilt. Ein ganzer Modus-Satz ist nichts als eine Auswahl
   * unter Volumina, ein Lock nichts als ein anderes Volumen — ein virtueller Getter traegt eine FCR. */
  virtual const FBRadarScanVolume &ActiveVolume() const { return Search_; }

  /* Nur Telemetrie-Label (0 = generische Suche); NICHT in FBRadarContact, weil ein Kontakt nichts
   * ueber den Modus weiss, der ihn fand. */
  virtual int ModeOrdinal() const { return 0; }

  /* Was fuer eine BOX hinter der Antenne sitzt, wie ein fremder Warnempfaenger sie einordnet — Hook,
   * weil ein Raketensucher genau das aendert. */
  virtual FBEmitterKind EmitterKind() const { return FBEmitterKind::AirborneFireControl; }

  /* Schraegentfernung + WELT-Paar (Peilung, Elevationswinkel) + KOERPER-Paar (Az/El). Koerperbezogen
   * heisst: volle Roll/Nick/Gier-Rotation, also was die ANTENNE sieht. */
  static void RelativeLos(const fb_fdm_state &own, double tgtLatDeg, double tgtLonDeg, double tgtAltM,
                          double &rangeM, double &bearingDeg, double &elevAngleDeg, double &azDeg,
                          double &elDeg);

  /* JEDER Entfernungstest dieser Klasse laeuft hierdurch — ein beschaedigtes Set kann nicht in einem
   * Codepfad weiter sehen als in einem anderen. */
  double GateRangeM(const FBRadarScanVolume &v) const { return v.RangeM * RangeFactor_; }

private:
  /* Eine interne Trackakte. UnitId ist der Korrelationsschluessel von Look zu Look und verlaesst dieses
   * Objekt NIE — publiziert wird der anonyme FBRadarContact aus dem Rest. */
  struct Track {
    int    UnitId = 0;
    int    TrackNum = 0;
    double RangeM = 0.0, BearingDeg = 0.0, ElevAngleDeg = 0.0, AzDeg = 0.0, ElDeg = 0.0;
    double ClosureMs = 0.0;
    double LastLookS = 0.0;     /* Simzeit der letzten echten Detektion */
    double PrevRangeM = 0.0, PrevLookS = 0.0;   /* das Paar, aus dem die Annaeherungsrate differenziert wird */
    /* Eigenes Referenzpaar des Notch-Tests, mindestens kDopplerDwellS gehalten. */
    double DopplerRefRangeM = 0.0, DopplerRefS = 0.0;
    double LastIffS = -1e9;
    int    Hits = 0;
    bool   Firm = false;
    bool   Seduced = false;   /* der letzte Look mass eine Chaff-Wolke statt des Flugzeugs */
    FBIffReply Iff = FBIffReply::NotInterrogated;
  };

  /* Liefert die Wolke, die dieser Look statt des Flugzeugs misst, oder null. Beide Eingaben sind EIGENE
   * Messungen; uebergeben wird das Wolken-Array statt der ganzen Signatur, weil ein Reflektor alles ist,
   * worueber ein Radar hier zu entscheiden hat. */
  const FBChaffCloud *SelectDecoy(const FBChaffCloud *clouds, const fb_fdm_state &st,
                                  const FBRadarScanVolume &v, double simTimeS) const;
  /* Eigengeschwindigkeit auf die Sichtlinie projiziert (+ = Annaeherung an einen STEHENDEN Punkt):
   * die Clutter-Doppler, gegen die der Notch gemessen wird. */
  static double OwnClosureOn(const fb_fdm_state &st, double bearingDeg, double elevAngleDeg);

  void ScanFrame(const fb_fdm_state &st, const FBUnitRegistry &net, double simTimeS);
  void UpdateLock(double simTimeS, bool autoAcquire);
  void DropAllTracks(const char *reason, double simTimeS);
  /* Mode-4: gueltig nur von einem eingeschalteten Transponder mit der Crypto DERSELBEN Koalition. Alles
   * andere ist ein und dasselbe NoReply. Die einzige Zeile der Klasse, die eine Fraktion liest — und sie
   * macht daraus eine zweiwertige Antwort, die keinen Feind benennen kann. */
  FBIffReply Interrogate(const FBUnit &u) const;

  Track Tracks_[kMaxRadarContacts]{};
  int TrackCount_ = 0;
  int NextTrackNum_ = 1;
  int LockedNum_ = 0;
  bool Designated_ = false;   /* der gehaltene Lock ist der des PILOTEN, keine ACM-Auto-Erfassung */

  int SelfId_ = 0;
  FBUnitTeam SelfTeam_ = FBUnitTeam::Friendly;
  bool Powered_ = true;
  double RangeFactor_ = 1.0;     /* Kampfschaden, s. SetRangeFactor */
  bool IffXpdr_ = true;         /* IFF Master NORM beim Start (doc/f16/procedures-startup.md, Schritt 46) */
  bool IffInterrogator_ = true;
  FBRadarScanVolume Search_{};
  double NextScanS_ = 0.0;       /* das eigene Frame-Raster der Antenne, unabhaengig vom Slot-Takt */

  /* Telemetrie braucht Zahlen, keine Tabelle. -1 = nichts zu melden. */
  float LockNm_ = -1.0f, LockAzDeg_ = 0.0f, LockElDeg_ = 0.0f, LockClosureKt_ = 0.0f;
  float LockAgeS_ = 0.0f;
  int LockIff_ = 0;
  int ContactCount_ = 0;
};

} // namespace FlightBox
#endif
