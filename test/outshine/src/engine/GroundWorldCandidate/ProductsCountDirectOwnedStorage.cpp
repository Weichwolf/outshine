#include "GroundWorldCandidate.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  GroundBuildProducts products;
  const size_t before = products.OwnedHeapBytes();
  products.PositionsM.reserve(17);
  products.Indices.reserve(19);
  products.ClassPalette.reserve(23);
  const size_t added = products.PositionsM.capacity() * sizeof(float) +
                       products.Indices.capacity() * sizeof(uint32_t) +
                       products.ClassPalette.capacity() * sizeof(float);
  CHECK(products.OwnedHeapBytes() == before + added,
        "candidate products count each directly owned mesh and palette allocation");
  return Report();
}
