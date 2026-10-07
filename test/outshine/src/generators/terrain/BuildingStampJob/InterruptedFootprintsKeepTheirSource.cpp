#include "BuildingStampJob.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<double, 8> points{0, 0, 0, 0.001, 0.001, 0.001, 0.001, 0};
  std::array<::outshine::Ground::BuildingFootprint, 4> prints{};
  prints[0].PointCount = 4;
  prints[0].SeatM = 12;
  prints[0].BaseM = 10;
  prints[1].FirstPoint = 4;
  prints[1].PointCount = 4;
  prints[2].PointCount = 4;
  prints[2].SeatM = 18;
  prints[2].BaseM = 12;
  prints[3] = prints[0];
  prints[3].MinimumHeightM = 5;
  prints[3].SeatM = 50;
  const TangentFrame frame = TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0});
  std::optional<std::vector<EarthworkStamp>> oracle;
  for (const size_t budget : {size_t{1}, size_t{7}, size_t{1024}}) {
    Generators::BuildingStampJob job(frame, 42);
    CHECK(!job.Advance({.Footprints = prints,
                        .Points = points,
                        .VectorGeneration = 43,
                        .UnitsMost = budget})
               .has_value(),
          "changed vector generation refuses an incomplete stamp job");
    CHECK(!std::move(job).Take().has_value(), "partial building stamps cannot publish");
    if (budget == 1) {
      const auto started = job.Advance(
          {.Footprints = prints, .Points = points, .VectorGeneration = 42, .UnitsMost = budget});
      CHECK(started.has_value() && !*started, "one work unit leaves the pad incomplete");
      CHECK(!job.Advance({.Footprints = std::span(prints).first(2),
                          .Points = points,
                          .VectorGeneration = 42,
                          .UnitsMost = budget})
                 .has_value(),
            "source dimensions cannot change within a candidate");
    }
    bool done = false;
    for (size_t step = 0; step < 1000 && !done; ++step) {
      const auto advanced = job.Advance(
          {.Footprints = prints, .Points = points, .VectorGeneration = 42, .UnitsMost = budget});
      CHECK(advanced.has_value(), "pinned building stamp source advances");
      if (!advanced) { break; }
      done = *advanced;
    }
    CHECK(done, "building stamps finish under every work budget");
    if (!done) { continue; }
    const size_t retainedBytes = job.HeapBytes();
    auto stamps = std::move(job).Take();
    CHECK(stamps.has_value() && stamps->size() == 2,
          "invalid and raised polygons create no stamp; both grounded pads publish");
    if (!stamps || stamps->size() != 2) { continue; }
    size_t actualBytes = stamps->capacity() * sizeof(EarthworkStamp);
    for (const auto &stamp : *stamps) { actualBytes += stamp.HeapBytes(); }
    CHECK(retainedBytes == actualBytes, "incremental heap accounting matches the owned stamps");
    CHECK(job.HeapBytes() == 0, "transferred stamps retain no diagnostic ownership");
    const EastNorthUp first =
        frame.ToLocalPosition({.LongitudeDeg = points[1], .LatitudeDeg = points[0], .HeightM = 12});
    CHECK((*stamps)[0].RingEastNorthM[0] == first.EastM &&
              (*stamps)[0].RingEastNorthM[1] == first.NorthM &&
              (*stamps)[0].PlateauM == first.UpM && (*stamps)[0].ApronM == 6.0 &&
              (*stamps)[0].YieldM == 2.0 &&
              (*stamps)[0].SeamEastNorthM == (*stamps)[0].RingEastNorthM,
          "pad placement, apron and seam follow the declared frame");
    CHECK((*stamps)[1].PlateauM > (*stamps)[0].PlateauM && (*stamps)[1].YieldM == 6.0,
          "later pad retains its own elevation and cut depth");
    if (oracle) {
      CHECK(*stamps == *oracle, "interruption schedule preserves every pad value and order");
    } else {
      oracle = std::move(*stamps);
    }
  }
  return Report();
}
