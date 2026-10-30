#include "RadianceSupport.h"
#include "ImpostorAtlas.h"
#include "Check.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <limits>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

ImpostorAtlas Source(size_t side, int pattern) {
  ImpostorAtlas::View view;
  view.TowardEye = {{0, 0, 1}};
  view.Texels.resize(side * side);
  for (size_t y = 0; y < side; ++y) {
    for (size_t x = 0; x < side; ++x) {
      const bool covered =
          pattern == 0 || pattern == 1 || pattern == 2 || (pattern == 3 && y == x) ||
          (pattern == 4 &&
           ((x == 0 && y == 0) || (x == side - 1 && y == 0) || (x == 0 && y == side - 1))) ||
          (pattern == 5 && y == 0 && x < 2);
      if (!covered) { continue; }
      const double u = (static_cast<double>(x) + .5) / static_cast<double>(side) * 2 - 1;
      const double v = 1 - (static_cast<double>(y) + .5) / static_cast<double>(side) * 2;
      double h = .25 * u - .5 * v + .125;
      if (pattern == 1) { h = -2; }
      if (pattern == 2) { h = .5 * u * u + .25 * v * v - .125; }
      view.Texels[y * side + x] = {.Normal = {{0, 0, 1}},
                                   .Depth = pattern == 1 ? std::numeric_limits<float>::min()
                                                         : static_cast<float>((h + 2) / 4),
                                   .Surface = 1};
    }
  }
  std::string error;
  auto atlas =
      ImpostorAtlas::Create(static_cast<int>(side), {}, 1, {Material{}}, {std::move(view)}, error);
  CHECK(atlas, "bounded synthetic source forms an independently specified field");
  if (!atlas) { std::abort(); }
  return std::move(*atlas);
}
}

int main() {
  using namespace outshine::Test;
  for (const size_t side : {size_t{8}, size_t{512}}) {
    const auto source = Source(side, 0);
    const auto fit = FitRadiancePlane(source, {.View = 0, .TexelsMost = side * side});
    CHECK(fit && fit->SourcePoints == side * side, "every occupied source point participates");
    if (!fit) { continue; }
    CHECK(std::abs(fit->DepthCoefficients[0] - .25) < 1e-11 &&
              std::abs(fit->DepthCoefficients[1] + .5) < 1e-11 &&
              std::abs(fit->DepthCoefficients[2] - .125) < 1e-11 && fit->NormalizedDepthRms < 1e-12,
          "constant-memory QR recovers the independent dyadic plane at both source sizes");
    CHECK(!FitRadiancePlane(source, {.View = 0, .TexelsMost = side * side - 1}) &&
              !FitRadiancePlane(source, {.View = 1, .TexelsMost = side * side}),
          "capacity and unavailable source view refuse before fitting");
  }
  const auto far = FitRadiancePlane(Source(8, 1), {.View = 0, .TexelsMost = 64});
  CHECK(far && far->SourcePoints == 64 && std::abs(far->DepthCoefficients[2] + 2) < 1e-12,
        "the smallest normal positive native depth remains an occupied source point");
  const auto curved = FitRadiancePlane(Source(8, 2), {.View = 0, .TexelsMost = 64});
  CHECK(curved && curved->NormalizedDepthRms > .1 &&
            std::abs(curved->DepthCoefficients[2] - .12109375) < 1e-12,
        "curved source returns measured residual, not an error certificate");
  CHECK(!FitRadiancePlane(Source(8, 3), {.View = 0, .TexelsMost = 64}),
        "collinear source points do not define a plane");
  const auto sparse = FitRadiancePlane(Source(8, 4), {.View = 0, .TexelsMost = 64});
  CHECK(sparse && sparse->SourcePoints == 3 && std::abs(sparse->DepthCoefficients[0] - .25) < 1e-12,
        "three sparse noncollinear source points remain sufficient");
  CHECK(!FitRadiancePlane(Source(8, 5), {.View = 0, .TexelsMost = 64}),
        "two occupied points are insufficient");
  return Report();
}
