#ifndef OUTSHINE_TEST_TRANSPORTPROVENANCEFIXTURE_H
#define OUTSHINE_TEST_TRANSPORTPROVENANCEFIXTURE_H
#include "TransportProvenance.h"
#include <cstdint>

namespace outshine::Test {
inline World::TransportProvenance TransportProvenanceFixture() {
  using namespace World;
  TransportProvenance provenance;
  provenance.IdentitySpace = TransportIdentitySpace::DerivedFragments;
  provenance.Sources = {
      {.Identity = {.DatasetId = "source-a", .Revision = "revision-1"}, .Item = "opaque-item-a"},
      {.Identity = {.DatasetId = "source-b", .Revision = "revision-2"}, .Item = "opaque-item-b"}};
  provenance.Sources[0].Content[0] = 1;
  provenance.Sources[1].Content[0] = 2;
  const auto width = static_cast<uint16_t>(TransportAttribute::Width);
  const auto direction = static_cast<uint16_t>(TransportAttribute::Direction);
  provenance.Paths = {{.PathId = 42,
                       .ReferenceCount = 2,
                       .SuppliedFields = static_cast<uint16_t>(width | direction),
                       .DerivedFields = width}};
  provenance.References = {{.Source = 0, .SegmentOrdinal = 3, .ProviderElementId = 0},
                           {.Source = 1, .SegmentOrdinal = 7}};
  provenance.References[0].Fragment[0] = 3;
  provenance.References[1].Fragment[0] = 4;
  return provenance;
}
}
#endif
