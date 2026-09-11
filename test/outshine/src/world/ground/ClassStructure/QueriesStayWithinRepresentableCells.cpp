#include "ClassStructure.h"
#include "Check.h"
#include <array>
#include <cfenv>
#include <limits>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto fine = std::make_shared<ClassStructure::Grid>();
  fine->W = fine->H = 1;
  fine->CellM = 10;
  fine->Cells = {7u | (1u << 8), 0};
  auto coarse = std::make_shared<ClassStructure::Grid>();
  coarse->W = coarse->H = 1;
  coarse->OrgE = coarse->OrgN = -50;
  coarse->CellM = 100;
  coarse->Cells = {3u | (1u << 8), 0};
  ClassStructure field(TangentFrame::At({}), fine, coarse, {});
  CHECK(field.Evaluate(0, 0, nullptr, nullptr) == 7, "lower cell edge is included");
  CHECK(field.Evaluate(9, 9, nullptr, nullptr) == 7, "fine interior takes precedence");
  CHECK(field.Evaluate(10, 0, nullptr, nullptr) == 3, "upper fine edge falls back to coarse");
  CHECK(field.Evaluate(-50, -50, nullptr, nullptr) == 3, "coarse lower edge is included");
  CHECK(field.Evaluate(50, 0, nullptr, nullptr) == -1, "coarse upper edge is excluded");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  const double maximum = std::numeric_limits<double>::max();
  for (const double coordinate : {nan, infinity, -infinity, maximum, -maximum, 1e30, -1e30}) {
    for (const bool east : {false, true}) {
      double distance = 0;
      int runner = 99;
      std::feclearexcept(FE_ALL_EXCEPT);
      CHECK(field.Evaluate(east ? coordinate : 0, east ? 0 : coordinate, &distance, &runner) == -1,
            "unrepresentable query has no class");
      CHECK(distance == ClassStructure::kNoEdgeM && runner == -1,
            "no-data outputs are initialized");
      CHECK(std::fetestexcept(FE_INVALID | FE_OVERFLOW) == 0,
            "out-of-range query performs no invalid conversion");
    }
  }
  return Report();
}
