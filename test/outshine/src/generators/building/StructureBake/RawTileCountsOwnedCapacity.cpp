#include "StructureBake.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::RawTile raw;
  const size_t before = raw.HeapBytes();
  raw.LatLon.reserve(7);
  raw.Structures.reserve(3);
  raw.Ways.reserve(5);
  CHECK(raw.HeapBytes() == before + raw.LatLon.capacity() * sizeof(double) +
                               raw.Structures.capacity() * sizeof(Generators::RawTile::Structure) +
                               raw.Ways.capacity() * sizeof(Generators::RawTile::Way),
        "raw structure input counts every owned vector capacity once");
  return Report();
}
