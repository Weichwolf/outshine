/* FlightBox — FBState: the live telemetry pose the renderer/HUD read each frame. Descendant of
 * common/protocol.h's telem_packet_t, trimmed to what an in-process sim (no wire) actually fills:
 * true relics of the old UDP/WS format (magic, seq — needed only to detect/space datagrams) are
 * gone. Atmosphere fields (sun/moon/cloud/vis) stay — hud.h/atmo.h read them as the SVS/EVS light
 * inputs. batt/rssi stay too (HUD BAT/LNK lines) though nothing feeds them a real value yet. */
#ifndef FB_FBSTATE_H
#define FB_FBSTATE_H
#include "FBAutopilot.h"   /* FBMode: this autopilot's only two live states */

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
};

} // namespace FlightBox
#endif /* FB_FBSTATE_H */
