/* FlightBox — FBF16Ufc: the ICP/UFC/DED (doc/f16/cockpit-displays.md) reduced to what it actually owns
 * for now — the COMMITTED value of a handful of DED fields: the CARA ALOW floor, the BNGO fuel
 * threshold, the selected steerpoint number. It publishes them as the UFC block and is the system that
 * consumes the DED-class avionics commands for those fields (core/FBAvionicsCommand.h): the propose ->
 * commit/reject cycle documented in doc/f16/controls-commands.md §1.2 ends HERE.
 *
 * TWO documented failure shapes live in this class, and they are deliberately different:
 *   - BNGO CLAMP (§6.8): the entry is ACCEPTED — ENTR succeeds, the field shows what was typed — but the
 *     warning fires at a system ceiling regardless. Modelled as an accept-with-clamp, not a rejection,
 *     because that is what the jet does.
 *   - RANGE REJECT: neither source guide documents ANY numeric bounds check on a DED field, so the
 *     bounds below are FlightBox's own model decision (see the constants) and are reported as such —
 *     a rejection with FBCommandReason::OutOfRange, never a silent clamp. Silence is the one behaviour
 *     the material rules out: every documented DED failure is visible to the pilot.
 * F-16-specific: the ICP/DED is this airframe's own control head, not a generic module slot. */
#ifndef FBF16UFC_H
#define FBF16UFC_H

#include "FBState.h"

namespace FlightBox {

class FBF16Ufc {
public:
  /* The documented BNGO ceiling (doc/f16/controls-commands.md §6.8, FUEL QTY SEL = NORM): a higher
   * entry is taken but the warning still fires here. */
  static constexpr float kBingoCeilingLbs = 6070.0f;

  /* FlightBox's OWN range policy (class banner — no source documents one). Chosen as the widest domain
   * in which the field still means something: an ALOW above the jet's absolute ceiling is a floor that
   * can never be crossed, and a negative fuel threshold is not a quantity. Out of range = rejected and
   * reported, so a mis-briefed number fails loudly instead of arming a warning that never fires. */
  static constexpr float kAlowMinFt = 0.0f, kAlowMaxFt = 50000.0f;
  static constexpr float kBingoMinLbs = 0.0f, kBingoMaxLbs = 20000.0f;
  static constexpr int kSteerNumMin = 1, kSteerNumMax = 99;

  virtual ~FBF16Ufc() = default;

  static bool AlowInRange(float ft) { return ft >= kAlowMinFt && ft <= kAlowMaxFt; }
  static bool BingoInRange(float lbs) { return lbs >= kBingoMinLbs && lbs <= kBingoMaxLbs; }
  static bool SteerNumInRange(int n) { return n >= kSteerNumMin && n <= kSteerNumMax; }

  void SetAlow(float ft) { AlowFt = ft; }
  void SetSteerpointNumber(int n) { StNum = n; }
  /* Commits BNGO. Returns true if the committed value is also the EFFECTIVE one; false means the entry
   * stands but the ceiling above governs (the documented clamp, not a rejection). */
  bool SetBingo(float lbs) { BingoLbs = lbs; return lbs <= kBingoCeilingLbs; }

  float GetAlow() const { return AlowFt; }
  float GetBingo() const { return BingoLbs; }
  /* What the warning system actually compares fuel against — the clamp made explicit. */
  float EffectiveBingo() const { return BingoLbs > kBingoCeilingLbs ? kBingoCeilingLbs : BingoLbs; }

  virtual void Run(FBState &state, double dt);

private:
  float AlowFt = 500.0f;   /* MIL-STD-1787 pull-up floor placeholder — see doc/f16/aerodynamics-performance.md */
  float BingoLbs = 0.0f;   /* 0 = no threshold entered; the warning stays inhibited */
  int StNum = 1;
};

} // namespace FlightBox
#endif
