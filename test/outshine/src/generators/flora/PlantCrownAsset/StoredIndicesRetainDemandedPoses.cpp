#include "CrownAssetFixture.h"
#include "PlantCrownAsset.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto source = CrownAssetFixture();
  CHECK(source, "cold producer creates a canonical crown plan");
  if (!source) { return Report(); }
  const auto encoded = EncodePlantCrownPlan(*source, kCrownAssetBudget);
  CHECK(encoded,
        "native plan encodes original skeleton, recipe, provenance and finished host index");
  if (!encoded) { return Report(); }
  const auto decoded = DecodePlantCrownPlan(*encoded, kCrownAssetBudget);
  CHECK(decoded, "stored plan restores its arrays without creating a new host index");
  if (!decoded) { return Report(); }
  CHECK(decoded->Recipe().Seed == source->Recipe().Seed &&
            decoded->Recipe().LengthM == source->Recipe().LengthM &&
            decoded->Recipe().BendRad == source->Recipe().BendRad &&
            decoded->Recipe().PetioleM == source->Recipe().PetioleM &&
            decoded->Recipe().FoliageDepthM == source->Recipe().FoliageDepthM &&
            decoded->Recipe().LeafAngleRad == source->Recipe().LeafAngleRad &&
            decoded->Recipe().NormalAlignment == source->Recipe().NormalAlignment,
        "consumers obtain the exact stored physical recipe before bounding or placing details");
  CHECK(decoded->Skeleton().PositionsM == source->Skeleton().PositionsM &&
            decoded->Skeleton().Parents == source->Skeleton().Parents &&
            decoded->Skeleton().Unreached == source->Skeleton().Unreached &&
            decoded->Skeleton().Stats.DistanceChecks == source->Skeleton().Stats.DistanceChecks &&
            decoded->Skeleton().Stats.WorkingBytes == source->Skeleton().Stats.WorkingBytes &&
            decoded->Skeleton().Stop == source->Skeleton().Stop &&
            std::ranges::equal(decoded->Hosts(), source->Hosts()),
        "source positions, parent IDs, unresolved inputs, growth diagnostics and selected hosts "
        "persist");
  CHECK(decoded->Summarize().Variants == source->Summarize().Variants &&
            decoded->Summarize().IndexBuildBytes == 0 &&
            decoded->Summarize().IndexResidentBytes == decoded->Hosts().size() * sizeof(uint32_t),
        "stored selection and counts load directly into compact host capacity");
  for (const auto node : source->Hosts()) {
    const auto original = source->Leaves({.Node = node, .Maximum = 5});
    const auto restored = decoded->Leaves({.Node = node, .Maximum = 5});
    CHECK(original && restored && original->size() == restored->size(),
          "same selected host has the same leaf demand");
    for (size_t row = 0; original && restored && row < original->size(); ++row) {
      CHECK((*original)[row].Id == (*restored)[row].Id &&
                (*original)[row].OwnerNode == (*restored)[row].OwnerNode &&
                (*original)[row].Variant == (*restored)[row].Variant &&
                (*original)[row].Pose.OriginM == (*restored)[row].Pose.OriginM &&
                (*original)[row].Pose.NodeM == (*restored)[row].Pose.NodeM &&
                (*original)[row].Pose.Frame == (*restored)[row].Pose.Frame &&
                (*original)[row].Pose.StemParameter == (*restored)[row].Pose.StemParameter,
            "cache restore preserves exact identities and all regenerated near-detail pose values");
    }
  }
  const auto rewritten = EncodePlantCrownPlan(*decoded, kCrownAssetBudget);
  CHECK(rewritten && *rewritten == *encoded,
        "native encoding is byte-stable despite compacted resident arrays");
  return Report();
}
