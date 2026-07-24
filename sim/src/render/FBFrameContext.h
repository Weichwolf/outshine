/* FlightBox — FBFrameContext: the shared per-frame state every draw stage's Encode() reads. FBRenderer
 * fills exactly one of these per RenderFrame() call, before recording any pass, and hands it to each
 * stage in turn — a stage never reaches back into FBRenderer for camera/sun/mode state. Grows as more
 * stages are extracted (kein Big-Bang); today it carries what the already-extracted stages need. */
#ifndef FBFRAMECONTEXT_H
#define FBFRAMECONTEXT_H

namespace FlightBox {

struct FBFrameContext {
  double Eye[3], Fwd[3], Right[3], CamUp[3];   /* ECEF camera basis, eye at the render origin */
  float Mvp20[20];        /* camera-relative view-projection (0..15, column-major) + sun dir (16..18) + pad */
  double SunDir[3], MoonDir[3];
  double DayFactor;        /* 0 night .. 1 day (DaylightFactor(sunElDeg)), EVS only; SVS pins 1.0 */
  double MoonPhase, Cloud;
  bool GroundPhoto;         /* SVS (false, constant-day database view) vs EVS (true, real ephemeris) */
  double SkyClock;          /* sim UTC (unix seconds) driving sidereal placement */
  float DayFade;            /* StarDayFade: (float)DayFactor, the night-only-draw gate stars/lights share */
  float Dt;
  unsigned FrameNo;
  int Width, Height;        /* fixed scene resolution (FrameTex) */
};

} // namespace FlightBox
#endif
