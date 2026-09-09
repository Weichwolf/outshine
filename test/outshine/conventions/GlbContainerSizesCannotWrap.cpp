#include "Emit.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine::Gltf;
  using namespace outshine::Test;
  // GLB header 12 bytes, JSON and BIN chunk headers 8 bytes each; payloads align to four.
  constexpr size_t headers = 12 + 8 + 8;
  constexpr size_t maximum = std::numeric_limits<uint32_t>::max();
  constexpr size_t payload = (maximum - headers) & ~size_t{3};
  CHECK(GlbFits({.JsonBytes = 4, .BinaryBytes = payload - 4}),
        "largest aligned container fits its unsigned 32-bit length");
  CHECK(!GlbFits({.JsonBytes = 4, .BinaryBytes = payload - 3}),
        "one extra raw byte needs a complete aligned word beyond the limit");
  CHECK(GlbFits({.JsonBytes = 1, .BinaryBytes = payload - 4}),
        "JSON padding is included in the boundary calculation");
  CHECK(!GlbFits({.JsonBytes = 1, .BinaryBytes = payload - 2}),
        "both chunk paddings count toward the shared limit");
  CHECK(GlbFits({.JsonBytes = payload, .BinaryBytes = 0}),
        "size helper admits the complete payload in one chunk");
  CHECK(!GlbFits({.JsonBytes = maximum, .BinaryBytes = 0}) &&
            !GlbFits({.JsonBytes = 0, .BinaryBytes = maximum}),
        "a chunk cannot consume the header's space");
  for (size_t padding = 0; padding < 4; ++padding) {
    const size_t oversized = std::numeric_limits<size_t>::max() - padding;
    CHECK(!GlbFits({.JsonBytes = oversized, .BinaryBytes = 4}) &&
              !GlbFits({.JsonBytes = 4, .BinaryBytes = oversized}),
          "near-size_t-limit inputs cannot wrap padding into an accepted container");
  }
  return Report();
}
