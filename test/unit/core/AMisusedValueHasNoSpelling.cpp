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
