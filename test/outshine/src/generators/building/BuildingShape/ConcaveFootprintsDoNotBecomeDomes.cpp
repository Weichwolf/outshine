#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "BuildingSurface.h"
#include "BuildingWallNormals.h"
#include "PreparedStructureCodec.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<double, 32> concave{
      54.78310823608913,  9.436064958572388, 54.78306802035973,  9.435705542564392,
      54.783046365719656, 9.435710906982422, 54.7830339916344,   9.435598254203796,
      54.783006149928745, 9.435608983039856, 54.78296593409784,  9.435233473777771,
      54.78316391934073,  9.435169100761414, 54.783182480407554, 9.435324668884277,
      54.783241257062976, 9.435308575630188, 54.78327219211048,  9.435576796531677,
      54.7833247816369,   9.435555338859558, 54.783343342629905, 9.435721635818481,
      54.78329075312761,  9.43573772907257,  54.78331859463734,  9.43601131439209,
      54.783281472620104, 9.436097145080566, 54.78315463880411,  9.436145424842834};
  BuildingScratch scratch;
  const auto parts = MassOf(concave, {.HeightM = 40, .HeightMeasured = true}, {}, scratch);
  CHECK(parts && !parts->empty(), "supplied concave building retains its massing");
  if (parts && !parts->empty()) {
    CHECK(parts->front().Fill > 0.70 && parts->front().Fill < 0.84,
          "concave regression shares the former round footprint fill range");
    CHECK(std::ranges::none_of(*parts,
                               [](const BuildingShape &part) {
                                 return part.RoundFootprint || part.Roof == RoofKind::Dome;
                               }),
          "concave building parts do not gain elliptical roofs or curved mantles");
    const auto tallest = std::ranges::max_element(*parts, {}, &BuildingShape::TopM);
    CHECK_NEAR(tallest->TopM(),
               40,
               1e-6,
               "height m",
               "simpler roof preserves the supplied height across stacked parts");
  }
  for (const int corners : {8, 16}) {
    std::vector<double> ring;
    for (int corner = 0; corner < corners; ++corner) {
      const double angle = 2 * std::numbers::pi * corner / corners;
      ring.push_back(48.0 + 4.0 * std::sin(angle) / 111320.0);
      ring.push_back(16.0 + 4.0 * std::cos(angle) /
                                (111320.0 * std::cos(48.0 * std::numbers::pi / 180.0)));
    }
    StructurePlan plan;
    plan.RingLatLon = ring;
    plan.HeightM = 80;
    plan.HeightMeasured = true;
    const auto surface = BuildingSurface::Prepare(plan, scratch);
    CHECK(surface && surface->Shapes().size() == 1 &&
              HasCurvedShaftWalls(surface->Shapes().front()),
          "convex round service shafts retain their common shape classification");
    if (!surface) { continue; }
    const std::array<const BuildingSurface *, 1> block{&*surface};
    const auto encoded = EncodeBuildingSurfaceBlock(block);
    CHECK(encoded.has_value(), "enriched round footprint encodes in the native asset");
    if (!encoded) { continue; }
    const auto loaded = DecodeBuildingSurfaceBlock(*encoded, 1024 * 1024);
    CHECK(loaded && loaded->size() == 1 && loaded->front().Shapes().size() == 1 &&
              loaded->front().Shapes().front().RoundFootprint &&
              HasCurvedShaftWalls(loaded->front().Shapes().front()),
          "native hits preserve roundness without repeating footprint classification");
    plan.HeightM = 12;
    const auto roundRoof = BuildingSurface::Prepare(plan, scratch);
    CHECK(roundRoof && !roundRoof->Shapes().empty() &&
              roundRoof->Shapes().front().Roof == RoofKind::Dome,
          "genuine round roofs remain available instead of flattening all roof families");
  }
  return Report();
}
