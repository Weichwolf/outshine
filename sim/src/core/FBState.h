/* The ONE shared per-frame state: a set of typed OUTPUT BLOCKS (FBAvionicsBlocks.h), one per source
 * system, each with a validity head. Sensors WRITE, displays and the pilot READ; the only cross-block
 * reads are the ones a block comment names explicitly (documented fusion, not accidental coupling).
 * doc/core.md, Abschnitt 1. */
#ifndef FB_FBSTATE_H
#define FB_FBSTATE_H
#include "FBAvionicsBlocks.h"

namespace FlightBox {

struct FBState {
  double NowS = 0.0;   /* the bus time reference every block header is stamped against */

  FBPlatformBlock    Platform;
  FBEnvironmentBlock Env;
  FBAirDataBlock     AirData;
  FBRadarAltBlock    RadarAlt;
  FBNavBlock         Nav;
  FBCruiseBlock      Cruise;
  FBFireControlBlock FireControl;
  FBUfcBlock         Ufc;
  FBStoresBlock      Stores;
  FBGunBlock         Gun;
  FBAirframeBlock    Airframe;
  FBWarningBlock     Warnings;
  FBRadarBlock       Radar;
  FBRwrBlock         Rwr;
  FBCmdsBlock        Cmds;
  FBDatalinkBlock    Datalink;
  FBBfmBlock         Bfm;
  /* APPENDED LAST — a new block's telemetry column is declared by its own system (see FBIrstBlock), so
   * appending here shifts nothing that was ever measured. */
  FBIrstBlock        Irst;
  FBVisualBlock      Visual;
};

} // namespace FlightBox
#endif /* FB_FBSTATE_H */
