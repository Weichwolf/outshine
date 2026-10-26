#include "OsmWaySemantics.h"
#include "OsmElementSet.h"
#include "OsmTransportBuilder.h"
#include "TransportTopology.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <string>

int main() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::World;
  using namespace outshine::Test;
  constexpr std::array<WayTagView, 8> tags{{{"highway", "primary"},
                                            {"width", "7.5"},
                                            {"lanes", "2"},
                                            {"oneway", "-1"},
                                            {"bridge", "yes"},
                                            {"layer", "1"},
                                            {"surface", "concrete"},
                                            {"access", "private"}}};
  const auto borrowed = DescribeOsmWay(tags);
  CHECK(borrowed && borrowed->TransportTagged &&
            borrowed->Modes == static_cast<uint8_t>(TransportMode::Motor) &&
            borrowed->Facility == TransportFacility::Arterial &&
            borrowed->Surface == TransportSurface::Concrete && borrowed->WidthM == 7.5 &&
            borrowed->LaneCount == 2 && borrowed->Layer == 1 && borrowed->Bridge &&
            !borrowed->Tunnel && borrowed->Access == TransportAccess::Restricted &&
            borrowed->Travel == OsmWayTravel::Reverse,
        "borrowed attributes preserve the existing native interpretation of every given field");
  Way owned;
  for (const auto &tag : tags) {
    owned.Tags.push_back({.Key = std::string(tag.Key), .Value = std::string(tag.Value)});
  }
  const auto reference = DescribeOsmWay(owned);
  CHECK(reference && borrowed && reference->Modes == borrowed->Modes &&
            reference->Facility == borrowed->Facility && reference->Surface == borrowed->Surface &&
            reference->WidthM == borrowed->WidthM && reference->LaneCount == borrowed->LaneCount &&
            reference->Layer == borrowed->Layer && reference->Bridge == borrowed->Bridge &&
            reference->Tunnel == borrowed->Tunnel && reference->Access == borrowed->Access &&
            reference->Travel == borrowed->Travel,
        "archive tags and allocation-free MVT views share one classifier");
  constexpr std::array<WayTagView, 2> ambiguous{{{"highway", "primary"}, {"highway", "service"}}};
  const auto refused = DescribeOsmWay(ambiguous);
  CHECK(!refused && refused.error() == TransportBuildErrorCode::AmbiguousTag,
        "borrowing cannot weaken duplicate-tag rejection");
  return Report();
}
