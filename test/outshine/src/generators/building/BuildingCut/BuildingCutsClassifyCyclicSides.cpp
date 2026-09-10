#include "src/generators/building/BuildingCut.h"
#include "Check.h"
#include <algorithm>
#include <array>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;

  struct Case {
    std::array<int, 8> Sides;
    bool Single;
  };

  const std::array cases{Case{{0, 0, 0, 0, 0, 0, 0, 0}, false},
                         Case{{1, 1, 0, 0, 1, 1, 1, 1}, false},
                         Case{{-1, -1, 0, -1, 0, -1, -1, 0}, false},
                         Case{{1, 1, 1, 1, -1, -1, -1, -1}, true},
                         Case{{0, 1, 1, 0, 0, -1, -1, 0}, true},
                         Case{{1, -1, 1, -1, 0, 0, 0, 0}, false},
                         Case{{1, 0, -1, 0, 1, 0, -1, 0}, false},
                         Case{{1, 1, 2, 1, -1, -1, -1, -1}, false}};
  for (const auto &example : cases) {
    auto rotated = example.Sides;
    for (size_t start = 0; start < rotated.size(); ++start) {
      CHECK(HasSingleCut(rotated) == example.Single,
            "cyclic classification distinguishes crossing, tangency and multiple intervals");
      std::rotate(rotated.begin(), rotated.begin() + 1, rotated.end());
    }
  }
  CHECK(!HasSingleCut({}), "empty ring has no cut");
  return Report();
}
