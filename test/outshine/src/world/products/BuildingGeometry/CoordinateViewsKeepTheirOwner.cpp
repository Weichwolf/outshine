#include "BuildingGeometry.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr size_t kBackingBytes = 1234;
constexpr uint64_t kSourceId = 42;

struct Backing {
  std::vector<double> Points{1, 2, 3, 4};
  std::vector<outshine::GeographicRing> Rings{{.First = 0, .Count = 2}};
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Ground::BuildingGeometry geometry;
  std::weak_ptr<const Backing> weak;
  {
    const auto backing = std::make_shared<const Backing>();
    weak = backing;
    geometry.ShareCoordinates(backing, backing->Points, backing->Rings, kBackingBytes);
    geometry.Sources.push_back({.Id = kSourceId});
    CHECK(geometry.Points().data() == backing->Points.data() &&
              geometry.Rings().data() == backing->Rings.data(),
          "shared coordinates expose the original complete arrays without copying");
  }
  CHECK(!weak.expired() && geometry.Points()[3] == 4 && geometry.Rings()[0].Count == 2,
        "the coordinate owner survives the loader's local lifetime");
  CHECK(geometry.HeapBytes() >= kBackingBytes,
        "retained backing is represented in payload accounting");
  auto copied = geometry;
  geometry = {};
  CHECK(!weak.expired() && copied.Points()[2] == 3 && copied.Sources[0].Id == kSourceId,
        "copies retain the immutable backing and independent source metadata");
  auto moved = std::move(copied);
  copied = {};
  CHECK(copied.Points().empty() && copied.Rings().empty() && moved.Points()[1] == 2,
        "moves transfer the lease and the reset source exposes no stale views");
  moved = {};
  CHECK(weak.expired(), "the final coordinate release retires its backing");
  geometry.OwnCoordinates({5, 6, 7, 8}, {{.First = 1, .Count = 1, .Exterior = false}});
  CHECK(geometry.Points()[0] == 5 && geometry.Rings()[0].First == 1 &&
            !geometry.Rings()[0].Exterior,
        "generated coordinates own moved point and hole arrays");
  return Report();
}
