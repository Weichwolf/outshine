#include "FBInputSystem.h"

namespace FlightBox::Systems {

double FBInputSystem::Ramp(double cur, double want, double step) {
  double d = want - cur;
  if (d > step) d = step;
  if (d < -step) d = -step;
  return cur + d;
}

void FBInputSystem::ReleaseStick() {
  Engaged_ = false;
  Seeded_ = false;
  Stick_ = FBStickInput{};
  RollIntent_ = PitchIntent_ = YawIntent_ = ThrIntent_ = 0;
  TriggerHeld_ = false;
  LastTriggerS_ = -kTriggerRepeatS;
  Count_ = 0;
}

void FBInputSystem::Seed(double throttle, double speedbrake, bool gearDown) {
  Stick_.Throttle = Clamp01(throttle);
  Stick_.Speedbrake = Clamp01(speedbrake);
  Stick_.GearDown = gearDown;
  Seeded_ = true;
}

void FBInputSystem::SetAxisIntent(int roll, int pitch, int yaw) {
  RollIntent_ = Clamp3(roll);
  PitchIntent_ = Clamp3(pitch);
  YawIntent_ = Clamp3(yaw);
}

bool FBInputSystem::Press(FBHotasAction a, double value) {
  if (!Engaged_ || a == FBHotasAction::None || Count_ >= kMaxPending) return false;
  Queue_[Count_++] = {a, value};
  return true;
}

/* Der Master-Mode geht in die Vorgabe-Tabelle NICHT ein, und das ist eine Aussage: der Pickle ist auf
 * dieser Zelle in jedem Mode der Pickle. Ob er im gerade gewaehlten Mode etwas BEWIRKT, beantwortet die
 * Box, der er gilt (OutOfContext) — nicht die Hand, die ihn drueckt. */
FBCommandTarget FBInputSystem::Route(FBHotasAction a, FBMasterMode mode) const {
  (void)mode;
  switch (a) {
    case FBHotasAction::MasterArm: return FBCommandTarget::MasterArm;
    case FBHotasAction::StationSelect: return FBCommandTarget::StationSelect;
    case FBHotasAction::WeaponRelease: return FBCommandTarget::WeaponRelease;
    case FBHotasAction::None: break;
  }
  return FBCommandTarget::None;
}

void FBInputSystem::Run(FBMasterMode mode, FBCommandBus &bus, double nowS, double dt) {
  if (!Engaged_) return;

  double step = (dt > 0.0) ? dt / kAxisRampS : 1.0;
  Stick_.Roll = Ramp(Stick_.Roll, RollIntent_, step);
  Stick_.Pitch = Ramp(Stick_.Pitch, PitchIntent_, step);
  Stick_.Yaw = Ramp(Stick_.Yaw, YawIntent_, step);
  /* Der Schub bleibt stehen, wo er hingeschoben wurde — ein Knueppel zentriert sich, ein Schubhebel
   * nicht. Deshalb ist die Absicht 0 hier ein HALTEN und dort ein Zurueckfedern. */
  double thrWant = ThrIntent_ > 0 ? 1.0 : (ThrIntent_ < 0 ? 0.0 : Stick_.Throttle);
  Stick_.Throttle = Ramp(Stick_.Throttle, thrWant, step);

  /* Ein gehaltener Abzug ist dieselbe Handlung wiederholt, und ZWEI Bedingungen halten daraus einen
   * Finger statt acht. Der eigene Abstand deckt die Zeit VOR der ersten Antwort ab — die Sperre des
   * Busses laeuft ab der Vollendung, vorher ist der Schalter fuer ihn frei, und ohne diese Zeile
   * fuellte ein einziger Tastendruck die ganze Warteschlange. Die Frage AN den Bus deckt die Zeit
   * DANACH ab, weil die Vollendung so spaet faellt, wie die antwortende Box getaktet ist, und Raten
   * kostete je Wiederholung ein ChannelBusy. */
  if (TriggerHeld_ && nowS - LastTriggerS_ >= kTriggerRepeatS &&
      bus.SwitchReady(FBCommandTarget::GunTrigger, nowS)) {
    LastTriggerS_ = nowS;
    bus.Post(FBCommandTarget::GunTrigger, kTriggerRepeatS, nowS);
    Posted_++;
  }

  for (int i = 0; i < Count_; i++) {
    FBCommandTarget t = Route(Queue_[i].A, mode);
    if (t == FBCommandTarget::None) continue;
    bus.Post(t, Queue_[i].Value, nowS);
    Posted_++;
  }
  Count_ = 0;
}

} // namespace FlightBox::Systems
