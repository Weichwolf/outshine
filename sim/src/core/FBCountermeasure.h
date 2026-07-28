/* The COUNTERMEASURE value types: a dispense program as DATA, and the chaff cloud a cartridge leaves.
 * The program schema is the AN/ALE-47's own, field for field and range for range
 * (doc/modules/f16/defence-rwr-cm.md §2.2) — a program is mission/loadout data, not behaviour.
 * The CLOUD carries the entire physics a radar sees: a large return, and NO velocity of its own (it
 * does not move — FlightBox has no wind field). doc/core.md, Abschnitt 8.5. */
#ifndef FBCOUNTERMEASURE_H
#define FBCOUNTERMEASURE_H

#include <cstdint>

namespace FlightBox {

/* Only the two expendables this airframe carries — the panel's OTHER1/OTHER2 have no function. */
enum class FBCmType : uint8_t { Chaff = 0, Flare };

/* The CMDS mode knob. Ordinals are telemetry-visible — append, never reorder. */
enum class FBCmdsMode : uint8_t { Off = 0, Stby, Man, Semi, Auto, Byp };

/* The panel's 3-state status display: powered-but-failed, ready, ready-and-awaiting-consent. */
enum class FBCmdsStatus : uint8_t { NoGo = 0, Go, DispenseReady };

inline const char *FBCmdsModeStr(FBCmdsMode m) {
  switch (m) {
    case FBCmdsMode::Off: return "off";
    case FBCmdsMode::Stby: return "stby";
    case FBCmdsMode::Man: return "man";
    case FBCmdsMode::Semi: return "semi";
    case FBCmdsMode::Auto: return "auto";
    case FBCmdsMode::Byp: return "byp";
  }
  return "?";
}

inline bool FBCmdsModeFromString(const char *s, FBCmdsMode &out) {
  for (int i = 0; i <= (int)FBCmdsMode::Byp; i++) {
    FBCmdsMode m = (FBCmdsMode)i;
    const char *n = FBCmdsModeStr(m);
    const char *a = s, *b = n;
    while (*a && *a == *b) { a++; b++; }
    if (!*a && !*b) { out = m; return true; }
  }
  return false;
}

/* One type's half of a program — the DED page's four fields with its documented ranges. */
struct FBCmProgramType {
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

struct FBCmProgram {
  FBCmProgramType Chaff;
  FBCmProgramType Flare;

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

struct FBChaffCloud {
  bool   Active = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;   /* where it was dispensed; it does not move */
  double BloomS = 0.0;                             /* sim time of the ejection */
};

/* Free function because both sides need the SAME curve: the dispenser to know when a cloud has stopped
 * counting, the radar to weigh two clouds against each other. */
inline double FBChaffRcsNorm(double ageS) {
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

/* [SET] Ignition, peak and burnout. No source in the tree states a flare's radiometric time history —
 * doc/modules/f16/defence-rwr-cm.md documents dispense PARAMETERS only, exactly as for chaff. The
 * shape is the one a decoy grain has (it is lit by an igniter and burns out), the numbers are the
 * order of magnitude of a magnesium/Teflon cartridge. */
constexpr double kFlareIgniteS = 0.15;   /* ejection to full radiation */
constexpr double kFlareLifeS = 4.0;      /* burnout: after this it radiates nothing at all */

/* [SET] Peak radiant intensity of ONE cartridge, expressed in the only currency the seeker has: the
 * intensity of a clean, unaugmented aircraft seen from DEAD ASTERN (sensors/FBIrstSystem's rear-aspect
 * reference, which is what its documented reach figure is stated for). 1.0 therefore means "as bright
 * as a nozzle, seen straight into it" — the calibration that makes every seduction outcome in the tree
 * a consequence of ASPECT rather than of this number:
 *   head-on, dry   : the aircraft radiates (10/25)^2 = 0.16 of the reference -> a flare is 6x brighter
 *   astern, dry    : 1.00                                                    -> a flare merely equals it
 *   astern, burner : 1.5^2 = 2.25                                            -> a flare loses by 2.25x
 * Stated this way so the number can be argued with instead of tuned. */
constexpr double kFlarePeakIntensity = 1.0;

struct FBFlareCloud {
  bool   Active = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;   /* where it was ejected; it does not move */
  double BloomS = 0.0;                             /* sim time of the ejection */
};

/* The same free-function reason as FBChaffRcsNorm: the dispenser needs to know when a cartridge has
 * stopped counting and the SEEKER needs to weigh it against an aircraft — one curve, two readers. */
inline double FBFlareIrNorm(double ageS) {
  if (ageS < 0.0 || ageS >= kFlareLifeS) return 0.0;
  if (ageS < kFlareIgniteS) return ageS / kFlareIgniteS;   /* the igniter, not yet the grain */
  return 1.0 - (ageS - kFlareIgniteS) / (kFlareLifeS - kFlareIgniteS);
}

} // namespace FlightBox
#endif
