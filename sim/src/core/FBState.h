/* FlightBox — FBState: the live telemetry pose the renderer/HUD read each frame. Holds exactly what an
 * in-process sim (no wire) fills. Atmosphere fields (sun/moon/cloud/vis) — hud.h/atmo.h read them as
 * the SVS/EVS light inputs. batt/rssi stay too (HUD BAT/LNK lines) though nothing feeds them a real
 * value yet.
 *
 * The HUD-telemetry block below is written by the generic systems/ slots + today's one module's own
 * placeholder systems (FBAirDataSystem, FBRadarAltimeter, FBNavSystem, and modules/f16/'s
 * FBF16FireControl/FBF16Ufc/FBF16Sms) — each writes its own slice, none of them read another's write
 * back out of FBState (Peers rufen sich nie gegenseitig); the HUD display (modules/f16/displays'
 * FBF16Hud today) only READS. */
#ifndef FB_FBSTATE_H
#define FB_FBSTATE_H
#include "FBArmState.h"
#include "FBDatalinkTrack.h"
#include "FBMode.h"

namespace FlightBox {

struct FBState {
  float  roll, pitch, yaw;  /* deg */
  float  alt;               /* m ASL (GPS); AGL = alt - terrain, derived by the caller */
  float  x, y;              /* m, ENU offset from the sim origin (home) */
  float  gs, airspeed, vs;  /* m/s: groundspeed, true airspeed, vertical speed (+ = climb) */
  float  home_dist;         /* m */
  float  home_bearing;      /* deg, relative to nose (-180..180) */
  float  glideslope_err;    /* deg; + = above the ideal approach path; |err| >= 90 = NO VALID SOURCE
                             * (sentinel — no approach mode active, HUD must declutter the cue) */
  FBMode state;             /* the REAL, confirmed mode (MIL-STD-1787: no optimistic labels) */
  float  batt;              /* volts. TODO: unfed placeholder until real link telemetry exists */
  int    rssi;              /* 0..100 link quality. TODO: see batt */
  float  cloud;             /* 0..1 total cloud cover (live weather); legacy total for the HUD/haze */
  float  cloud_low, cloud_mid, cloud_high;   /* 0..1 layer cover (open-meteo) -> volumetric type mix */
  float  cloud_base;        /* m AGL, base of the main deck (open-meteo; 0 = unknown -> default) */
  float  vis;               /* horizontal visibility, m (live weather -> haze) */
  float  sun_el, sun_az;    /* sun elevation/azimuth, deg (+ = above horizon, 0=N 90=E) */
  float  moon_el, moon_az;  /* deg */
  float  moon_phase;        /* illuminated fraction 0=new .. 1=full */

  /* ---- FBAirDataSystem: ADC-class air data + the FPM's world-referenced direction ---- */
  float casKts, mach;       /* calibrated airspeed (kt), Mach number */
  float gLoad, gLoadPeak;   /* body normal load factor (g), running peak since boot */
  float trackDeg;           /* ground track, true, deg 0..360 (from the velocity vector) */
  float fpaDeg;             /* flight-path angle, deg (+ = climbing) — FPM's elevation vs. level */

  /* ---- FBRadarAltimeter ---- */
  float radarAltFt;         /* AGL, ft (elev - DEM ground, the same source SetAgl already gets) */

  /* ---- FBNavSystem: one active steerpoint + the bullseye reference, planar-approx geodesy ---- */
  float steerBearingDeg;    /* true bearing aircraft -> steerpoint, deg 0..360 */
  float steerElevAngleDeg;  /* elevation angle aircraft -> steerpoint, deg (+ = above) */
  float steerDistNm;        /* horizontal distance to the steerpoint, nm */
  float steerElevFt;        /* steerpoint ground elevation, ft ASL (FBF16FireControl's slant input) */
  float steerTtgS;          /* time-to-go to the steerpoint at current groundspeed, s */
  float bullBearingDeg;     /* bearing FROM the bullseye TO the aircraft, deg 0..360 */
  float bullDistNm;         /* distance from the bullseye, nm */
  float magVarDeg;          /* magnetic variation, deg (placeholder: 0) */

  /* ---- FBF16FireControl: the "B" (baro/steerpoint-elevation) slant-range method ---- */
  float steerSlantNm;       /* slant range to the steerpoint, nm */
  char  rangeProvider;      /* range-provider letter, 'B' = baro-computed (see FBF16FireControl) */

  /* ---- FBF16Ufc: ICP/DED placeholders ---- */
  int   steerNum;           /* selected steerpoint number */
  float alowFt;             /* ALOW floor, ft */

  /* ---- FBF16Sms: master-arm placeholder ---- */
  FBArmState armState;

  /* ---- FBDatalinkSystem: the cooperative-net picture (MIDS/Link-16, doc/f16/datalink-iff.md) ----
   * The ONLY way anything above the sensors — pilot, HUD, HSD — ever learns that another unit exists.
   * Inline, fixed capacity: rebuilding it allocates nothing (core/FBDatalinkTrack.h). Entries
   * 0..dlTrackCount-1 are valid, in mission-declaration order. */
  bool dlOn;                /* terminal powered (receiving) */
  bool dlXmt;               /* terminal transmitting (XMT ON) — what other units can hear */
  int  dlTrackCount;
  FBDatalinkTrack dlTracks[kMaxDatalinkTracks];
};

} // namespace FlightBox
#endif /* FB_FBSTATE_H */
