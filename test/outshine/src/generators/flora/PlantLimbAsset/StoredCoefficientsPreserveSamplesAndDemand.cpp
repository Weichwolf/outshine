#include "Check.h"
#include "PlantLimbAsset.h"
#include "PlantLimbPlan.h"
#include "PlantLimbSurface.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array anchors{
      PlantLimbAnchor{.Node = 8, .PositionM = {{0, 0, 0}}, .RadiusM = 0.2},
      PlantLimbAnchor{.Node = 19, .PositionM = {{0, 1, 0}}, .RadiusM = 0.1},
      PlantLimbAnchor{.Node = 81, .PositionM = {{0.2, 2, 0.3}}, .RadiusM = 0.05}};
  auto original = PlantLimbPlan::Create(anchors);
  CHECK(original, "independent curved source control prepares successfully");
  if (!original) { return Report(); }
  std::vector<PlantLimbPlan> source;
  source.push_back(std::move(*original));
  constexpr size_t kEncoded = 16 + 8 + 2 * 148;
  constexpr size_t kResident =
      sizeof(PlantLimbPlan) + 3 * sizeof(uint32_t) + 2 * 18 * sizeof(double);
  const auto encoded = EncodePlantLimbs(source, kEncoded);
  CHECK(encoded && encoded->size() == kEncoded, "fieldwise format uses its exact declared budget");
  if (!encoded) { return Report(); }
  CHECK(!EncodePlantLimbs(source, kEncoded - 1), "encoded shortfall refuses the complete product");
  const auto restored = DecodePlantLimbs(*encoded, kResident);
  CHECK(restored && restored->size() == 1,
        "exact resident budget restores the prepared coefficients");
  CHECK(!DecodePlantLimbs(*encoded, kResident - 1),
        "resident shortfall fails before limb allocation");
  if (!restored) { return Report(); }
  const auto roundtrip = EncodePlantLimbs(*restored, kEncoded);
  CHECK(roundtrip && *roundtrip == *encoded,
        "cooked fields remain byte-identical without replanning");
  CHECK((*restored)[0].Nodes()[2] == anchors[2].Node, "arbitrary source IDs survive the raw asset");
  constexpr size_t kSamples = 256;
  for (size_t at = 0; at <= kSamples; ++at) {
    const auto before = source[0].Sample(static_cast<double>(at) / kSamples);
    const auto after = (*restored)[0].Sample(static_cast<double>(at) / kSamples);
    CHECK(before && after && before->CentreM == after->CentreM &&
              before->Tangent == after->Tangent && before->Curvature == after->Curvature &&
              before->RadiusM == after->RadiusM && before->RadiusSlopeM == after->RadiusSlopeM,
          "decoding preserves every sampled field exactly rather than refitting the curve");
  }
  const PlantLimbSurfaceDemand demand{
      .Along = 2, .Sides = 8, .MaximumVertices = 65536, .MaximumTriangles = 262144};
  const auto before = BuildPlantLimbSurface(source[0], demand);
  const auto after = BuildPlantLimbSurface((*restored)[0], demand);
  CHECK(before && after && before->Positions == after->Positions &&
            before->Normals == after->Normals && before->Triangles == after->Triangles,
        "detail is generated after loading and produces exactly the same surface");
  return Report();
}
