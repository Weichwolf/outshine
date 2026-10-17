#include "PlantRandom.h"
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <limits>
#include <numbers>

int main() {
  uint64_t seed = 0;
  uint64_t axis = 0;
  unsigned paired = 0;
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
  while (std::cin >> seed >> paired >> axis) {
    if (paired > 1) { return 2; }
    auto stream = paired ? outshine::Generators::PlantRandom(seed, axis)
                         : outshine::Generators::PlantRandom(seed);
    for (unsigned i = 0; i < 64; ++i) { std::cout << stream.Raw() << ' '; }
    for (unsigned i = 0; i < 4; ++i) {
      std::cout << stream.Uniform(-std::numbers::pi, std::numbers::pi) << ' ';
    }
    std::cout << '\n';
  }
}
