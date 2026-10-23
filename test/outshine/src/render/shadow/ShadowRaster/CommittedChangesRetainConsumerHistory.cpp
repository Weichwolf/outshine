#include "CasterChanges.h"
#include "Check.h"
#include <limits>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

Boxf Region(float low, float high) {
  return {.Min = {{low, low, low}}, .Max = {{high, high, high}}};
}

void History() {
  CasterChanges changes;
  CHECK(changes.Since(0).Complete && changes.Since(0).Bounds.Empty(),
        "a new owner has no committed mutations");
  changes.Include(std::nullopt, Region(0, 1));
  const auto birth = changes.Since(0);
  changes.Include(Region(100, 101), std::nullopt);
  changes.Acknowledge(birth.Revision);
  const auto both = changes.Since(0);
  CHECK(both.Complete && both.Bounds.Holds({{0, 0, 0}}) && both.Bounds.Holds({{101, 101, 101}}),
        "a late acknowledgement cannot discard a newer removal");
  CHECK(changes.Since(birth.Revision).Complete &&
            changes.Since(birth.Revision).Bounds.Holds({{101, 101, 101}}),
        "independent readers receive every change since their own baseline");
  changes.Acknowledge(both.Revision);
  CHECK(!changes.Since(0).Complete, "a reader behind retired history requires a full rebuild");
  CHECK(changes.Since(both.Revision).Complete && changes.Since(both.Revision).Bounds.Empty(),
        "the accepted baseline starts an empty new interval");
  CHECK(!changes.Since(both.Revision + 1).Complete, "a future baseline cannot match this owner");
  changes.Include(Region(-2, -1), Region(2, 3));
  const auto moved = changes.Since(both.Revision);
  CHECK(moved.Complete && moved.Bounds.Holds({{-2, -2, -2}}) && moved.Bounds.Holds({{3, 3, 3}}),
        "movement dirties both departed and arriving geometry");
  changes.Unknown();
  CHECK(!changes.Since(both.Revision).Complete, "unqualified content forces full invalidation");
  const auto full = changes.Since(both.Revision);
  changes.Acknowledge(full.Revision);
  changes.Include(std::nullopt, std::nullopt);
  const auto empty = changes.Since(full.Revision);
  CHECK(empty.Complete && empty.Bounds.Empty(), "known empty geometry invents no dirty volume");
  auto invalid = Region(0, 1);
  invalid.Min[1] = std::numeric_limits<float>::quiet_NaN();
  changes.Include(invalid, std::nullopt);
  CHECK(!changes.Since(empty.Revision).Complete, "nonfinite geometry remains unqualified");
  CasterChanges reversed;
  invalid = Region(0, 1);
  invalid.Min[2] = 2;
  reversed.Include(std::nullopt, invalid);
  CHECK(!reversed.Since(0).Complete, "reversed bounds are not known empty geometry");
}

void Spheres() {
  const auto bounds = CasterChanges::SphereBounds({1, -2, 3, 4});
  CHECK(bounds && bounds->Holds({{-3, -6, -1}}) && bounds->Holds({{5, 2, 7}}),
        "sphere conversion encloses all extrema with outward rounding");
  CHECK(!CasterChanges::SphereBounds({0, 0, 0, -1}), "negative radii are unknown");
  CHECK(!CasterChanges::SphereBounds({std::numeric_limits<float>::infinity(), 0, 0, 1}),
        "nonfinite centres are unknown");
  CHECK(!CasterChanges::SphereBounds(
            {std::numeric_limits<float>::max(), 0, 0, std::numeric_limits<float>::max()}),
        "overflowing extrema are unknown");
}
}

int main() {
  History();
  Spheres();
  return Report();
}
