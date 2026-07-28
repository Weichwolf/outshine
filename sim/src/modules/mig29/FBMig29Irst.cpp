#include "FBMig29Irst.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox::Modules {

FBMig29Irst::FBMig29Irst() { RebuildFields(); }

void FBMig29Irst::RebuildFields() {
  Sensors::FBIrstFieldOfRegard off;
  off.Active = false;
  off.FrameS = 1.0;
  Fields_[(int)FBMig29IrstMode::Off] = off;

  Sensors::FBIrstFieldOfRegard ir;
  ir.AzHalfDeg = kSearchAzHalfDeg; ir.ElHalfDeg = kSearchElHalfDeg;
  ir.RangeM = kRearRangeM; ir.FrameS = kSearchFrameS;
  /* No auto-lock in search, and here the reason is stronger than on the radar: capture is a documented
   * two-handed act ("slew the strobe onto the mark, press and HOLD LOCKON 2-3 s"), and an angle-only
   * sensor that locked itself onto whatever was nearest the centre would be a decision the pilot never
   * made about a target he cannot identify. */
  Fields_[(int)FBMig29IrstMode::Ir] = ir;

  Sensors::FBIrstFieldOfRegard cc;
  cc.AzHalfDeg = kCcAzHalfDeg; cc.ElHalfDeg = kCcElHalfDeg;
  cc.RangeM = kRearRangeM; cc.FrameS = kCcFrameS; cc.AutoAcquire = true;
  Fields_[(int)FBMig29IrstMode::IrCc] = cc;

  Sensors::FBIrstFieldOfRegard bore;
  bore.AzHalfDeg = kBoreHalfDeg; bore.ElHalfDeg = kBoreHalfDeg;
  bore.RangeM = kRearRangeM; bore.FrameS = kBoreFrameS; bore.AutoAcquire = true;
  Fields_[(int)FBMig29IrstMode::Bore] = bore;

  /* Tracking: the head stays on the one source at the search field's full extent, refreshed fast.
   * NOT SingleTarget, and that is the one place where copying the radar would have been wrong: a set in
   * STT spends all its transmitted power on one target and stops refreshing every other track file. A
   * passive head has no power to spend — it keeps seeing its whole field while it follows one mark, and
   * the only thing that is exclusive about the track is where the LASER points. */
  Track_.AzHalfDeg = kSearchAzHalfDeg; Track_.ElHalfDeg = kSearchElHalfDeg;
  Track_.RangeM = kRearRangeM;
  Track_.FrameS = kBoreFrameS;
  Track_.AutoAcquire = true;
}

void FBMig29Irst::SetMode(FBMig29IrstMode m) {
  if (Mode_ == m) return;
  Mode_ = m;
  FBLog::Info("kols", "MODE", {{"mode", FBMig29IrstModeStr(m)}});
}

/* The generic aspect law with THIS head's endpoints — kFrontFraction in the base class is the ratio of
 * the documented pair, so calling up would already give the right curve; it is recomputed from the two
 * named constants instead, so that changing one of them cannot leave the other behind.
 *
 * On top of it the one documented aspect GATE: IR CC is a rear-hemisphere mode, aspect up to 3/4. A
 * source beyond that limit is not detected in this mode at all — not merely detected at shorter range. */
double FBMig29Irst::DetectRangeM(const Sensors::FBIrstFieldOfRegard &f, const Units::FBUnitPose &tgt,
                                 bool afterburner, double bearingDeg) const {
  (void)f;
  double aspectDeg = std::fabs(FBWrap180(bearingDeg - tgt.HeadingDeg));
  if (Mode_ == FBMig29IrstMode::IrCc && aspectDeg > kCcAspectLimitDeg) return 0.0;
  double face = 0.5 * (1.0 + std::cos(aspectDeg * kDeg2Rad));
  double reach = kFrontRangeM + (kRearRangeM - kFrontRangeM) * face;
  if (afterburner) reach *= kAfterburnerRangeFactor;
  return reach;
}

} // namespace FlightBox::Modules
