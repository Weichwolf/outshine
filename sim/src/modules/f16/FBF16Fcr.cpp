#include "FBF16Fcr.h"

namespace FlightBox {

FBF16Fcr::FBF16Fcr() { RebuildVolumes(); }

/* The mode table (see the header on where these numbers come from and what status they have).
 *
 *   mode      azimuth        elevation          range   frame  auto-lock  what it is
 *   off       -              -                  -       -      -          set not radiating
 *   crm       +/-60 deg      +/-10.5 deg        40 nm   4.0 s  no         RWS search, power-up mode
 *   acm_hud   +/-15 deg      +/-10 deg          10 nm   1.0 s  YES        the 30x20 HUD-scan box
 *   acm_bore  +/- 5 deg      +/- 5 deg          10 nm   0.3 s  YES        the ~10 deg boresight cone
 *   acm_vert  +/- 5 deg      -13 .. +47 deg     10 nm   1.2 s  YES        narrow and tall, for the pull
 *   acm_slew  +/-10 deg      +/-10 deg (slewed) 10 nm   0.8 s  YES        the 20x20 slewable box
 *   (locked)  +/-60 deg      +/-60 deg          40 nm   0.1 s  YES        STT: the gimbal envelope
 *
 * The frame times follow the volumes: a mechanically-scanned antenna needs longer for a wider pattern,
 * which is why the boresight cone acquires in a fraction of the HUD box's time and CRM is the slowest of
 * all. That relation, not the absolute seconds, is what the mode proof measures.
 *
 * Vertical scan is centred at +17 deg with a +/-30 deg half-height so it spans -13..+47: it exists to be
 * pulled THROUGH a target during a high-g turn, so it reaches far above the boresight and barely below
 * it — the one sub-mode whose asymmetry is the point, and the reason FBRadarScanVolume carries a centre
 * instead of a bare half-angle. */
void FBF16Fcr::RebuildVolumes() {
  double acm = (RangeOverrideNm_ > 0.0 ? RangeOverrideNm_ : kAcmRangeNm) * kNmToM;
  double search = (RangeOverrideNm_ > 0.0 ? RangeOverrideNm_ : kSearchRangeNm) * kNmToM;

  FBRadarScanVolume off;
  off.Active = false;
  off.FrameS = 1.0;   /* never swept; a nonzero frame keeps the base class's scan grid well-defined */
  Modes_[(int)FBF16FcrMode::Off] = off;

  /* CRM's elevation CENTRE is the antenna-elevation control, not a fixed zero: a mechanically-scanned
   * set's 4-bar pattern covers +/-10.5 deg about wherever the pilot has pointed it, and at BVR range
   * that window is a few thousand feet of altitude — pointing it at the wrong band is the classic way to
   * fly straight past a target the radar was perfectly capable of seeing. The same SlewEl_ the slewable
   * ACM box uses carries it, because it is the same control; azimuth stays nose-centred, since the +/-60
   * deg pattern already spans everything the jet can turn to inside a search cycle. Zero by default, so
   * a mission that never touches it gets exactly the pattern it always got. */
  FBRadarScanVolume crm;
  crm.AzHalfDeg = 60.0; crm.ElCenterDeg = SlewElDeg_; crm.ElHalfDeg = 10.5;
  crm.RangeM = search; crm.FrameS = 4.0;
  Modes_[(int)FBF16FcrMode::Crm] = crm;

  FBRadarScanVolume hud;
  hud.AzHalfDeg = 15.0; hud.ElHalfDeg = 10.0; hud.RangeM = acm; hud.FrameS = 1.0;
  hud.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmHud] = hud;

  FBRadarScanVolume bore;
  bore.AzHalfDeg = 5.0; bore.ElHalfDeg = 5.0; bore.RangeM = acm; bore.FrameS = 0.3;
  bore.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmBore] = bore;

  FBRadarScanVolume vert;
  vert.AzHalfDeg = 5.0; vert.ElCenterDeg = 17.0; vert.ElHalfDeg = 30.0;
  vert.RangeM = acm; vert.FrameS = 1.2; vert.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmVert] = vert;

  FBRadarScanVolume slew;
  slew.AzCenterDeg = SlewAzDeg_; slew.AzHalfDeg = 10.0;
  slew.ElCenterDeg = SlewElDeg_; slew.ElHalfDeg = 10.0;
  slew.RangeM = acm; slew.FrameS = 0.8; slew.AutoAcquire = true;
  Modes_[(int)FBF16FcrMode::AcmSlew] = slew;

  /* STT: all power on one target. The pattern stops being a search at all — it is the antenna's gimbal
   * envelope, revisited every 0.1 s, and it is what lets a lock taken inside a 10 deg boresight cone
   * survive the target manoeuvring far off the nose. AutoAcquire stays true so the base class keeps the
   * lock it holds; losing it drops the pattern straight back to the sub-mode's box. */
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

} // namespace FlightBox
