/* A GENERATOR PRODUCES A RESULT, NEVER A LINE AND NEVER A PICTURE. It has no camera, no device, no
 * streamer and no log: core/io is off this include set, so scattered output has no spelling here,
 * and the drawing half lives one directory further out. */

#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/outshine/unit/compile/generators");
  NoIncludeClimbsOutOfItsDirectory("src/generators");
  NoIncludeClimbsOutOfItsDirectory("src/generators/draw");
  Covers("I.7 a generator sees core and its own directory, and nothing that consumes what it makes");
  return Report();
}
