#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/world");
  NoIncludeClimbsOutOfItsDirectory("src/world");
  NoIncludeClimbsOutOfItsDirectory("src/world/tiles");
  Covers("I.9 world/ streams what is there and names nothing that decides what grows on it");
  return Report();
}
