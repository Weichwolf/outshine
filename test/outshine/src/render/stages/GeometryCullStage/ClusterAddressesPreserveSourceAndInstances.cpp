#include "Check.h"
#include "GeometryClusterAddresses.h"
#include <array>
#include <cstdint>
#include <cstddef>

int main() {
  using namespace outshine::Render;
  constexpr std::array templates{
      GeometryClusterTemplate{.RelativeFirstIndex = 7, .IndexCount = 3},
      GeometryClusterTemplate{.RelativeFirstIndex = 16, .IndexCount = 6}};
  constexpr std::array roots{
      GeometryRoot{.TemplateCount = 2, .ModelSlot = 11, .IndexBase = 100},
      GeometryRoot{.TemplateFirst = 1,
                   .TemplateCount = 1,
                   .LogicalFirst = 2,
                   .BatchRow = 1,
                   .ModelSlot = 23,
                   .IndexBase = 200},
      GeometryRoot{.TemplateCount = 0, .LogicalFirst = 3, .BatchRow = 2},
      GeometryRoot{
          .TemplateCount = 2, .LogicalFirst = 3, .BatchRow = 3, .ModelSlot = 31, .IndexBase = 100}};
  const auto addresses = BuildClusterAddresses(roots, templates);
  CHECK(addresses.size() == 5, "empty roots add no logical addresses");
  constexpr std::array<std::array<uint32_t, 4>, 5> expected{
      {{107, 11, 0, 0}, {116, 11, 0, 0}, {216, 23, 1, 1}, {107, 31, 3, 3}, {116, 31, 3, 3}}};
  if (addresses.size() == expected.size()) {
    for (size_t at = 0; at < expected.size(); ++at) {
      const auto &one = addresses[at];
      CHECK(
          (std::array{one.SourceFirst, one.ModelSlot, one.RootRow, one.BatchRow}) == expected[at],
          "source offsets, root identity, batch identity and repeated template instances survive");
    }
  }
  CHECK(BuildClusterAddresses({}, templates).empty(), "empty root sets produce no metadata");
  return outshine::Test::Report();
}
