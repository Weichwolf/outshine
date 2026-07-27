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
  double DayFactor;        /* 0 night .. 1 day (DaylightFactor(sunElDeg)), EVS only; SVS pins 1.0 */
  double MoonPhase, Cloud;
  float CloudCover;        /* HudState.cloud (raw overall coverage), for FBCloudMarchStage's deck cover */
  float CloudLow, CloudMid, CloudHigh, CloudBaseAGL;  /* weather deck mix (HudState.cloud_*), for FBCloudMarchStage */
  float AltM;               /* HudState.alt (m ASL) — the cloud shell radii are absolute, referenced off it */
  bool GroundPhoto;         /* SVS (false, constant-day database view) vs EVS (true, real ephemeris) */
  double SkyClock;          /* sim UTC (unix seconds) driving sidereal placement */
  float DayFade;            /* StarDayFade: (float)DayFactor, the night-only-draw gate stars/lights share */
  float Dt;
  unsigned FrameNo;
  int Width, Height;        /* fixed scene resolution (FrameTex) */
};

} // namespace FlightBox::Render
#endif
