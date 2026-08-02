/* The shared per-frame state every stage's Encode() reads: FBRenderer fills exactly one per frame,
 * before recording any pass. A stage never reaches back into FBRenderer for camera/sun/mode state. */
#ifndef FBFRAMECONTEXT_H
#define FBFRAMECONTEXT_H

namespace FlightBox::Render {

struct FBFrameContext {
  double Eye[3], Fwd[3], Right[3], CamUp[3];   /* ECEF camera basis, eye at the render origin */
  double Up[3];            /* geographic/radial up at the eye (NOT CamUp — that's rolled); cloud tangent basis */
  float Mvp20[20];        /* camera-relative view-projection (0..15, column-major) + sun dir (16..18) + pad */
  double SunDir[3], MoonDir[3];
  double DayFactor;        /* 0 night .. 1 day (core/FBEphemeris.h FBDaylightFactor), EVS only; SVS pins 1.0 */
  double MoonPhase, Cloud;
  float CloudCover;        /* FBState.Env total cover; drives the sky dome's noise sheet */
  float CloudLow, CloudMid, CloudHigh, CloudBaseAGL;  /* the weather deck mix as the client sampled it */
  float AltM;               /* HudState.alt (m ASL) — the cloud shell radii are absolute, referenced off it */
  bool GroundPhoto;         /* SVS (false, constant-day database view) vs EVS (true, real ephemeris) */
  double SkyClock;          /* sim UTC (unix seconds) driving sidereal placement */
  float DayFade;            /* StarDayFade: (float)DayFactor, the night-only-draw gate stars/lights share */
  float Dt;
  unsigned FrameNo;
  int Width, Height;        /* fixed scene resolution (FrameTex) */
  /* THE 3x3 GRID (doc/render/renderer.md §2.4): the out-the-window viewport is the top two rows and
   * ViewH is its lower edge; the bottom row is the MFD bank. Equal to Height when the cockpit is off
   * (the cloud lab), which is what keeps those frames unchanged. */
  int ViewH;
};

} // namespace FlightBox::Render
#endif
