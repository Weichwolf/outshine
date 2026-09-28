#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "Check.h"
#include "SceneRenderer.h"
#include "SubjectDraw.h"
#include "SubjectResidency.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  static_assert(
      std::is_same_v<decltype(std::declval<const SubjectDraw &>().HeldBytes()), uint64_t>);
  static_assert(
      std::is_same_v<decltype(std::declval<const SceneRenderer &>().PieceBytesHeld()), uint64_t>);
  static_assert(std::is_same_v<decltype(SceneRenderer::Effort::DeviceBytes), uint64_t>);
  SubjectResidency held;
  CHECK(held.HeldBytes() == 0, "empty residency owns no buffer capacity");
  *held.HeldAt(SubjectResidency::Stream::Vertex) = uint32_t{1} << 31;
  *held.HeldAt(SubjectResidency::Stream::Index) = uint32_t{1} << 31;
  CHECK(held.HeldBytes() == uint64_t{4294967296}, "two 2-GiB capacities total 4 GiB");
  for (size_t i = 0; i < SubjectResidency::kStreams; ++i) {
    *held.HeldAt(static_cast<SubjectResidency::Stream>(i)) = std::numeric_limits<uint32_t>::max();
  }
  const uint64_t expected = uint64_t{4294967295} * SubjectResidency::kStreams;
  CHECK(held.HeldBytes() == expected, "every maximum SDL buffer capacity contributes");
  *held.HeldAt(SubjectResidency::Stream::Vertex) = 0;
  CHECK(held.HeldBytes() == expected - uint64_t{4294967295},
        "releasing one capacity preserves the remaining wide total");
  return Report();
}
