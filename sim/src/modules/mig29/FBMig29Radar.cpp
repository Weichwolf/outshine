#include "FBMig29Radar.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox::Modules {

FBMig29Radar::FBMig29Radar() { RebuildVolumes(); }

/* Every number in this table carries its source in the header, next to the constant it names. What is
 * NOT arbitrary in it: the frame times follow the volumes exactly as they do on the F-16 — a
 * mechanically scanned antenna needs longer for a wider pattern, and here that relation is anchored at
 * both ends by documented figures (RAD 3.0 s from the 6-second detection latency, CC 2.5 s from T4). */
void FBMig29Radar::RebuildVolumes() {
  double search = RangeOverrideNm_ > 0.0 ? RangeOverrideNm_ * kNmToM : kSearchRangeM;
  double close = (RangeOverrideNm_ > 0.0 ? RangeOverrideNm_ : kCloseRangeNm) * kNmToM;

  /* Not radiating. A nonzero frame keeps the base class's scan grid well-defined while nothing sweeps. */
  Silent_ = Sensors::FBRadarScanVolume{};
  Silent_.Active = false;
  Silent_.FrameS = 1.0;
  Modes_[(int)FBMig29RadarMode::Off] = Silent_;

  double zoneCenter = Zone_ == FBMig29Zone::Left ? -kZoneOffsetDeg
                    : Zone_ == FBMig29Zone::Right ? kZoneOffsetDeg : 0.0;
  Sensors::FBRadarScanVolume rad;
  rad.AzCenterDeg = zoneCenter; rad.AzHalfDeg = kZoneHalfDeg;
  rad.ElCenterDeg = AntennaElDeg_; rad.ElHalfDeg = kRadElHalfDeg;
  rad.RangeM = search; rad.FrameS = kRadFrameS;
  /* No auto-lock, for the same reason CRM has none: a search mode finds everything and locks nothing,
   * because on THIS jet a lock is doubly expensive — it warns the target and it is the moment the whole
   * doctrine has been avoiding (datalink-gci.md §5.2). */
  Modes_[(int)FBMig29RadarMode::Rad] = rad;

  Sensors::FBRadarScanVolume cc;
  cc.AzHalfDeg = kCcAzHalfDeg;
  cc.ElCenterDeg = kCcElCenterDeg; cc.ElHalfDeg = kCcElHalfDeg;
  cc.RangeM = close; cc.FrameS = kCcFrameS; cc.AutoAcquire = true;
  Modes_[(int)FBMig29RadarMode::Cc] = cc;

  Sensors::FBRadarScanVolume vs;
  vs.AzHalfDeg = kVsAzHalfDeg;
  vs.ElCenterDeg = kVsElCenterDeg; vs.ElHalfDeg = kVsElHalfDeg;
  vs.RangeM = close; vs.FrameS = kVsFrameS; vs.AutoAcquire = true;
  Modes_[(int)FBMig29RadarMode::Vs] = vs;

  Sensors::FBRadarScanVolume bore;
  bore.AzHalfDeg = kBoreHalfDeg; bore.ElHalfDeg = kBoreHalfDeg;
  bore.RangeM = close; bore.FrameS = kBoreFrameS; bore.AutoAcquire = true;
  Modes_[(int)FBMig29RadarMode::Bore] = bore;

  /* ACM — the broad forward acquisition box the merge needs (header). Nose-centred, auto-locking. */
  Sensors::FBRadarScanVolume acm;
  acm.AzHalfDeg = kAcmAzHalfDeg;
  acm.ElHalfDeg = kAcmElHalfDeg;
  acm.RangeM = close; acm.FrameS = kAcmFrameS; acm.AutoAcquire = true;
  Modes_[(int)FBMig29RadarMode::Acm] = acm;

  /* РНП: the gimbal envelope revisited every 0.2 s. SingleTarget, so the lock costs every other track
   * file — on a set with a 10-target TWS capacity that is a real price, and the capacity itself is not
   * modelled (see the header's gap list). */
  Stt_.AzCenterDeg = 0.0; Stt_.AzHalfDeg = kGimbalAzHalfDeg;
  Stt_.ElCenterDeg = kGimbalElCenterDeg; Stt_.ElHalfDeg = kGimbalElHalfDeg;
  Stt_.RangeM = search;
  Stt_.FrameS = kSttFrameS;
  Stt_.AutoAcquire = true;
  Stt_.SingleTarget = true;
}

