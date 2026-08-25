#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/world");
  NoIncludeClimbsOutOfItsDirectory("src/ground");
  NoIncludeClimbsOutOfItsDirectory("src/ground/tiles");
  Covers("I.100 world/ streams what is there and names nothing that decides what grows on it");
  return Report();
}
