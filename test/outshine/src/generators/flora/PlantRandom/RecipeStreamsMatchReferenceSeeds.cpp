#include "PlantRandom.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <cstddef>

int main() {
  using outshine::Generators::PlantRandom;
  using namespace outshine::Test;
  constexpr uint64_t kSeed = 17;
  constexpr std::array<uint64_t, 4> kExpected{15588878425886018675ULL,
                                              2969429415550431307ULL,
                                              10288570923761897802ULL,
                                              6789876482546754402ULL};
  PlantRandom stream(kSeed);
  PlantRandom root(kSeed, 0);
  for (const uint64_t word : kExpected) {
    CHECK(stream.Raw() == word && root.Raw() == word,
          "single seed and root-axis seed preserve the reference PCG64 sequence");
  }
  PlantRandom first(kSeed, 1);
  PlantRandom second(kSeed, 2);
  PlantRandom repeated(kSeed, 1);
  bool distinct = false;
  for (size_t at = 0; at < kExpected.size(); ++at) {
    const uint64_t a = first.Raw();
    const uint64_t b = second.Raw();
    distinct = distinct || a != b;
    CHECK(a == repeated.Raw(), "independent axis requests do not perturb each other's stream");
  }
  CHECK(distinct, "sibling axes receive different deterministic streams");
  constexpr uint64_t kLargest = UINT64_MAX;
  PlantRandom edge(kLargest, kLargest);
  for (size_t at = 0; at < kExpected.size(); ++at) {
    const double value = edge.Uniform(0, 1);
    CHECK(value >= 0 && value < 1,
          "full-width seed arithmetic preserves a half-open unit interval");
  }
  return Report();
}
