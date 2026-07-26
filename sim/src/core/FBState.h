/* FlightBox — FBState: the ONE shared per-frame state struct (CLAUDE.md: "Shared per-frame state
 * travels through one plain typed struct — direct typed access, no string-keyed runtime property
 * tree"). It is no longer a flat field list: it is a set of typed OUTPUT BLOCKS (core/
 * FBAvionicsBlocks.h), one per source system, each carrying an FBBlockHeader that says whether its
 * numbers are current, deliberately frozen, or meaningless (core/FBBlockStatus.h).
 *
 * WHY THE BLOCKS. The flat version could not express "this value is not valid right now", and with
 * damage/failure modelling that is the first thing a display and an AI pilot need to know. It also had
 * no compiler-checkable answer to "who wrote this field" — a maintenance audit found ten dead fields
 * and four that were read but never written. Blocks fix both: one writer per block, named in its
 * comment, and a head that a reader must consult before trusting the payload.
 *
 * WHO WRITES WHAT: see each block's comment in FBAvionicsBlocks.h. Sensors WRITE, displays and the
 * pilot READ; no system reads back another's write to derive its own output through this struct except
 * where the block comment says so explicitly (fire control reads nav + platform, warnings read radar
 * altitude/UFC/airframe — both are documented fusion, not accidental coupling).
 *
 * NowS is the bus's own time reference: the module stamps it once per Run() from its own sim clock
 * before cycling any slot, and every block header's timestamp comes from it. One clock for the whole
 * bus is what makes a block's age answerable without every system carrying its own notion of now. */
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
  FBAirframeBlock    Airframe;
  FBWarningBlock     Warnings;
  FBRadarBlock       Radar;
  FBDatalinkBlock    Datalink;
  FBBfmBlock         Bfm;
};

} // namespace FlightBox
#endif /* FB_FBSTATE_H */
