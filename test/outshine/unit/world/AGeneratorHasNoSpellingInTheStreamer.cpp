/* THE STREAMER IS UNDER THE GENERATORS, NOT BESIDE THEM. A world file that could name one would let
 * content decide what the world is. */

#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/outshine/unit/compile/world");
  NoIncludeClimbsOutOfItsDirectory("src/world");
  NoIncludeClimbsOutOfItsDirectory("src/world/tiles");
  Covers("I.9 world/ streams what is there and names nothing that decides what grows on it");
  return Report();
}
