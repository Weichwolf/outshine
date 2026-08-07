/* The COUNTERMEASURE value types: a dispense program as DATA, and the chaff cloud a cartridge leaves.
 * The program schema is the AN/ALE-47's own, field for field and range for range — that designation is
 * the RANGES' provenance and stays; a program is loadout data, not behaviour.
 * The CLOUD carries the entire physics a radar sees: a large return, and NO velocity of its own (it
 * does not move — outshine has no wind field). doc/core.md, Abschnitt 8.5. */
#ifndef COUNTERMEASURE_H
#define COUNTERMEASURE_H

#include <cstdint>

namespace outshine {

/* Only the two expendables this airframe carries — the panel's OTHER1/OTHER2 have no function. */
enum class CmType : uint8_t { Chaff = 0, Flare };

/* The CMDS mode knob. Ordinals are telemetry-visible — append, never reorder. */
enum class CmdsMode : uint8_t { Off = 0, Stby, Man, Semi, Auto, Byp };

/* The panel's 3-state status display: powered-but-failed, ready, ready-and-awaiting-consent. */
enum class CmdsStatus : uint8_t { NoGo = 0, Go, DispenseReady };

inline const char *CmdsModeStr(CmdsMode m) {
  switch (m) {
    case CmdsMode::Off: return "off";
    case CmdsMode::Stby: return "stby";
    case CmdsMode::Man: return "man";
    case CmdsMode::Semi: return "semi";
    case CmdsMode::Auto: return "auto";
    case CmdsMode::Byp: return "byp";
  }
  return "?";
}

inline bool CmdsModeFromString(const char *s, CmdsMode &out) {
  for (int i = 0; i <= (int)CmdsMode::Byp; i++) {
    CmdsMode m = (CmdsMode)i;
    const char *n = CmdsModeStr(m);
    const char *a = s, *b = n;
    while (*a && *a == *b) { a++; b++; }
    if (!*a && !*b) { out = m; return true; }
  }
  return false;
}

/* One type's half of a program — the AN/ALE-47 DED page's four fields with its documented ranges. The
 * SCHEMA is that one dispenser's, field for field and range for range, and it stays that way until a second dispenser
 * with a different page exists to generalise it against: a schema invented for a device nobody has
 * modelled would be a guess wearing the shape of a fact. verify-types `value`. */
struct CmProgramType {
  int    BurstQty = 0;           /* BQ, 0..99 cartridges per salvo (0 = type not in this program) */
  double BurstIntervalS = 0.1;   /* BI, 0.020..10.000 s between cartridges */
  int    SalvoQty = 0;           /* SQ, 0..99 salvos (0 = type not in this program) */
  double SalvoIntervalS = 1.0;   /* SI, 0.50..150.00 s between salvos */

  bool Present() const { return BurstQty > 0 && SalvoQty > 0; }
  bool Valid() const {
    return BurstQty >= 0 && BurstQty <= 99 && SalvoQty >= 0 && SalvoQty <= 99 &&
           BurstIntervalS >= 0.020 && BurstIntervalS <= 10.000 &&
           SalvoIntervalS >= 0.50 && SalvoIntervalS <= 150.00;
  }
  int Cartridges() const { return Present() ? BurstQty * SalvoQty : 0; }
};

struct CmProgram {
  CmProgramType Chaff;
  CmProgramType Flare;

  bool Valid() const { return Chaff.Valid() && Flare.Valid(); }
  bool Empty() const { return !Chaff.Present() && !Flare.Present(); }
};

/* The freshest kMaxChaffClouds cartridges are published in the unit's emission signature; older ones
 * are the dispersed ones and are the right thing to lose. */
constexpr int kMaxChaffClouds = 8;

/* [SET]: nothing before bloom, full strength at bloom, linear decay to nothing at kChaffLifeS. The
 * sources document dispense PARAMETERS, never bloom or persistence times. */
constexpr double kChaffBloomS = 0.3;
constexpr double kChaffLifeS = 8.0;

struct ChaffCloud {
  bool   Active = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;   /* where it was dispensed; it does not move */
  double BloomS = 0.0;                             /* sim time of the ejection */
};

/* Free function because both sides need the SAME curve: the dispenser to know when a cloud has stopped
 * counting, the radar to weigh two clouds against each other. */
inline double ChaffRcsNorm(double ageS) {
  if (ageS < 0.0 || ageS >= kChaffLifeS) return 0.0;
  if (ageS < kChaffBloomS) return 0.0;   /* packed bundle: not a reflector yet */
  return 1.0 - (ageS - kChaffBloomS) / (kChaffLifeS - kChaffBloomS);
}

/* ---- THE INFRARED HALF. Structurally the chaff cloud's twin — same publication path, same ring, same
 * "it does not move" — and different in the ONE thing the two expendables differ in physically: a
 * chaff cloud REFLECTS somebody else's transmitter, a flare RADIATES on its own. So the age curve is
 * not a reflectivity but an intensity, and it is the shape of a pyrotechnic composition rather than of
 * a dispersing dipole cloud: a fast rise to a peak, then a long decay to burnout. */
constexpr int kMaxFlareClouds = 8;

/* [SET] Ignition, peak and burnout. No source in the tree states a flare's radiometric time history;
 * the sources that exist document dispense PARAMETERS only, exactly as for chaff. The shape is the one
 * a decoy grain has (it is lit by an igniter and burns out), the numbers are the order of magnitude of
 * a magnesium/Teflon cartridge. */
constexpr double kFlareIgniteS = 0.15;   /* ejection to full radiation */
constexpr double kFlareLifeS = 4.0;      /* burnout: after this it radiates nothing at all */

/* [SET] Peak radiant intensity of ONE cartridge, expressed in the only currency the seeker has: the
 * intensity of a clean, unaugmented aircraft seen from DEAD ASTERN (sensors/IrstSystem's rear-aspect
 * reference, which is what its documented reach figure is stated for). 1.0 therefore means "as bright
 * as a nozzle, seen straight into it" — the calibration that makes every seduction outcome in the tree
 * a consequence of ASPECT rather than of this number:
 *   head-on, dry   : the aircraft radiates (10/25)^2 = 0.16 of the reference -> a flare is 6x brighter
 *   astern, dry    : 1.00                                                    -> a flare merely equals it
 *   astern, burner : 1.5^2 = 2.25                                            -> a flare loses by 2.25x
 * Stated this way so the number can be argued with instead of tuned. */
constexpr double kFlarePeakIntensity = 1.0;

struct FlareCloud {
  bool   Active = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;   /* where it was ejected; it does not move */
  double BloomS = 0.0;                             /* sim time of the ejection */
};

/* The same free-function reason as ChaffRcsNorm: the dispenser needs to know when a cartridge has
 * stopped counting and the SEEKER needs to weigh it against an aircraft — one curve, two readers. */
inline double FlareIrNorm(double ageS) {
  if (ageS < 0.0 || ageS >= kFlareLifeS) return 0.0;
  if (ageS < kFlareIgniteS) return ageS / kFlareIgniteS;   /* the igniter, not yet the grain */
  return 1.0 - (ageS - kFlareIgniteS) / (kFlareLifeS - kFlareIgniteS);
}

} // namespace outshine
#endif
