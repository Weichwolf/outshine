#include "TransportPortFrame.h"
#include "RoadCrossSection.h"
#include "TransportPlanFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

std::optional<TangentFrame> CoreFrame(GeoCellId core) {
  const auto bounds = core.Bounds();
  if (!bounds) { return std::nullopt; }
  return TangentFrame::At({.LongitudeDeg = std::midpoint(bounds->WestDeg, bounds->EastDeg),
                           .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)});
}

Vec3 Ecef(const TangentFrame &frame, const Vec3f &position) {
  return frame.ToEcefPosition({.EastM = position[0], .NorthM = -position[2], .UpM = position[1]});
}

double Distance(const Vec3 &a, const Vec3 &b) {
  return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}

double RoundingBound(const RoadPort &port) {
  const auto &gate = port.Gate;
  const double scale = std::max({1.0,
                                 std::abs(gate.EastM),
                                 std::abs(gate.NorthM),
                                 std::abs(gate.GradeM),
                                 gate.HalfWidthM + gate.ShoulderM});
  return 8 * std::numeric_limits<float>::epsilon() * scale *
             (1 + std::abs(port.Plane.SlopeE) + std::abs(port.Plane.SlopeN)) +
         64 * std::numeric_limits<double>::epsilon() * kEarthMeanRadiusM;
}

void CheckContact(GeoCellId core, TransportPort port) {
  const auto bounds = core.Bounds();
  if (!bounds) { return; }
  port.Anchor = {.LongitudeDeg = bounds->EastDeg,
                 .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)};
  auto next = core;
  next.X = (next.X + 1) % (uint32_t{1} << static_cast<unsigned>(core.Level));
  const auto aCore = CoreFrame(core);
  const auto bCore = CoreFrame(next);
  CHECK(aCore && bCore, "both adjacent cores have geographic bounds");
  if (!aCore || !bCore) { return; }
  const auto &aFrame = *aCore;
  const auto &bFrame = *bCore;
  const auto a = TransportPortInFrame(port, aFrame, {});
  const auto b = TransportPortInFrame(port, bFrame, {});
  CHECK(a && b && a->Gate.OutE < 0 && b->Gate.OutE > 0,
        "the same canonical direction is oriented into each core without changing the source port");
  if (!a || !b) { return; }
  const auto aPositions = RoadPortPositions(*a);
  const auto bPositions = RoadPortPositions(*b);
  const auto sourceFrame = TangentFrame::At(port.Anchor);
  const auto &gate = port.Contact.Gate;
  const std::array across{-gate.HalfWidthM - gate.ShoulderM,
                          -gate.HalfWidthM,
                          gate.HalfWidthM,
                          gate.HalfWidthM + gate.ShoulderM};
  const double aBound = RoundingBound(*a);
  const double bBound = RoundingBound(*b);
  double gap = 0;
  for (size_t at = 0; at < across.size(); ++at) {
    const double east = -gate.OutN * across[at];
    const double north = gate.OutE * across[at];
    const auto reference =
        sourceFrame.ToEcefPosition({.EastM = east,
                                    .NorthM = north,
                                    .UpM = gate.GradeM + port.Contact.Plane.SlopeE * east +
                                           port.Contact.Plane.SlopeN * north});
    const auto aWorld = Ecef(aFrame, aPositions[across.size() - 1 - at]);
    const auto bWorld = Ecef(bFrame, bPositions[at]);
    CHECK(Distance(aWorld, reference) <= aBound && Distance(bWorld, reference) <= bBound,
          "each frame preserves the physical cross section within its Float32 rounding bound");
    gap = std::max(gap, Distance(aWorld, bWorld));
  }
  CHECK(gap <= aBound + bBound,
        "opposite core representations meet within the combined rounding bound");
  Note("canonical port frame gap", gap, "m");
  Note("combined Float32 rounding bound", aBound + bBound, "m");
  const auto bAgain = TransportPortInFrame(port, bFrame, {});
  const auto aAgain = TransportPortInFrame(port, aFrame, {});
  CHECK(aAgain && bAgain && RoadPortPositions(*aAgain) == aPositions &&
            RoadPortPositions(*bAgain) == bPositions,
        "reversing conversion order retains exactly the same per-core vertices");
}

}

int main() {
  const auto plan = TransportPlanFixture();
  CHECK(plan.has_value(), "the canonical shared input contact is available");
  if (!plan) { return Report(); }
  const auto core = plan->Definition.Core;
  CheckContact(core, plan->Ports[0]);
  constexpr uint32_t last = (uint32_t{1} << 16) - 1;
  CheckContact({.Level = 16, .X = last, .Y = core.Y}, plan->Ports[0]);
  CheckContact({.Level = 16, .X = core.X, .Y = 0}, plan->Ports[0]);
  CheckContact({.Level = 16, .X = core.X, .Y = last}, plan->Ports[0]);
  const auto frame = CoreFrame(core);
  CHECK(frame.has_value(), "the reference core has a valid tangent frame");
  if (!frame) { return Report(); }
  auto invalid = plan->Ports[0];
  invalid.Contact.Gate.EastM = 1;
  CHECK(!TransportPortInFrame(invalid, *frame, {}),
        "a canonical port cannot carry an undisclosed local origin offset");
  invalid = plan->Ports[0];
  invalid.Contact.Gate.OutE = 0;
  invalid.Contact.Gate.OutN = 0;
  CHECK(!TransportPortInFrame(invalid, *frame, {}),
        "an unknown source orientation cannot become a qualified boundary contact");
  invalid = plan->Ports[0];
  invalid.Contact.Plane.SlopeE = std::numeric_limits<double>::quiet_NaN();
  CHECK(!TransportPortInFrame(invalid, *frame, {}),
        "nonfinite planes cannot reach the page mesher");
  return Report();
}
