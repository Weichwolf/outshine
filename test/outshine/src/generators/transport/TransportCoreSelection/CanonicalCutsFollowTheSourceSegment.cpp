#include "TransportCoreFixture.h"
#include "Check.h"
#include "Units.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::World;
using namespace outshine::Test;
constexpr GeoCellId kCell{.Level = 3, .X = 4, .Y = 4};

std::shared_ptr<const TransportNetworkSnapshot> Single(LongitudeLatitude from,
                                                       LongitudeLatitude to) {
  return CoreSource(
      {{.SourceNodeId = 1, .LatitudeDeg = from.LatitudeDeg, .LongitudeDeg = from.LongitudeDeg},
       {.SourceNodeId = 2, .LatitudeDeg = to.LatitudeDeg, .LongitudeDeg = to.LongitudeDeg}},
      {{.Id = {.PathId = kCoreBridgePath}, .FromNodeId = 1, .ToNodeId = 2}});
}

bool SameCut(const TransportCoreEnd &a, const TransportCoreEnd &b) {
  return std::bit_cast<uint64_t>(a.Fraction) == std::bit_cast<uint64_t>(b.Fraction) &&
         std::bit_cast<uint64_t>(a.Geographic.LongitudeDeg) ==
             std::bit_cast<uint64_t>(b.Geographic.LongitudeDeg) &&
         std::bit_cast<uint64_t>(a.Geographic.LatitudeDeg) ==
             std::bit_cast<uint64_t>(b.Geographic.LatitudeDeg) &&
         a.Boundaries == b.Boundaries;
}

void CheckAdjacent() {
  const auto source = BridgeAcrossCore(0.25);
  auto east = kTransportCore;
  ++east.X;
  const auto left = SelectTransportCore(kTransportCore, source, kCoreLimits);
  const auto right = SelectTransportCore(east, source, kCoreLimits);
  const auto rightFirst = SelectTransportCore(east, source, kCoreLimits);
  const auto leftLast = SelectTransportCore(kTransportCore, source, kCoreLimits);
  CHECK(left && right && rightFirst && leftLast && left->Segments.size() == 1 &&
            right->Segments.size() == 1,
        "both geographic cores select the same actual source segment without a camera");
  if (!left || !right || !rightFirst || !leftLast || left->Segments.size() != 1 ||
      right->Segments.size() != 1) {
    return;
  }
  CHECK(SameCut(left->Segments[0].Ends[1], right->Segments[0].Ends[0]) &&
            SameCut(leftLast->Segments[0].Ends[1], rightFirst->Segments[0].Ends[0]) &&
            SameCut(left->Segments[0].Ends[1], leftLast->Segments[0].Ends[1]),
        "shared source fractions, canonical coordinates and boundary IDs are bit-identical in "
        "either core construction order");
  CHECK(left->Segments[0].Ends[0].Fraction > 0 && right->Segments[0].Ends[1].Fraction < 1 &&
            source &&
            source->Topology().FindNode(1)->LongitudeDeg <
                left->Segments[0].Ends[0].Geographic.LongitudeDeg &&
            source->Topology().FindNode(2)->LongitudeDeg >
                right->Segments[0].Ends[1].Geographic.LongitudeDeg,
        "the original outside endpoints remain source-owned instead of replacing a cut with a "
        "new guessed ground condition");
}

void CheckDateline() {
  constexpr double longitude = kDegPerHalfTurn - 1;
  const auto source = Single({.LongitudeDeg = longitude, .LatitudeDeg = 10},
                             {.LongitudeDeg = -longitude, .LatitudeDeg = 10});
  const auto east = SelectTransportCore({.Level = 3, .X = 7, .Y = 4}, source, kCoreLimits);
  const auto west = SelectTransportCore({.Level = 3, .X = 0, .Y = 4}, source, kCoreLimits);
  const auto whole = SelectTransportCore({}, source, kCoreLimits);
  CHECK(east && west && whole && east->Segments.size() == 1 && west->Segments.size() == 1 &&
            whole->Segments.size() == 2,
        "a dateline crossing selects the short original segment and both global chart pieces");
  if (!east || !west || !whole || east->Segments.size() != 1 || west->Segments.size() != 1 ||
      whole->Segments.size() != 2) {
    return;
  }
  const auto &cut = east->Segments[0].Ends[1];
  const auto &boundary = cut.Boundaries[0];
  CHECK(cut.Fraction == 0.5 && cut.Geographic.LongitudeDeg == -180 &&
            SameCut(cut, west->Segments[0].Ends[0]) && boundary && boundary->Line == 0 &&
            whole->Segments[0].Ends[0].Fraction == 0 &&
            whole->Segments[0].Ends[1].Fraction == whole->Segments[1].Ends[0].Fraction &&
            whole->Segments[1].Ends[1].Fraction == 1,
        "dateline aliases share the same canonical cut and preserve ordered source coverage");
  const auto reverse = Single({.LongitudeDeg = -longitude, .LatitudeDeg = 10},
                              {.LongitudeDeg = longitude, .LatitudeDeg = 10});
  const auto reversed = SelectTransportCore({}, reverse, kCoreLimits);
  CHECK(reversed && reversed->Segments.size() == 2 && reversed->Segments[0].Ends[0].Fraction == 0 &&
            reversed->Segments[0].Ends[1].Fraction == 0.5 &&
            reversed->Segments[1].Ends[0].Fraction == 0.5 &&
            reversed->Segments[1].Ends[1].Fraction == 1,
        "westward wrapped pieces retain source order rather than longitude-window order");
}

