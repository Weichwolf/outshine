/* WHAT THE TYPE SYSTEM MUST REFUSE. Both negatives beside this test were live defects before they
 * were compile errors: a height read without the state that gives it meaning, and a Try- answer
 * discarded so the caller reads whatever its own variable held. A rule a reviewer enforces is a rule
 * that comes back; these do not compile. */

#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/core");
  NoIncludeClimbsOutOfItsDirectory("src/core");
  NoIncludeClimbsOutOfItsDirectory("src/core/io");
  Covers("I.2 a value whose misuse must not compile -- and does not");
  return Report();
}
