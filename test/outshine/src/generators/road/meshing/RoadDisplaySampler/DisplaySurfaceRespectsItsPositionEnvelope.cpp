#include "RoadDisplaySampler.h"
#include "Carriageway.h"
#include "Check.h"
#include "math/RenderFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Vec3 Vertex(const Ribbon &mesh, size_t station, size_t across) {
  const size_t offset = (station * kRibbonAcross + across) * 3;
  return {{mesh.PositionM[offset], mesh.PositionM[offset + 1], mesh.PositionM[offset + 2]}};
}

void Observe(const ReferenceLine &line, const Section &section, double errorM) {
  const auto plan = Generators::SampleRoadDisplay(
      line, section, errorM, {.StationsMost = 4096, .PoseQueriesMost = 100000});
  CHECK(plan.has_value(), "native sampler produces a bounded Upper plan");
  if (!plan) { return; }
  CHECK(plan->PositionBoundM <= errorM, "the requested error is never increased");
  const auto mesh =
      Sweep(line, section, 0, line.LengthM(), line.LengthM(), RibbonForm::Surface, plan->StationsM);
  CHECK(mesh.Woven && mesh.Stations == plan->StationsM.size(),
        "the native ribbon directly consumes only the planned stations");
  if (!mesh.Woven) { return; }
  const std::array across{-section.HalfWidthM - section.ShoulderM,
                          -section.HalfWidthM,
                          section.HalfWidthM,
                          section.HalfWidthM + section.ShoulderM};
  double maximum = 0;
  for (size_t sample = 0; sample <= 1000; ++sample) {
    const double station = line.LengthM() * static_cast<double>(sample) / 1000;
    const auto next = std::ranges::upper_bound(plan->StationsM, station);
    const size_t interval = std::min(static_cast<size_t>(next - plan->StationsM.begin() - 1),
                                     plan->StationsM.size() - 2);
    const double t = (station - plan->StationsM[interval]) /
                     (plan->StationsM[interval + 1] - plan->StationsM[interval]);
    Placed pose;
    CHECK(line.At(station, pose), "independent dense source sample");
    for (size_t band = 0; band + 1 < kRibbonAcross; ++band) {
      for (const double u : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        const double lateral = std::lerp(across[band], across[band + 1], u);
        const auto source = StandAt(line, {.AlongM = station, .AcrossM = lateral});
        const Vec3 exact{{pose.EastM - std::sin(pose.HeadingRad) * lateral - mesh.OriginM[0],
                          source.HeightM - mesh.OriginM[1],
                          RenderFrame::ZOfNorth(pose.NorthM + std::cos(pose.HeadingRad) * lateral) -
                              mesh.OriginM[2]}};
        const Vec3 a = Vertex(mesh, interval, band);
        const Vec3 b = Vertex(mesh, interval + 1, band);
        const Vec3 c = Vertex(mesh, interval, band + 1);
        const Vec3 d = Vertex(mesh, interval + 1, band + 1);
        const Vec3 actual = t + u <= 1 ? a * (1 - t - u) + b * t + c * u
                                       : c * (1 - t) + b * (1 - u) + d * (t + u - 1);
        const Vec3 delta = actual - exact;
        maximum = std::max(maximum, std::hypot(delta[0], delta[1], delta[2]));
      }
    }
  }
  CHECK(maximum <= plan->PositionBoundM + 1e-10,
        "dense actual triangle interpolation stays inside the model bound plus explicit "
        "observation slack");
}
}

int main() {
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 100}}, error), "straight source");
  const Section section{.HalfWidthM = 2, .ShoulderM = 0.35, .ThicknessM = 0.3};
  const auto straight = Generators::SampleRoadDisplay(
      line, section, 0.001, {.StationsMost = 2, .PoseQueriesMost = 16});
  CHECK(straight && straight->StationsM.size() == 2, "straight source needs exactly two rings");
  Observe(line, section, 0.001);
  CHECK(line.Lay({}, {{.LengthM = 10}}, error) &&
            line.Rise({{.AlongM = 0, .Value = 0, .RatePerM = 0},
                       {.AlongM = 10, .Value = 10, .RatePerM = 2}},
                      error),
        "independent quadratic height h(s)=s*s/10");
  const auto quadratic = Generators::SampleRoadDisplay(
      line, {.HalfWidthM = 2}, 0.25, {.StationsMost = 16, .PoseQueriesMost = 1000});
  const std::vector<double> expectedStations{0, 2.5, 5, 7.5, 10};
  CHECK(quadratic && quadratic->StationsM == expectedStations,
        "analytic chord error h''*length^2/8 gives the same dyadic plan as the Python reference");
  if (quadratic) {
    CHECK_NEAR(quadratic->PositionBoundM,
               0.15625,
               1e-12,
               "meters",
               "the quadratic midpoint chord error is independently exact");
  }
  Observe(line, {.HalfWidthM = 2, .ThicknessM = 0.3}, 0.25);
  CHECK(line.Lay(
            {},
            {{.Shape = Curve::Arc, .LengthM = 100, .EntryCurvature = 0.01, .ExitCurvature = 0.01}},
            error),
        "arc source");
  CHECK(line.Rise({{.AlongM = 0, .Value = 0, .RatePerM = 0},
                   {.AlongM = 50, .Value = 3, .RatePerM = 0},
                   {.AlongM = 100, .Value = 0, .RatePerM = 0}},
                  error),
        "height crest");
  CHECK(line.Bank({{.AlongM = 0, .Value = 0, .RatePerM = 0},
                   {.AlongM = 100, .Value = 0.2, .RatePerM = 0}},
                  error),
        "varying bank");
  for (const double tolerance : {1.0, 0.25, 0.0625}) { Observe(line, section, tolerance); }
  return Report();
}
