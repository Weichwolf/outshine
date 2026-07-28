/* FlightBox — FBGunSystem: der Bordkanonen-Slot und FBStoresSystems Geschwister. Der Abzug ist ein
 * KOMMANDO (Wert = DAUER des Abzugsdrucks), die Rate wird INTEGRIERT statt pro Tick gezaehlt, und die
 * Klasse wertet NIE einen eigenen Treffer — sie legt FBGunBurst-Saetze in eine Warteschlange, die der
 * BESITZER der Simulation leert. doc/flightbox/weapons-and-damage.md, Abschnitt 3. */
#ifndef FBGUNSYSTEM_H
#define FBGUNSYSTEM_H

#include "FBArmState.h"
#include "FBAvionicsCommand.h"
#include "FBGun.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBFdm.h"

namespace FlightBox::Weapons {

class FBGunSystem : public FBTelemetrySource {
public:
  /* Ein Buendel je Slot-Takt; der Besitzer leert die Queue jeden Tick. */
  static constexpr int kMaxPendingBursts = 4;

  ~FBGunSystem() override = default;

  /* Die Versaetze setzen die Muendung im Koerperrahmen (+vorn/+rechts/+unten vom CG, Meter), der Bore
   * mit Senkung/Schraenkung in Grad. Beides ist Wissen des Moduls. */
  void Install(const FBGunSpec &spec, double fwdM, double rightM, double downM, double boreDownDeg,
               double boreRightDeg);
  bool Installed() const { return Spec_ != nullptr; }
  const FBGunSpec *Spec() const { return Spec_; }

  void SetMasterArm(FBArmState s) { Arm_ = s; }
  FBArmState MasterArm() const { return Arm_; }
  void SetUnitId(int id) { SelfId_ = id; }
  /* Fuellt die Trommel. Eine Mission darf sie NIEDRIGER setzen, nie hoeher — eine Trommel ueber der
   * Kapazitaet waere eine andere Kanone. */
  bool SetRounds(int rounds);
  int  RoundsRemaining() const { return Rounds_; }
  int  RoundsFired() const { return Fired_; }

  /* Drueckt fuer `seconds` ab oder lehnt mit dem Grund ab, den der Jet geben wuerde. */
  bool Trigger(double seconds, double nowS, FBCommandOutcome &outcome, FBCommandReason &reason);
  bool Firing() const { return FireUntilS_ > 0.0; }

  /* Der Drain des Besitzers: das aelteste noch nicht genommene Buendel, FIFO. */
  bool TakeBurst(FBGunBurst &out);

  /* Aus `st` kommen Muendungsposition und Startgeschwindigkeit — mehr liest diese Klasse dort nicht. */
  virtual void Run(FBState &state, const Fdm::fb_fdm_state &st, double nowS, double dt);

  const char *TelemetryName() const override { return "gun"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  const FBGunSpec *Spec_ = nullptr;
  double MuzzleFwdM_ = 0.0, MuzzleRightM_ = 0.0, MuzzleDownM_ = 0.0;
  double BoreDownDeg_ = 0.0, BoreRightDeg_ = 0.0;

  FBArmState Arm_ = FBArmState::Sim;   /* ein Jet faehrt mit Master Arm SAFE hoch */
  int    SelfId_ = 0;
  int    Rounds_ = 0, Fired_ = 0;
  bool   Wow_ = true;                  /* bis die Luftdaten anderes sagen: am Boden */
  double FireUntilS_ = 0.0;            /* Ende des Abzugsdrucks, absolute Simzeit; 0 = feuert nicht */
  double FireStartS_ = 0.0;            /* ...und sein Beginn, fuer die Hochlauframpe */
  double Fraction_ = 0.0;              /* geschuldete, noch nicht ganze Schuesse */
  int    Triggers_ = 0, Refused_ = 0;
  double LastBurstRounds_ = 0.0;
  int    BlockStatus_ = 0;
  /* Die Zielloesung, nur fuer die Telemetrie vom Feuerleitblock gelesen. -1 = keine. */
  float  SolRangeM_ = -1.0f, SolAimErrDeg_ = -1.0f, SolSpanMr_ = -1.0f;
  int    SolInFunnel_ = 0;

  FBGunBurst Pending_[kMaxPendingBursts]{};
  int PendingCount_ = 0;
};

} // namespace FlightBox::Weapons
#endif
