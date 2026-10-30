#ifndef OUTSHINE_TEST_CANONICALGROUNDFIELDFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDFIELDFIXTURE_H
#include "CanonicalGroundSourceFixture.h"
#include "CanonicalGroundTerrainFixture.h"
#include "Check.h"
#include "GroundPortField.h"
#include "MvtGroundFieldScope.h"
#include "ScopedGroundSourceFixture.h"
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Test::CanonicalGroundFixture {
using namespace outshine::Generators;

inline void AppendField(std::vector<GroundPortField> &fields,
                        GroundPortFieldRequest request,
                        const std::shared_ptr<const GroundPortTerrain> &terrain,
                        double priorM) {
  auto field = GroundPortField::Make(std::move(request), terrain, priorM);
  CHECK(field.has_value(), "the original finite affine/C1/scoped field is built before selection");
  if (field) { fields.push_back(std::move(*field)); }
}

inline std::vector<GroundPortField> LoadFields() {
  std::vector<GroundPortField> fields;
  auto source = Load();
  auto terrain = LoadTerrain();
  CHECK(source && terrain, "original full-component source and original pinned raster load");
  if (!source || !terrain) { return fields; }
  auto bound =
      GroundPortTerrain::Bind(terrain->Input, terrain->SourceDatum, terrain->HeightMapping);
  auto index = GroundPortSources::Make(source->Snapshot, source->Profiles, source->Unknown);
  CHECK(bound && index, "owned original immutable source and DEM remain shared");
  if (!bound || !index) { return fields; }
  auto held = std::make_shared<const GroundPortTerrain>(std::move(*bound));
  for (size_t which = 0; which < 2; ++which) {
    auto request = GroundPortFieldRequest::Prepare(*index, source->Components[which].front());
    CHECK(request.has_value(), "original five/six-span requests retain the strict source entry");
    if (request) { AppendField(fields, std::move(*request), held, .5); }
  }
  for (const unsigned publications : {1U, 2U}) {
    auto scoped = ScopedGroundFixture::Load(publications);
    CHECK(scoped.has_value(), "the actual two-span source is published before native ingest");
    if (!scoped) { continue; }
    auto candidate = scoped->Index.Candidate({.PathId = 8409897393650800292ULL});
    CHECK(candidate.has_value(), "the actual finite source component has a structural candidate");
    if (!candidate) { continue; }
    auto scope =
        Osm::BindMvtGroundFieldScope(scoped->Snapshot, scoped->Inputs, std::move(*candidate));
    CHECK(scope.has_value(), "actual whole original incidence qualifies before DEM");
    if (!scope) { continue; }
    auto request = GroundPortFieldRequest::PrepareScoped(std::move(scope->Field));
    CHECK(request.has_value(), "owned scoped source enters the same existing finite producer");
    if (request) { AppendField(fields, std::move(*request), held, 3); }
  }
  return fields;
}

}
#endif
