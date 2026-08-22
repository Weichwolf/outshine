#include "Check.h"
#include "Layering.h"

int main() {
  using namespace outshine::Test;
  EveryCompileSubjectHolds("test/unit/compile/scene");
  NoIncludeClimbsOutOfItsDirectory("src/scene");
  Covers("II.7 a tag is minted only by the catalogue: a value outside it has no spelling, so "
         "two vocabularies cannot arise");
  return Report();
}