void CheckCornersAndOwnership() {
  constexpr double startLongitude = -15;
  constexpr double endLongitude = 60;
  constexpr double northPole = kDegPerHalfTurn * 0.5;
  const auto diagonal =
      Single({.LongitudeDeg = startLongitude, .LatitudeDeg = startLongitude * 0.5},
             {.LongitudeDeg = endLongitude, .LatitudeDeg = endLongitude * 0.5});
  const auto corner = SelectTransportCore(kCell, diagonal, kCoreLimits);
  CHECK(corner && corner->Segments.size() == 1,
        "a source diagonal crosses both opposite geographic corners");
  if (corner && corner->Segments.size() == 1) {
    const auto &ends = corner->Segments[0].Ends;
    constexpr double eastDeg = 45;
    constexpr double northDeg = 22.5;
    CHECK(ends[0].Boundaries[0] && ends[0].Boundaries[1] && ends[1].Boundaries[0] &&
              ends[1].Boundaries[1] && ends[0].Geographic.LongitudeDeg == 0 &&
              ends[0].Geographic.LatitudeDeg == 0 && ends[1].Geographic.LongitudeDeg == eastDeg &&
              ends[1].Geographic.LatitudeDeg == northDeg,
          "corner crossings retain both exact boundary constraints instead of choosing an "
          "arbitrary single side");
  }
  constexpr double longitude = 45;
  const auto parallel = Single({.LongitudeDeg = longitude, .LatitudeDeg = 10},
                               {.LongitudeDeg = longitude, .LatitudeDeg = 15});
  auto adjacent = kCell;
  ++adjacent.X;
  const auto left = SelectTransportCore(kCell, parallel, kCoreLimits);
  const auto right = SelectTransportCore(adjacent, parallel, kCoreLimits);
  CHECK(left && right && left->Segments.empty() && right->Segments.size() == 1,
        "a road lying on an east boundary belongs to the half-open neighboring core once");
  const auto pole = Single({.LongitudeDeg = 1, .LatitudeDeg = northPole},
                           {.LongitudeDeg = 2, .LatitudeDeg = northPole});
  const auto north = SelectTransportCore({.Level = 3, .X = 4, .Y = 7}, pole, kCoreLimits);
  CHECK(north && north->Segments.size() == 1 && north->Segments[0].Ends[0].Boundaries[0] &&
            north->Segments[0].Ends[0].Geographic.LatitudeDeg == 90,
        "the final north boundary is retained instead of leaving the geographic domain open");
}

void CheckSourceIdentityAndUnknowns() {
  constexpr double antipodalLongitude = kDegPerHalfTurn * 0.5;
  const auto coincident =
      Single({.LongitudeDeg = 1, .LatitudeDeg = 1}, {.LongitudeDeg = 1, .LatitudeDeg = 1});
  const auto retained = SelectTransportCore(kCell, coincident, kCoreLimits);
  CHECK(retained && retained->Segments.size() == 1 && coincident &&
            retained->Source->Topology().FindNode(1) != retained->Source->Topology().FindNode(2),
        "distinct coincident source nodes remain separate functional inputs for the planner");
  const auto ambiguous = Single({.LongitudeDeg = -antipodalLongitude, .LatitudeDeg = 10},
                                {.LongitudeDeg = antipodalLongitude, .LatitudeDeg = 10});
  const auto refused = SelectTransportCore(kCell, ambiguous, kCoreLimits);
  CHECK(!refused && refused.error() == TransportCoreError::AmbiguousSegment,
        "an exactly antipodal source segment does not acquire an invented geographic direction");
  const auto invalid =
      Single({.LongitudeDeg = 1, .LatitudeDeg = std::numeric_limits<double>::quiet_NaN()},
             {.LongitudeDeg = 2, .LatitudeDeg = 1});
  const auto badSource = SelectTransportCore(kCell, invalid, kCoreLimits);
  CHECK(!badSource && badSource.error() == TransportCoreError::InvalidSource,
        "invalid native coordinates fail before publishing a geographic selection");
}
}

int main() {
  CheckAdjacent();
  CheckDateline();
  CheckCornersAndOwnership();
  CheckSourceIdentityAndUnknowns();
  return Report();
}
