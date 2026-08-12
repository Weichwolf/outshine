/* A DECLARATION SAYS WHAT IS TO BE SHOWN AND KNOWS NOTHING THAT SHOWS IT. That is what lets the
 * whole declaration be tested with no device and no wire, and it is the include set that holds it. */

#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/scenario");
  NoIncludeClimbsOutOfItsDirectory("src/scenario");
  Covers("I.4 a scenario declares what is to be shown and can name nothing that shows it");
  return Report();
}
