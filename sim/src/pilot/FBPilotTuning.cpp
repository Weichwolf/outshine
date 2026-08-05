#include "FBPilotTuning.h"

namespace FlightBox::Pilot {

namespace {
/* Die ganze oeffentliche Flaeche einer Piloten-Variante in EINER Tabelle. Die Baender sind
 * Plausibilitaetsgelaender, kein Geschmack: ein Turnier darf eine schlechte Idee versuchen, es darf
 * nicht 6.000 nm versuchen. Bedeutung je Schluessel: doc/pilot.md, Abschnitt 9. */
constexpr FBPilotKey kParams[] = {
  {"pilot_speed_kt",       FBPilotParam::InterceptSpeedKt, FBPilotKeyKind::Free, 150.0,  900.0, ""},
  /* [SET] Kein Feuerleitgeraet dieses Baums torrt weiter; jenseits davon ist die Zahl ein Vertipper und
   * keine Doktrin. Das ECHTE Tor ist das des jeweiligen Geraets und lehnt einen weiteren Lock ohnehin
   * ab — dieses Gelaender braucht die Zahl deshalb nicht von einem Muster zu leihen. */
  {"pilot_lock_nm",        FBPilotParam::LockRangeNm,      FBPilotKeyKind::Free,   1.0,  100.0, ""},
  {"pilot_shot_rtr",       FBPilotParam::ShotRtrFactor,    FBPilotKeyKind::Free,   0.1,    3.0, ""},   /* >1 = jenseits von Rtr */
  {"pilot_shot_ata_deg",   FBPilotParam::ShotAtaDeg,       FBPilotKeyKind::Free,   1.0,   60.0, ""},   /* der Kardanwinkel */
  {"pilot_shot_spacing_s", FBPilotParam::ShotSpacingS,     FBPilotKeyKind::Free,   0.0,  120.0, ""},
  {"pilot_crank_deg",      FBPilotParam::CrankAtaDeg,      FBPilotKeyKind::Free,   0.0,   60.0, ""},
  {"pilot_abort_nm",       FBPilotParam::AbortRangeNm,     FBPilotKeyKind::Free,   0.0,   40.0, ""},
  {"pilot_beam_deg",       FBPilotParam::BeamOffsetDeg,    FBPilotKeyKind::Free,   0.0,  180.0, ""},
  {"pilot_chaff_s",        FBPilotParam::ChaffIntervalS,   FBPilotKeyKind::Free,   0.2,   60.0, ""},
  {"pilot_defend_hold_s",  FBPilotParam::DefendHoldS,      FBPilotKeyKind::Free,   0.0,  120.0, ""},
  {"pilot_react_s",        FBPilotParam::ReactionS,        FBPilotKeyKind::Free,   0.0,   30.0, ""},
  {"pilot_action_s",       FBPilotParam::ActionSpacingS,   FBPilotKeyKind::Free,   0.1,   30.0, ""},
  /* Obergrenze = der laengste vom Geschuetz honorierte Feuerstoss (core/FBGun.h, MaxBurstS). */
  {"pilot_gun_burst_s",    FBPilotParam::GunBurstS,        FBPilotKeyKind::Free,   0.1,    1.0, ""},
  {"pilot_gun_tol_frac",   FBPilotParam::GunFireTolFrac,   FBPilotKeyKind::Free,  0.05,    1.0, ""},
  {"pilot_bfm_ctrl_min_nm", FBPilotParam::BfmCtrlMinNm,    FBPilotKeyKind::Free,  0.05,    5.0, ""},
  {"pilot_bfm_ctrl_max_nm", FBPilotParam::BfmCtrlMaxNm,    FBPilotKeyKind::Free,  0.05,   10.0, ""},
  /* Bewusst WEIT und VORZEICHENBEHAFTET: der Parameter fuer einen ABSICHTLICH falschen Abwurf. */
  {"pilot_attack_bias_s",  FBPilotParam::AttackBiasS,      FBPilotKeyKind::Free, -10.0,   10.0, ""},
  {"pilot_attack_ccip_m",  FBPilotParam::AttackCcipTolM,   FBPilotKeyKind::Free,   1.0, 2000.0, ""},
  /* --- DIE GENE (doc/doctrine-evolution.md Abschnitt 2.1). Beide `Scale`, also VIELFACHE einer Zahl,
   * die dem Flugzeug bzw. seiner Waffe gehoert — die Zahl selbst kommt hier nie vor. G4: das Band
   * laeuft von unterhalb BfmMinSpeedKt (300/380 = 0,79) bis ueber die Uebergeschwindigkeitsklemme
   * (1,15). G2: 0 = Regel aus, 1,0 = genau eine Bindung. */
  {"pilot_energy_frac",    FBPilotParam::BfmEnergyFrac,    FBPilotKeyKind::Scale,  0.7,    1.2, "BfmCornerSpeedKt"},
  {"pilot_cover_frac",     FBPilotParam::CoverFrac,        FBPilotKeyKind::Scale,  0.0,    3.0, "weapon ttaS"},
  /* G1, die FORM des Verbands (doc/formation.md F5a). Drei Verhaeltnisse, kein Meter: quer, laengs und
   * hoch, jedes ein Vielfaches des Hakens, dem die Laenge gehoert. 0 im Trail ist Line abreast, also
   * die Form, die dieser Baum bis heute als einzige fliegen konnte. */
  {"pilot_flight_spread_frac", FBPilotParam::FlightSpreadFrac, FBPilotKeyKind::Scale, 0.25, 3.0, "FormationSpreadM"},
  {"pilot_flight_trail_frac",  FBPilotParam::FlightTrailFrac,  FBPilotKeyKind::Scale, 0.0,  3.0, "FormationTrailM"},
  {"pilot_flight_stack_frac",  FBPilotParam::FlightStackFrac,  FBPilotKeyKind::Scale, 0.0,  3.0, "FormationStackM"},
  /* G5, das EMISSIONS-TIMING (doc/duels.md D3c). Vielfaches der eigenen Erfassungsreichweite dieses
   * Flugzeugs: 1,0 = strahlen, sobald ueberhaupt etwas darin sein koennte; kleiner = laenger still auf
   * fremdem Bild; 3,0 = praktisch Dauerstrahlen, also die Form vor dieser Runde. */
  {"pilot_emcon_frac",     FBPilotParam::EmconFrac,        FBPilotKeyKind::Scale,  0.0,    3.0, "EmconRadiateNm"},
};

/* DIE STRUKTURELLE SCHRANKE, und sie ist eine `static_assert` und keine Konvention: ein `Scale`-Band
 * ist dimensionslos, also klein und nicht-negativ, und es MUSS den Haken nennen, dessen Vielfaches es
 * ist. Ein Gen, das eine Geschwindigkeit (380), eine Entfernung (16) oder eine Masse tragen wollte,
 * kommt an dieser Zeile nicht vorbei — es uebersetzt nicht. Die Obergrenze ist die des weitesten
 * dimensionslosen Bandes, das die Tabelle schon vorher trug (`pilot_shot_rtr`, 0,1…3,0). */
constexpr double kMaxScaleFactor = 3.0;

constexpr bool ScaleBandsAreDimensionless() {
  for (const FBPilotKey &e : kParams) {
    if (e.Kind != FBPilotKeyKind::Scale) continue;
    if (e.Lo < 0.0 || e.Hi > kMaxScaleFactor || e.Lo >= e.Hi) return false;
    if (e.Hook[0] == '\0') return false;
  }
  return true;
}
static_assert(ScaleBandsAreDimensionless(),
              "a Scale gene must be a dimensionless multiple of a NAMED airframe hook — "
              "doc/doctrine-evolution.md 2.2");

/* Und die Umkehrung: ein `Free`-Eintrag nennt keinen Haken, sonst waere die Unterscheidung Kosmetik. */
constexpr bool FreeKeysNameNoHook() {
  for (const FBPilotKey &e : kParams)
    if (e.Kind == FBPilotKeyKind::Free && e.Hook[0] != '\0') return false;
  return true;
}
static_assert(FreeKeysNameNoHook(), "a Free key is a pure pilot decision and scales nothing");

constexpr bool TableCoversEveryParam() {
  int n = 0;
  for (const FBPilotKey &e : kParams) { (void)e; n++; }
  return n == (int)FBPilotParam::Count;
}
static_assert(TableCoversEveryParam(), "every FBPilotParam needs exactly one .fbm key");
} // namespace

const FBPilotKey *FBPilotKeys() { return kParams; }
int FBPilotKeyCount() { return (int)(sizeof kParams / sizeof kParams[0]); }

bool FBPilotTuning::Set(const std::string &key, double value) {
  for (const FBPilotKey &e : kParams) {
    if (key != e.Key) continue;
    if (value < e.Lo || value > e.Hi) return false;
    Have_[(int)e.Param] = true;
    Value_[(int)e.Param] = value;
    return true;
  }
  return false;
}

} // namespace FlightBox::Pilot
