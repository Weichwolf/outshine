#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/data");
  NoIncludeClimbsOutOfItsDirectory("src/data");
  Covers("I.41 each upstream is a provider that declares what it covers and knows nothing that consumes it");
  return Report();
}
