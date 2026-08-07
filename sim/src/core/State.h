/* The ONE shared per-frame state: a set of typed OUTPUT BLOCKS (AvionicsBlocks.h), one per source
 * system, each with a validity head. One writer per block, any number of readers.
 * doc/core.md, Abschnitt 1. */
#ifndef _FBSTATE_H
#define _FBSTATE_H
#include "AvionicsBlocks.h"

namespace outshine {

struct State {
  double NowS = 0.0;   /* the bus time reference every block header is stamped against */

  PlatformBlock    Platform;
  EnvironmentBlock Env;
};

} // namespace outshine
#endif /* _FBSTATE_H */
