#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/scenario");
  NoIncludeClimbsOutOfItsDirectory("src/scenario");
  Covers("I.4 a scenario declares what is to be shown and can name nothing that shows it");
  return Report();
}
