/* WHAT MUST NOT BE REACHABLE FROM world/. The streamer is under the generators, not beside them: a
 * world file that could name one would let content decide what the world is. This has to FAIL to
 * compile, and for the stated reason. */
#include "Forest.h"

int main() { return 0; }
