/* FlightBox — FBPilotTuning: die Piloten-VARIANTE als Missionsdaten, eine duenne Tabelle von
 * Ueberschreibungen, die `set pilot_* <wert>` fuellt. Ein NICHT gesetzter Eintrag ist keine Null,
 * sondern „die eigene Zahl dieses Piloten". doc/pilot.md, Abschnitt 9; die Genom-Grenze
 * doc/doctrine-evolution.md, Abschnitt 2.2. */
#ifndef FBPILOTTUNING_H
#define FBPILOTTUNING_H

#include <string>

namespace FlightBox::Pilot {

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
  BfmEnergyFrac,
  CoverFrac,
  Count
};

/* DIE GRENZE, und sie ist eine Frage der SYNTAX und nicht der Disziplin: ein `Scale`-Eintrag traegt
 * keinen Absolutwert, weil er die Klasse nur durch Scaled() verlaesst und dort mit der eigenen Zahl des
 * Flugzeugs multipliziert wird. Eine dimensionsbehaftete Zahl kann in so einem Schluessel nicht STEHEN. */
enum class FBPilotKeyKind { Free, Scale };

struct FBPilotKey {
  const char      *Key;
  FBPilotParam     Param;
  FBPilotKeyKind   Kind;
  double           Lo, Hi;
  const char      *Hook;   /* Scale: der Flugzeug-Haken, dessen VIELFACHES der Wert ist. Free: "" */
};

/* Das Alphabet als Daten — die EINE Quelle, aus der auch der Evolutionslaeufer sein Genom liest
 * (`fb-gym --pilot-keys`), damit keine zweite Kopie der Tabelle in einem Werkzeug entsteht. */
const FBPilotKey *FBPilotKeys();
int               FBPilotKeyCount();

class FBPilotTuning {
public:
  /* `key` inklusive `pilot_`-Praefix. false bei unbekanntem Schluessel oder Wert ausserhalb des Bandes;
   * daraus macht der Aufrufer einen Missions-FAIL. */
  bool Set(const std::string &key, double value);

  bool Has(FBPilotParam p) const { return Have_[(int)p]; }
  /* Der getunte Wert, oder die eigene Zahl des Aufrufers, wenn diese Variante dazu nichts sagt. */
  double Or(FBPilotParam p, double own) const { return Have_[(int)p] ? Value_[(int)p] : own; }
  /* Der EINZIGE Weg, auf dem ein `Scale`-Eintrag die Klasse verlaesst: immer als Vielfaches von `own`,
   * nie als Zahl fuer sich. `own` bleibt damit in der Klasse des Flugzeugs, dem sie gehoert. */
  double Scaled(FBPilotParam p, double own) const { return own * Or(p, 1.0); }

private:
  bool   Have_[(int)FBPilotParam::Count]{};
  double Value_[(int)FBPilotParam::Count]{};
};

} // namespace FlightBox::Pilot
#endif
