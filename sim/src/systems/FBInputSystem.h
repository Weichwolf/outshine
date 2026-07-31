/* FlightBox — FBInputSystem: die HANDS eines Menschen im Sitz, und der Slot, der bisher NoOp war.
 * Zwei Haelften, und die Trennung ist die ganze Klasse: die ANALOGE (Stick/Schub/Klappen) verlaesst
 * sie als Manual-Guidance ueber genau den Pfad, auf dem auch die Manual-Kommandos des Piloten laufen,
 * die DISKRETE (Master Arm, Stationswahl, Pickle, Abzug) NUR als FBCommandBus::Post. Es gibt keinen
 * dritten Weg — der Mensch bekommt dieselbe Latenz, dieselbe Belegung und denselben Ablehnungskatalog
 * wie die KI. doc/systems.md, Abschnitt 10; doc/player-layer.md §10.2. */
#ifndef FBINPUTSYSTEM_H
#define FBINPUTSYSTEM_H

#include <cstdint>
#include "FBCommandBus.h"
#include "FBMasterMode.h"

namespace FlightBox::Systems {

/* Eine Bedienhandlung, flankengetriggert: ein Mensch DRUECKT, er haelt keinen Zustand, den der Jet
 * abliest. Der Abzug ist die Ausnahme und deshalb kein Eintrag hier (s. SetTrigger). */
enum class FBHotasAction : uint8_t { None = 0, MasterArm, StationSelect, WeaponRelease };

/* Was die Haende gerade TUN. Vom Modul im Pilotentakt gelesen und durch dieselbe ApplyPilotCommands
 * geschickt, die die Manual-Guidance des Piloten anwendet. */
struct FBStickInput {
  double Roll = 0.0, Pitch = 0.0, Yaw = 0.0;   /* -1..1, FLCS-Kommandonormale */
  double Throttle = 0.0;                       /* 0..1 */
  double Speedbrake = 0.0, WheelBrake = 0.0;   /* 0..1 */
  bool   GearDown = true;
};

class FBInputSystem {
public:
  /* WIE SCHNELL EINE HAND EINEN KNUEPPEL BEWEGT, und die Zahl ist nicht neu: der Kommandobus benennt
   * mit kHotasLatencyS bereits das eine Mass dieses Baums dafuer, wie lange eine HOTAS-Handlung
   * dauert. Volle Auslenkung aus der Mitte kostet genau dieses Fenster; das Zentrieren ebenso. Eine
   * Taste ist ein Schalter, ein Knueppel ist es nicht — ohne Rampe waere jede Eingabe ein Sprung auf
   * den Anschlag, und das ist keine Steuerung, sondern ein Ausschlag. */
  static constexpr double kAxisRampS = FBCommandBus::kHotasLatencyS;
  /* EIN GEHALTENER ABZUG IST KEIN UNENDLICHER DRUCK, sondern dieselbe Handlung wiederholt. WANN sie
   * wieder erlaubt ist, sagt der Bus (SwitchReady) — es zu rechnen ging daneben, weil das Fenster ab
   * der VOLLENDUNG laeuft und deren Zeitpunkt die Taktrate der antwortenden Box ist. WIE LANG der Druck
   * ist, steht hier: die Sperre plus die Auslaufzeit des Kommandos, damit zwischen zwei Druecken keine
   * Luecke im Feuerstoss klafft. */
  static constexpr double kTriggerRepeatS = FBCommandBus::kHotasLatencyS + FBCommandBus::kTriggerLatencyS;
  static constexpr int kMaxPending = 8;

  virtual ~FBInputSystem() = default;

  /* DER SITZ. Bis ihn jemand einnimmt, tut diese Klasse nichts: ein Modul, das sie komponiert und von
   * seinem eigenen Piloten geflogen wird, ist bitgleich zu einem, das sie nie hatte.
   * Den Knueppel zu uebernehmen ist kein Schalter, sondern eine UEBERGABE: die Haende kommen an einem
   * Jet an, der bereits konfiguriert ist, und ein Sitz mit Vorgabewerten faehre in 6000 m das Fahrwerk
   * aus. Das Modul besitzt beide Seiten und saet den Slot deshalb im ersten Takt danach. */
  void TakeStick() { Engaged_ = true; Seeded_ = false; }
  void ReleaseStick();
  bool Engaged() const { return Engaged_; }
  bool NeedsSeed() const { return Engaged_ && !Seeded_; }
  void Seed(double throttle, double speedbrake, bool gearDown);

  /* Die ABSICHT je Achse als -1/0/+1: eine Taste kennt nur diese drei Stellungen, die Rampe in Run()
   * macht daraus eine Auslenkung. */
  void SetAxisIntent(int roll, int pitch, int yaw);
  void SetThrottleIntent(int t) { ThrIntent_ = Clamp3(t); }
  void SetGearDown(bool d) { Stick_.GearDown = d; }
  void SetSpeedbrake(double s) { Stick_.Speedbrake = Clamp01(s); }
  void SetWheelBrake(double b) { Stick_.WheelBrake = Clamp01(b); }
  /* Der Abzug ist der einzige HELD-Schalter der Zelle; alles andere hier ist eine Flanke. */
  void SetTrigger(bool held) { TriggerHeld_ = held; }

  /* Die DISKRETE Haelfte: eine gedrueckte Taste wird eingereiht und verlaesst dieses System AUSSCHLIESS-
   * LICH als Post auf den Kommandobus. False = die Warteschlange ist voll (der Mensch hat schneller
   * gedrueckt, als der Jet antworten kann), was der Client anzeigt statt zu verschlucken. */
  bool Press(FBHotasAction a, double value = 0.0);

  const FBStickInput &Stick() const { return Stick_; }
  int PostedCount() const { return Posted_; }

  /* Der Takt des Slots: Achsen rampen, dann die eingereihten Handlungen auf den Bus. */
  void Run(FBMasterMode mode, FBCommandBus &bus, double nowS, double dt);

protected:
  /* DER EINE UEBERSCHREIBUNGSPUNKT: welches Kommando eine Handlung auf DIESER Zelle in DIESEM
   * Master-Mode wird. Eingabe-Routing ist Modul-Hoheit, nicht global (doc/systems.md §10) — eine Zelle
   * mit anderem HOTAS haengt hier ihre Tabelle ein und teilt alles andere verbatim. */
  virtual FBCommandTarget Route(FBHotasAction a, FBMasterMode mode) const;

private:
  static int Clamp3(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }
  static double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
  static double Ramp(double cur, double want, double step);

  struct Action { FBHotasAction A = FBHotasAction::None; double Value = 0.0; };

  bool Engaged_ = false, Seeded_ = false;
  FBStickInput Stick_{};
  int RollIntent_ = 0, PitchIntent_ = 0, YawIntent_ = 0, ThrIntent_ = 0;
  bool TriggerHeld_ = false;
  double LastTriggerS_ = -kTriggerRepeatS;   /* so the first squeeze of a session is not held back */
  Action Queue_[kMaxPending]{};
  int Count_ = 0;
  int Posted_ = 0;
};

} // namespace FlightBox::Systems
#endif
