#include "CloudViewProbe.h"
#include "MediumViewProbe.h"
#include "NativeGpuCheck.h"
#include "Check.h"

#include <algorithm>
#include <vector>

namespace outshine::Test::CloudViewProbe {
namespace {
constexpr double kBrokenCover = 0.45;
constexpr double kFrameStepS = 1.0 / 30.0;
constexpr double kWindAdvanceS = 60.0;
constexpr float kNearSkyM = 50.0f;
constexpr float kFarSkyM = 600000.0f;
constexpr Pixel kBrightBoundary{1.0f, 0.5f, 0.25f, kCloudViewBoundary[3]};
}

std::vector<Case> BoundaryCases(MediumViewProbe::ImageShape shape) {
  auto result = Cases(shape);
  for (auto &value : result) { value.BoundarySky = true; }
  auto sky = result[1];
  sky.Name = "sky-only";
  sky.SkyOnly = true;
  result.push_back(sky);
  auto clear = sky;
  clear.Name = "sky-clear";
  clear.Snapshot = result[0].Snapshot;
  result.push_back(clear);
  auto broken = sky;
  broken.Name = "sky-broken";
  broken.Snapshot.Values.CloudLow = kBrokenCover;
  result.push_back(broken);
  broken.Name = "sky-broken-wind";
  broken.PhaseS += kWindAdvanceS;
  result.push_back(broken);
  auto wind = sky;
  wind.Name = "sky-wind";
  wind.PhaseS += kFrameStepS;
  result.push_back(wind);
  wind.Name = "sky-wind-60s";
  wind.PhaseS = sky.PhaseS + kWindAdvanceS;
  result.push_back(wind);
  auto translated = result[11];
  translated.Name = "sky-translation";
  translated.SkyOnly = true;
  result.push_back(translated);
  auto black = sky;
  black.Name = "sky-black";
  black.Boundary = {0.0f, 0.0f, 0.0f, sky.Boundary[3]};
  result.push_back(black);
  auto bright = sky;
  bright.Name = "sky-bright";
  bright.Boundary = kBrightBoundary;
  result.push_back(bright);
  auto far = sky;
  far.Name = "sky-far";
  far.View.Projection.FarM = kFarSkyM;
  result.push_back(far);
  auto near = sky;
  near.Name = "sky-near";
  near.View.Projection.FarM = kNearSkyM;
  result.push_back(near);
  auto vacuum = result[8];
  vacuum.Name = "space-outward";
  vacuum.SkyOnly = true;
  vacuum.View.Basis = sky.View.Basis;
  result.push_back(vacuum);
  auto miss = vacuum;
  miss.Name = "space-miss";
  miss.View.Basis.Forward = {{0.0, 0.0, -1.0}};
  miss.View.Basis.Up = {{0.0, 1.0, 0.0}};
  miss.View.Projection.FovDeg = 1.0f;
  result.push_back(miss);
  return result;
}

std::vector<Case> CumulusCases(MediumViewProbe::ImageShape shape) {
  auto result = BoundaryCases(shape);
  for (auto &scenario : result) { scenario.Profile = CloudProfile::CumulusV1; }
  const auto found = std::ranges::find(result, "sky-only", &Case::Name);
  CHECK(found != result.end(),
        "the declared full-sky control exists before making its cover variant");
  if (found == result.end()) { Gpu::Failed(); }
  auto closed = *found;
  closed.Name = "sky-overcast";
  closed.Snapshot.Values.CloudLow = 1.0;
  result.push_back(closed);
  return result;
}
}
