#include <scene/LevelOfDetail.h>

#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(LevelOfDetailForRung(-1) == LevelOfDetail::Fine &&
            LevelOfDetailForRung(0) == LevelOfDetail::Fine &&
            LevelOfDetailForRung(1) == LevelOfDetail::Shell &&
            LevelOfDetailForRung(2) == LevelOfDetail::Massed &&
            LevelOfDetailForRung(3) == LevelOfDetail::Skyline &&
            LevelOfDetailForRung(100) == LevelOfDetail::Skyline,
        "representation rungs saturate from full geometry to skyline");
  CHECK(Coarser(LevelOfDetail::Fine, LevelOfDetail::Massed) == LevelOfDetail::Massed &&
            Coarser(LevelOfDetail::Skyline, LevelOfDetail::Shell) == LevelOfDetail::Skyline,
        "coarser representation selection follows the declared order");
  return Report();
}
