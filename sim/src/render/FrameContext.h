/* The shared per-frame state every stage's Encode() reads: Renderer fills exactly one per frame,
 * before recording any pass. A stage never reaches back into Renderer for camera/sun/mode state. */
#ifndef FRAMECONTEXT_H
#define FRAMECONTEXT_H

namespace outshine::Render {

struct FrameContext {
  double Eye[3], Fwd[3], Right[3], CamUp[3];   /* ECEF camera basis, eye at the render origin */
  double Up[3];            /* geographic/radial up at the eye (NOT CamUp — that's rolled); cloud tangent basis */
  float Mvp20[20];        /* camera-relative view-projection (0..15, column-major) + sun dir (16..18) + pad */
  /* WHAT A MOTION VECTOR NEEDS, and it is three things and not one. The previous frame's matrix is
   * camera-relative to the PREVIOUS eye, so a point held at this frame's eye has to be carried over
   * by EyeDeltaM first; the jitters are what the resolve subtracts to get the true motion out of two
   * jittered projections. */
  float PrevMvp16[16];
  float EyeDeltaM[3];      /* eyeCur - eyePrev, ECEF metres */
  float JitterNdc[2], PrevJitterNdc[2];
  bool HistoryValid;       /* false on the first frame and after a device reset: no history to blend */
  double SunDir[3], MoonDir[3];
  double DayFactor;        /* 0 night .. 1 day (core/Ephemeris.h DaylightFactor) */
  double MoonPhase;
  float CloudCover;        /* State.Env total cover, as the client sampled it */
  float CloudLow, CloudMid, CloudHigh, CloudBaseAGL;  /* the weather deck mix as the client sampled it */
  float AltM;               /* HudState.alt (m ASL) — the cloud shell radii are absolute, referenced off it */
  double SkyClock;          /* sim UTC (unix seconds) driving sidereal placement */
  float DayFade;            /* StarDayFade: (float)DayFactor, the night-only-draw gate stars/lights share */
  float Dt;
  unsigned FrameNo;
  int Width, Height;        /* fixed scene resolution (FrameTex) */
  /* THE SCENE'S vertical field of view over the FULL frame height, as the scene file declared it.
   * A per-frame fact and not a constant: it is an animation channel, so a stage reads the number the
   * projection was actually built with instead of keeping a second copy. */
  float FovDeg;
};

} // namespace outshine::Render
#endif
