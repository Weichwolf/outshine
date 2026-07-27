/* FlightBox — FBF16Ufc: this airframe's ICP/UFC/DED, reduced to the COMMITTED value of three DED fields
 * (ALOW floor, BNGO threshold, selected steerpoint). The propose -> commit/reject cycle of the DED-class
 * avionics commands ends HERE. Two deliberately DIFFERENT failure shapes: the BNGO clamp is an
 * accept-with-clamp because that is what the jet does, an out-of-range entry is a REJECT because
 * silence is the one behaviour the material rules out. doc/flightbox/modules-f16.md §9.3. */
#ifndef FBF16UFC_H
#define FBF16UFC_H

#include "FBState.h"

namespace FlightBox {

class FBF16Ufc {
public:
  /* [DOC] §6.8 (FUEL QTY SEL = NORM): a higher entry is taken, but the warning still fires here. */
  static constexpr float kBingoCeilingLbs = 6070.0f;

  /* FlightBox's OWN range policy [SET] — no source documents one. The widest domain in which the field
   * still MEANS something, so a mis-briefed number fails loudly instead of arming a dead warning. */
  static constexpr float kAlowMinFt = 0.0f, kAlowMaxFt = 50000.0f;
  static constexpr float kBingoMinLbs = 0.0f, kBingoMaxLbs = 20000.0f;
  static constexpr int kSteerNumMin = 1, kSteerNumMax = 99;

  virtual ~FBF16Ufc() = default;

  static bool AlowInRange(float ft) { return ft >= kAlowMinFt && ft <= kAlowMaxFt; }
  static bool BingoInRange(float lbs) { return lbs >= kBingoMinLbs && lbs <= kBingoMaxLbs; }
  static bool SteerNumInRange(int n) { return n >= kSteerNumMin && n <= kSteerNumMax; }

  void SetAlow(float ft) { AlowFt = ft; }
  void SetSteerpointNumber(int n) { StNum = n; }
  /* False = the entry STANDS but the ceiling governs (the documented clamp, not a rejection). */
  bool SetBingo(float lbs) { BingoLbs = lbs; return lbs <= kBingoCeilingLbs; }

  float GetAlow() const { return AlowFt; }
  float GetBingo() const { return BingoLbs; }
  /* What the warning system compares fuel against — the clamp made explicit. */
  float EffectiveBingo() const { return BingoLbs > kBingoCeilingLbs ? kBingoCeilingLbs : BingoLbs; }

  virtual void Run(FBState &state, double dt);

private:
  float AlowFt = 500.0f;   /* MIL-STD-1787 pull-up floor placeholder */
  float BingoLbs = 0.0f;   /* 0 = no threshold entered; the warning stays inhibited */
  int StNum = 1;
};

} // namespace FlightBox
#endif
