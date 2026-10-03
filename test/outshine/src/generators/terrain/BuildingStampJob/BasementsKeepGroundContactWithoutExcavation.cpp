#include "BuildingStampJob.h"
#include "Check.h"
#include <array>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<double, 8> points{0, 0, 0, 0.001, 0.001, 0.001, 0.001, 0};
  std::array<Ground::BuildingFootprint, 3> prints{};
  prints[0].PointCount = 4;
  prints[0].BaseM = 100;
  prints[0].SeatM = 100;
  prints[0].MinimumHeightM = -11;
  prints[0].HeightM = 4;
  prints[1] = prints[0];
  prints[1].HeightM = -2;
  prints[2] = prints[0];
  prints[2].HeightM = 0;
  const auto frame = TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0});
  Generators::BuildingStampJob job(frame, 1);
  bool done = false;
  for (int step = 0; step < 100 && !done; ++step) {
    const auto advanced = job.Advance(
        {.Footprints = prints, .Points = points, .VectorGeneration = 1, .UnitsMost = 1});
    CHECK(advanced.has_value(), "signed footprints keep their source across interruptions");
    if (!advanced) { return Report(); }
    done = *advanced;
  }
  CHECK(done, "signed footprint stamping finishes");
  const auto stamps = std::move(job).Take();
  CHECK(stamps && stamps->size() == 1,
        "only the body crossing the ground creates a surface foundation");
  if (stamps && stamps->size() == 1) {
    CHECK(stamps->front().PlateauM == 100 && stamps->front().YieldM == 0,
          "basement endpoint is not a demand to excavate eleven metres of terrain");
  }
  return Report();
}
