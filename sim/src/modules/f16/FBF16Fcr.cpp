#include "FBF16Fcr.h"

namespace FlightBox::Modules {

FBF16Fcr::FBF16Fcr() { RebuildVolumes(); }

/* The mode table [SET], doc/modules-f16.md §4.2. What is NOT arbitrary in it: the frame times
 * follow the volumes, because a mechanically-scanned antenna needs longer for a wider pattern — that
 * RELATION, not the absolute seconds, is what the mode proof measures. */
void FBF16Fcr::RebuildVolumes() {
  double acm = (RangeOverrideNm_ > 0.0 ? RangeOverrideNm_ : kAcmRangeNm) * kNmToM;
  double search = (RangeOverrideNm_ > 0.0 ? RangeOverrideNm_ : kSearchRangeNm) * kNmToM;

  Sensors::FBRadarScanVolume off;
  off.Active = false;
  off.FrameS = 1.0;   /* never swept; a nonzero frame keeps the base class's scan grid well-defined */
  Modes_[(int)FBF16FcrMode::Off] = off;

  /* CRM's elevation CENTRE is the antenna-elevation control, not a fixed zero: at BVR range the 4-bar
   * window is a few thousand feet of altitude band, and pointing it wrong is the classic way to fly
   * past a target the radar could easily have seen. Azimuth stays nose-centred — ±60° already spans
   * everything the jet can turn to inside one search cycle. */
  Sensors::FBRadarScanVolume crm;
  crm.AzHalfDeg = 60.0; crm.ElCenterDeg = SlewElDeg_; crm.ElHalfDeg = 10.5;
  crm.RangeM = search; crm.FrameS = 4.0;
  Modes_[(int)FBF16FcrMode::Crm] = crm;

  Sensors::FBRadarScanVolume hud;
  hud.AzHalfDeg = 15.0; hud.ElHalfDeg = 10.0; hud.RangeM = acm; hud.FrameS = 1.0;
  hud.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmHud] = hud;

  Sensors::FBRadarScanVolume bore;
  bore.AzHalfDeg = 5.0; bore.ElHalfDeg = 5.0; bore.RangeM = acm; bore.FrameS = 0.3;
  bore.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmBore] = bore;

  /* Deliberately ASYMMETRIC (-13..+47): it exists to be pulled THROUGH a target in a high-g turn, so it
   * reaches far above the boresight and barely below — the reason a volume carries a CENTRE at all. */
  Sensors::FBRadarScanVolume vert;
  vert.AzHalfDeg = 5.0; vert.ElCenterDeg = 17.0; vert.ElHalfDeg = 30.0;
  vert.RangeM = acm; vert.FrameS = 1.2; vert.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmVert] = vert;

  Sensors::FBRadarScanVolume slew;
  slew.AzCenterDeg = SlewAzDeg_; slew.AzHalfDeg = 10.0;
  slew.ElCenterDeg = SlewElDeg_; slew.ElHalfDeg = 10.0;
  slew.RangeM = acm; slew.FrameS = 0.8; slew.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmSlew] = slew;

  /* STT stops being a search at all: the gimbal envelope revisited every 0.1 s, which is what lets a
   * lock taken inside a 10° cone survive the target manoeuvring far off the nose. AutoAcquire stays
   * true so the base class keeps the lock; losing it drops straight back to the sub-mode's box. */
  Stt_.AzHalfDeg = kGimbalAzDeg;
  Stt_.ElHalfDeg = kGimbalElDeg;
  Stt_.RangeM = search;
  Stt_.FrameS = 0.1;
  Stt_.AutoAcquire = true;
  Stt_.SingleTarget = true;   /* all power on one target: entering STT costs you every other trackfile */
}

void FBF16Fcr::SetMode(FBF16FcrMode m) { Mode_ = m; }

void FBF16Fcr::SetSlewAz(double azDeg) { SlewAzDeg_ = azDeg; RebuildVolumes(); }
void FBF16Fcr::SetSlewEl(double elDeg) { SlewElDeg_ = elDeg; RebuildVolumes(); }

void FBF16Fcr::SetRangeOverrideNm(double nm) {
  RangeOverrideNm_ = nm;
  RebuildVolumes();
}

} // namespace FlightBox::Modules
