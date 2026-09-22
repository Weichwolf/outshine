#include "tiles/TerrainTiles.h"
#include "Check.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace outshine;
using namespace outshine::Ground;

constexpr uint8_t kPng[]{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x00, 0x00, 0x00, 0x08,
    0xd6, 0x28, 0xbb, 0x00, 0x00, 0x00, 0x21, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x68,
    0xb0, 0x65, 0x20, 0x80, 0x16, 0x32, 0x34, 0xb2, 0x32, 0x34, 0x66, 0x32, 0x34, 0x9e, 0x65,
    0x68, 0x32, 0x64, 0x20, 0xa4, 0x7a, 0x00, 0x34, 0x00, 0x00, 0x57, 0xeb, 0x32, 0xc5, 0x0a,
    0xb4, 0xfd, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

class Fixture final : public TerrainSource {
public:
  explicit Fixture(std::string revision) : Revision_(std::move(revision)) {}

  TerrainBytes Take(Data::TileId at) override {
    ++Calls;
    return TerrainBytes::From(at,
                              {kPng, kPng + sizeof(kPng)},
                              {.Kind = Data::DataKind::Elevation,
                               .Tile = at,
                               .SourceId = "fixture-dem",
                               .Revision = Revision_});
  }

  int Calls = 0;

private:
  std::string Revision_;
};

TerrainTiles::Config Cache() {
  return {.DemCacheBytes = 1024u * 1024u, .StitchedFieldBytes = 1024u * 1024u};
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  Fixture first("r1");
  TerrainTiles tiles(first, EnuFrame::At(Geo{}), Cache());
  const TerrainGrid stitched = tiles.StitchedGrid(2, 1, 1);
  const TerrainField *field = stitched.TryField();
  CHECK(field != nullptr, "centre and eight neighbours decode into a stitched field");
  if (!field) { return Report(); }
  const auto sources = field->Sources();
  CHECK(sources.size() == 9 && std::ranges::is_sorted(sources),
        "stitched height field owns nine unique sources in stable order");
  CHECK(std::ranges::all_of(sources,
                            [](const Data::TileSourceIdentity &source) {
                              return source.SourceId == "fixture-dem" && source.Revision == "r1";
                            }),
        "every contributing raw DEM keeps the declared revision");
  const int decoded = first.Calls;
  const TerrainGrid cached = tiles.StitchedGrid(2, 1, 1);
  const TerrainField *cachedField = cached.TryField();
  CHECK(cachedField && std::ranges::equal(sources, cachedField->Sources()) &&
            first.Calls == decoded,
        "decoded-cache hits preserve the exact stitched source set without refetching");

  Fixture second("r2");
  TerrainTiles revised(second, EnuFrame::At(Geo{}), Cache());
  const TerrainGrid revisedGrid = revised.StitchedGrid(2, 1, 1);
  const TerrainField *revisedField = revisedGrid.TryField();
  CHECK(revisedField && revisedField->Sources().size() == 9 &&
            revisedField->Sources().front().Revision == "r2" &&
            !std::ranges::equal(sources, revisedField->Sources()),
        "equal DEM payloads from different source revisions stay distinguishable");

  Fixture unused("r1");
  TerrainTiles shaped(unused, EnuFrame::At(Geo{}), Cache());
  shaped.Shapes({.Kind = "sineRidge", .AmplitudeM = 30.0, .WavelengthM = 100.0});
  const auto before = shaped.StitchedField(0, 0, 0);
  shaped.Shapes({.Kind = "sineRidge", .AmplitudeM = 60.0, .WavelengthM = 100.0});
  const auto after = shaped.StitchedField(0, 0, 0);
  CHECK(before && after && before->Sources().size() == 1 && after->Sources().size() == 1 &&
            before->Sources().front().From == Data::TileSourceIdentity::Origin::Shaped &&
            !(before->Sources().front() == after->Sources().front()) && unused.Calls == 0,
        "changing shaped-terrain parameters invalidates the stitched cache and source identity");
  return Report();
}
