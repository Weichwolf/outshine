/* FlightBox — FBPilotTuning: die Piloten-VARIANTE als Missionsdaten, eine duenne Tabelle von
 * Ueberschreibungen, die `set pilot_* <wert>` fuellt. Ein NICHT gesetzter Eintrag ist keine Null,
 * sondern „die eigene Zahl dieses Piloten". doc/flightbox/pilot-ai.md, Abschnitt 9. */
#ifndef FBPILOTTUNING_H
#define FBPILOTTUNING_H

#include <string>

namespace FlightBox {

/* Nur anhaengen — die `.fbm`-Schluessel in FBPilotTuning.cpp sind die oeffentlichen Namen. */
enum class FBPilotParam {
  InterceptSpeedKt,
  LockRangeNm,
  ShotRtrFactor,
  ShotAtaDeg,
  ShotSpacingS,
  CrankAtaDeg,
  AbortRangeNm,
  BeamOffsetDeg,
  ChaffIntervalS,
  DefendHoldS,
  ReactionS,
  ActionSpacingS,
  GunBurstS,
  GunFireTolFrac,
  BfmCtrlMinNm,
  BfmCtrlMaxNm,
  AttackBiasS,
  AttackCcipTolM,
  Count
};

class FBPilotTuning {
public:
  /* `key` inklusive `pilot_`-Praefix. false bei unbekanntem Schluessel oder Wert ausserhalb des Bandes;
   * daraus macht der Aufrufer einen Missions-FAIL. */
  bool Set(const std::string &key, double value);

  bool Has(FBPilotParam p) const { return Have_[(int)p]; }
  /* Der getunte Wert, oder die eigene Zahl des Aufrufers, wenn diese Variante dazu nichts sagt. */
  double Or(FBPilotParam p, double own) const { return Have_[(int)p] ? Value_[(int)p] : own; }

private:
  bool   Have_[(int)FBPilotParam::Count]{};
  double Value_[(int)FBPilotParam::Count]{};
};

} // namespace FlightBox
#endif
