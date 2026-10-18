#include "PlanHierarchy.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<Box> bounds;
  for (size_t at = 0; at < 129; ++at) {
    const double x = static_cast<double>(128 - at) * 8.0;
    bounds.push_back({.Min = {{x, -1, -1}}, .Max = {{x + 2, 1, 1}}});
  }
  const auto original = PlanHierarchy::Build(bounds);
  CHECK(original, "an odd source count produces a complete stored hierarchy");
  if (!original) { return Report(); }
  const auto nodesOf = [&] {
    return std::vector(original->Nodes().begin(), original->Nodes().end());
  };
  const auto orderOf = [&] {
    return std::vector(original->SourceOrder().begin(), original->SourceOrder().end());
  };
  auto nodes = nodesOf();
  auto order = orderOf();
  const auto *nodeStorage = nodes.data();
  const auto *orderStorage = order.data();
  const auto restored = PlanHierarchy::Restore(std::move(nodes), std::move(order), bounds);
  CHECK(restored, "valid stored nodes restore without rebuilding a partition");
  if (!restored) { return Report(); }
  CHECK(restored->Nodes().data() == nodeStorage && restored->SourceOrder().data() == orderStorage,
        "restore takes ownership of the existing node and source arrays without copies");
  for (const Vec3 origin : std::array{Vec3{{-1, 0, 0}}, Vec3{{1040, 0, 0}}, Vec3{{0, 2, 0}}}) {
    for (double direction : {-1.0, 1.0}) {
      const Vec3 ray{{direction, 0, 0}};
      const auto intersect = [&](uint32_t source, double maximum) -> std::optional<double> {
        const double x = direction > 0 ? bounds[source].Min[0] : bounds[source].Max[0];
        const double along = (x - origin[0]) / direction;
        return along >= 0 && along <= maximum ? std::optional(along) : std::nullopt;
      };
      const auto before = original->TraceClosest(origin, ray, 0, 2000, intersect);
      const auto after = restored->TraceClosest(origin, ray, 0, 2000, intersect);
      CHECK(before.has_value() == after.has_value() &&
                (!before || (before->Source == after->Source && before->Along == after->Along)),
            "restored queries preserve near hits and outside misses in both directions");
    }
  }
  const auto reject = [&](auto corrupt) {
    auto damagedNodes = nodesOf();
    auto damagedOrder = orderOf();
    corrupt(damagedNodes, damagedOrder);
    CHECK(!PlanHierarchy::Restore(std::move(damagedNodes), std::move(damagedOrder), bounds),
          "corrupt cached hierarchy cannot hide sources or escape traversal bounds");
  };
  reject([](auto &n, auto &) { n.pop_back(); });
  reject([](auto &, auto &o) { o.pop_back(); });
  reject([](auto &, auto &o) { o[0] = o[1]; });
  reject([](auto &, auto &o) { o[0] = std::numeric_limits<uint32_t>::max(); });
  reject([](auto &n, auto &) { n[0].Count = 0; });
  reject([](auto &n, auto &) { n[0].First = 1; });
  reject([](auto &n, auto &) { n[0].Left = 0; });
  reject([](auto &n, auto &) { n[0].Right = std::numeric_limits<uint32_t>::max(); });
  reject([](auto &n, auto &) { n[1].First = 1; });
  reject([](auto &n, auto &) { n[1].Count += 1; });
  reject([](auto &n, auto &) { n.back().Left = 1; });
  reject([](auto &n, auto &) { n.back().First = std::numeric_limits<uint32_t>::max(); });
  reject([](auto &n, auto &) { n.back().Bounds.Max[0] = n.back().Bounds.Min[0]; });
  reject([](auto &n, auto &) { n[0].Bounds.Max[0] = 2; });
  reject([](auto &n, auto &) { n[0].Bounds.Min[1] = std::numeric_limits<double>::quiet_NaN(); });
  reject([](auto &n, auto &) { n[0].Bounds.Max[1] = std::numeric_limits<double>::infinity(); });
  auto invalidBounds = bounds;
  invalidBounds[0].Max[0] = invalidBounds[0].Min[0] - 1;
  CHECK(!PlanHierarchy::Restore(nodesOf(), orderOf(), invalidBounds),
        "source geometry must itself have valid finite bounds");
  const auto empty = PlanHierarchy::Restore({}, {}, {});
  CHECK(empty && empty->Nodes().empty() && empty->SourceOrder().empty(),
        "an empty native asset restores without sentinel nodes");
  const std::array tiedBounds{bounds[0], bounds[0], bounds[0]};
  const auto tied = PlanHierarchy::Build(tiedBounds);
  CHECK(tied, "overlapping sources have a valid balanced partition");
  if (tied) {
    auto tiedNodes = std::vector(tied->Nodes().begin(), tied->Nodes().end());
    const auto permuted = PlanHierarchy::Restore(std::move(tiedNodes), {2, 0, 1}, tiedBounds);
    CHECK(permuted && permuted->SourceOrder()[0] == 2 && permuted->SourceOrder()[1] == 0 &&
              permuted->SourceOrder()[2] == 1,
          "cache restore preserves a valid source permutation rather than repartitioning ties");
  }
  return Report();
}
