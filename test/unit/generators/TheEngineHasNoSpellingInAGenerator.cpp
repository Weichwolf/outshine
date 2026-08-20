#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/generators");
  NoIncludeClimbsOutOfItsDirectory("src/generators");
  NoIncludeClimbsOutOfItsDirectory("src/generators/draw");
  Covers("I.7 a generator sees core and its own directory, and nothing that consumes what it makes");
  return Report();
}