void FBMig29Radar::SetMode(FBMig29RadarMode m) {
  if (Mode_ == m) return;
  Mode_ = m;
  RebuildVolumes();
  FBLog::Info("n019", "MODE", {{"mode", FBMig29RadarModeStr(m)}});
}

/* The emission switch is not a mode: it decides whether ANY pattern reaches the air. OFF also powers
 * the set down, so the base class drops its track files and resets the scan raster; DUMMY keeps them
 * running against a volume that is not Active, which is the same "no picture, not an empty picture"
 * the base class already produces for a standby set. */
void FBMig29Radar::SetEmission(FBMig29Emission e) {
  if (Emit_ == e) return;
  Emit_ = e;
  SetPowered(e != FBMig29Emission::Off);
  /* DUMMY does not sweep either, so coming back to ILLUM from it has to restart the raster just as a
   * power-up does — otherwise the whole time spent silent is caught up in the tick the switch moves,
   * and the set reports a firm track in the same tenth of a second it started radiating. Measured on
   * mig29-intercept before this line existed: contact at t=27.9 instead of the documented 2 x 3.0 s. */
  if (e == FBMig29Emission::Illum) ResyncScan();
  FBLog::Info("n019", "EMISSION", {{"state", e == FBMig29Emission::Illum ? "illum"
                                          : e == FBMig29Emission::Dummy ? "dummy" : "off"}});
}

void FBMig29Radar::SetZone(FBMig29Zone z) {
  if (Zone_ == z) return;
  Zone_ = z;
  RebuildVolumes();
  FBLog::Info("n019", "ZONE", {{"zone", z == FBMig29Zone::Left ? "left"
                                      : z == FBMig29Zone::Right ? "right" : "center"},
      {"azCenterDeg", Modes_[(int)FBMig29RadarMode::Rad].AzCenterDeg}});
}

/* Clamped into the gimbal envelope rather than rejected: the control is a continuous knob on the real
 * panel and it physically cannot drive the dish past its own stops. */
void FBMig29Radar::SetAntennaElevDeg(double deg) {
  double lo = kGimbalElCenterDeg - kGimbalElHalfDeg, hi = kGimbalElCenterDeg + kGimbalElHalfDeg;
  if (deg < lo) deg = lo;
  if (deg > hi) deg = hi;
  if (AntennaElDeg_ == deg) return;
  AntennaElDeg_ = deg;
  RebuildVolumes();
  FBLog::Info("n019", "ANTENNA_ELEV", {{"elDeg", deg}});
}

void FBMig29Radar::SetRangeOverrideNm(double nm) {
  RangeOverrideNm_ = nm > 0.0 ? nm : 0.0;
  RebuildVolumes();
}

/* The three documented bands, plus the one documented exemption. The comparison is on the RANGE at
 * which the look is being taken, so a target crossing 8 nm inbound sees the requirement collapse from
 * 81 kt to 27 kt at exactly that range — the step is the model, not a smoothed curve the source does
 * not describe. */
double FBMig29Radar::DopplerNotchMs(double rangeM) const {
  /* CC and the broad ACM box are the dogfight modes: both drop the closure requirement, because a
   * co-speed merge is exactly the low-radial-velocity case [DCS-EA §3.3, "tracks at equal speeds and at
   * a lag"] — a notch here would reject the very target the mode exists to hold. */
  if (Mode_ == FBMig29RadarMode::Cc || Mode_ == FBMig29RadarMode::Acm) return 0.0;
  double nm = rangeM * kMToNm;
  double kt = nm > kNotchFarNm ? kNotchFarKt : nm >= kNotchNearNm ? kNotchMidKt : kNotchNearKt;
  return kt * kKtToMs;
}

} // namespace FlightBox::Modules
