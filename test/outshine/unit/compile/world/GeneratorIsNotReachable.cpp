/* WHAT MUST NOT BE REACHABLE FROM world/. The streamer is under the generators, not beside them: a
 * world file that could name one would let content decide what the world is. This has to FAIL to
 * compile, and for the stated reason. */
#include "TilePool.h"
#include "Forest.h"
// REFUSED: 'Forest.h' file not found

int main() { return 0; }
