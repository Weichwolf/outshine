#include "FieldPageTable.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

void KeyDomain() {
  constexpr std::array keys{FieldPageAddress{},
                            FieldPageAddress{.X = -1, .Y = -1, .Mip = 0},
                            FieldPageAddress{.X = -65, .Y = -64, .Mip = 5},
                            FieldPageAddress{.X = 2147483647, .Y = -2147483647 - 1, .Mip = 31}};
  constexpr std::array<uint32_t, 4> shaderWords{0, 0xfd0c8742u, 0x43483c6du, 0x0e363ceeu};
  for (size_t i = 0; i < keys.size(); ++i) {
    CHECK(FieldPageHash(keys[i]) == shaderWords[i],
          "native hashing preserves the independently GPU-qualified signed key domain");
  }
  std::vector<std::optional<FieldPageAddress>> slots(6);
  for (size_t i = 0; i < keys.size(); ++i) { slots[i + 1] = keys[i]; }
  auto table = BuildFieldPageTable(slots, 16, 8);
  CHECK(table, "holes and signed spatial mip keys share one immutable lookup table");
  if (!table) { return; }
  for (size_t i = 0; i < keys.size(); ++i) {
    CHECK(FindFieldPageSlot(*table, 8, keys[i]) == i + 1,
          "the complete key resolves its physical slot without treating zero as empty");
  }
  CHECK(!FindFieldPageSlot(*table, 8, {-65, -64, 4}), "mips at one coordinate remain distinct");
  slots[1].reset();
  auto removed = BuildFieldPageTable(slots, 16, 8);
  CHECK(removed && !FindFieldPageSlot(*removed, 8, keys[0]) &&
            FindFieldPageSlot(*table, 8, keys[0]),
        "new lookup snapshots retire a key without mutating earlier snapshots");
  slots[5] = keys[1];
  CHECK(!BuildFieldPageTable(slots, 16, 8), "one virtual page cannot occupy two physical slots");
  slots[5] = FieldPageAddress{.X = 0, .Y = 0, .Mip = 32};
  CHECK(!BuildFieldPageTable(slots, 16, 8),
        "unrepresentable mip shifts are rejected before upload");
  CHECK(!BuildFieldPageTable({}, 0, 1) && !BuildFieldPageTable({}, 7, 1) &&
            !BuildFieldPageTable({}, 8, 0) && !BuildFieldPageTable({}, 8, 9),
        "invalid masks and unbounded probe ranges do not allocate lookup storage");
}

void Collisions() {
  std::vector<std::optional<FieldPageAddress>> slots;
  for (int32_t x = -1000; x < 1000 && slots.size() < 3; ++x) {
    FieldPageAddress key{.X = x, .Y = -17, .Mip = 3};
    if ((FieldPageHash(key) & 7u) == 0) { slots.emplace_back(key); }
  }
  CHECK(slots.size() == 3, "the bounded counterexample contains three colliding complete keys");
  if (slots.size() != 3) { return; }
  const auto &last = slots.back();
  if (!last) { return; }
  auto small = BuildFieldPageTable(slots, 8, 2);
  CHECK(!small && small.error().Kind == outshine::Render::Native::ErrorKind::Unsupported,
        "an unsupported fixed lookup set is distinguished from temporary GPU backpressure");
  auto admitted = BuildFieldPageTable(slots, 8, 3);
  CHECK(admitted && FindFieldPageSlot(*admitted, 3, *last) == 2,
        "an admitted collision chain compares full keys and has a finite search bound");
}
}

int main() {
  KeyDomain();
  Collisions();
  return outshine::Test::Report();
}
