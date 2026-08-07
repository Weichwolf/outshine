/* The OUTPUT blocks: the typed payload of core/State, one block per SOURCE system.
 * THE ONE RULE: every block has exactly ONE writer — named in its comment below — and any number of
 * readers. doc/core.md, Abschnitt 1. */
#ifndef AVIONICSBLOCKS_H
#define AVIONICSBLOCKS_H

#include "BlockStatus.h"
#include "Mode.h"
#include <cstdint>

namespace outshine {

/* ---- Platform: the airframe's own pose/velocity, what everything conformal is drawn against.
 * WRITER: the owner of the FDM state (module from `st`; client re-publishes the live frame pose). */
struct PlatformBlock {
  BlockHeader H;
  float RollDeg = 0.0f, PitchDeg = 0.0f, YawDeg = 0.0f;
  float AltM = 0.0f;                 /* ASL (geodetic) */
  float EastM = 0.0f, NorthM = 0.0f; /* ENU offset from the sim origin (home) */
  float GsMs = 0.0f, TasMs = 0.0f, VsMs = 0.0f;
  float HomeDistM = 0.0f, HomeBearingDeg = 0.0f;   /* bearing relative to the nose, -180..180 */
  Mode Mode = Mode::Manual;      /* the REAL, confirmed guidance mode (MIL-STD-1787) */
};

/* ---- Environment: sky/weather for lighting and haze. WRITER: the client (ephemeris + live weather),
 * never a module system. An unfed weather source is Invalid, not "zero cover". */
struct EnvironmentBlock {
  BlockHeader H;
  float CloudCover = 0.0f;                                     /* 0..1 total (HUD/haze legacy total) */
  float CloudLow = 0.0f, CloudMid = 0.0f, CloudHigh = 0.0f;    /* 0..1 layer cover -> volumetric mix */
  float CloudBaseAglM = 0.0f;                                  /* 0 = unknown -> shader default */
  float SunElDeg = 0.0f, SunAzDeg = 0.0f;                      /* + = above horizon, az 0=N 90=E */
  float MoonElDeg = 0.0f, MoonAzDeg = 0.0f, MoonPhase = 0.0f;  /* phase = illuminated fraction */
};

} // namespace outshine
#endif
