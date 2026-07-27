/* FlightBox — the COUNTERMEASURE value types: a dispense program as DATA, and the chaff cloud a
 * dispensed cartridge leaves behind.
 *
 * THE PROGRAM SCHEMA IS THE AN/ALE-47's OWN (doc/f16/defence-rwr-cm.md §2.2, "CMDS CHAFF/FLARE DED
 * pages"), field for field and range for range: per countermeasure TYPE a burst quantity (cartridges
 * in one salvo, 0-99), a burst interval (time between cartridges within a salvo, 0.020-10.000 s), a
 * salvo quantity (salvos in the program, 0-99) and a salvo interval (time between salvos,
 * 0.50-150.00 s). Zeroing a type's burst or salvo quantity removes that type from the program, which is
 * how a chaff-only or flare-only program is expressed — a rule of the real DED page, reproduced here
 * rather than replaced by a "type" flag. A program is therefore mission/loadout data, not behaviour.
 *
 * THE CLOUD IS WHAT MAKES THE PROGRAM MEAN ANYTHING. A dispensed chaff cartridge blooms into a cloud of
 * resonant dipoles that has, within about a second, lost essentially all of the aircraft's velocity and
 * hangs in the air mass. Those two facts — a large radar return, and NO velocity of its own — are the
 * entire physics a radar sees, and both live in this struct: the cloud's position is where it was
 * dispensed and it does not move (FlightBox has no wind field, so "stationary in the air mass" is
 * "stationary"), and its strength follows the age curve below.
 *
 * WHY THE AGE CURVE HAS THE SHAPE IT HAS [SET]: a cartridge is a packed bundle at ejection and is not a
 * useful reflector until it has bloomed, and it keeps growing/thinning afterwards until it is no longer
 * dense enough to compete with an aircraft's own return. So: nothing before kBloomS, full strength at
 * bloom, then a linear decay to nothing at kLifeS. The numbers are FlightBox's own — the source guides
 * document dispense PARAMETERS, never bloom or persistence times — and they are the two knobs that
 * decide how long a salvo protects you, which is why they sit here as named constants rather than
 * inside the radar that reads them. */
#ifndef FBCOUNTERMEASURE_H
#define FBCOUNTERMEASURE_H

#include <cstdint>

namespace FlightBox {

/* Only the two expendables this airframe actually carries: doc/f16/defence-rwr-cm.md §2.2 records that
 * the OTHER1/OTHER2 stations exist on the panel and have NO function. */
enum class FBCmType : uint8_t { Chaff = 0, Flare };

/* The CMDS mode knob (doc/f16/defence-rwr-cm.md §2.2's state-machine table). Ordinals are telemetry-
 * visible — append, never reorder. */
enum class FBCmdsMode : uint8_t { Off = 0, Stby, Man, Semi, Auto, Byp };

/* The panel's 3-state status display (§2.2): powered-but-failed, ready, and ready-and-waiting-for-
 * consent (the SEMI "Counter" prompt). */
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

/* One countermeasure type's half of a program — the DED page's four fields, with its documented
 * ranges (see the banner). */
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

/* ---- the dispensed chaff cloud (see the banner) ----
 * Fixed capacity, published in the dispensing unit's emission signature (units/FBUnit.h): the freshest
 * kMaxChaffClouds cartridges. Eight covers any program a salvo interval keeps inside one cloud
 * lifetime; older ones are the dispersed ones and are the right thing to lose. */
constexpr int kMaxChaffClouds = 8;

/* [SET] — see the banner. Bloom is fast (the cartridge is designed to open in the airstream), useful
 * life is the order of ten seconds before the cloud is too thin to hold a seeker. */
constexpr double kChaffBloomS = 0.3;
constexpr double kChaffLifeS = 8.0;

struct FBChaffCloud {
  bool   Active = false;
  double LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;   /* where it was dispensed; it does not move */
  double BloomS = 0.0;                             /* sim time of the ejection */
};

/* The cloud's radar strength relative to its own peak, 0..1 (banner's age curve). Free function
 * because both sides need the SAME curve: the dispenser to know when a cloud has stopped counting, the
 * radar to weigh two clouds against each other. */
inline double FBChaffRcsNorm(double ageS) {
  if (ageS < 0.0 || ageS >= kChaffLifeS) return 0.0;
  if (ageS < kChaffBloomS) return 0.0;   /* packed bundle: not a reflector yet */
  return 1.0 - (ageS - kChaffBloomS) / (kChaffLifeS - kChaffBloomS);
}

} // namespace FlightBox
#endif
