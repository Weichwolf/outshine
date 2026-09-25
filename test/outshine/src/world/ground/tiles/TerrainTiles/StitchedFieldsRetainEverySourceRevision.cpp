#include "tiles/TerrainTiles.h"
#include "HeightField.h"
#include "Check.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace outshine;
using namespace outshine::Ground;

constexpr uint8_t kPng[]{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x02, 0x00, 0x00, 0x00, 0x4b,
    0x6d, 0x29, 0xdc, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x68,
    0x60, 0x60, 0xc0, 0x8a, 0xb0, 0x8b, 0x0e, 0x5a, 0x09, 0x00, 0xa1, 0x7c, 0x20, 0x01, 0x64,
    0xc6, 0x93, 0x18, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

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

class AncestorFixture final : public TerrainSource {
public:
  explicit AncestorFixture(std::string revision) : Revision_(std::move(revision)) {}

  TerrainBytes Take(Data::TileId at) override {
    ++Calls;
    const Data::TileId parent{.Zoom = at.Zoom - 1, .X = at.X >> 1u, .Y = at.Y >> 1u};
    return TerrainBytes::From(parent,
                              {kPng, kPng + sizeof(kPng)},
                              {.Kind = Data::DataKind::Elevation,
                               .Tile = parent,
                               .SourceId = "ancestor-dem",
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
  std::vector<float> nodes;
  std::vector<Data::TileSourceIdentity> meshSources;
  uint32_t postings = 0;
  int side = 0;
  CHECK(shaped.NodesOf({.Zoom = 0, .X = 0, .Y = 0}, 4, &nodes, &meshSources, &postings, &side) ==
                TerrainGrid::State::Decoded &&
            before && std::ranges::equal(before->Sources(), meshSources),
        "sampled mesh product retains the stitched source set");
  shaped.Shapes({.Kind = "sineRidge", .AmplitudeM = 30.0, .WavelengthM = 100.0});
  CHECK(shaped.StitchedField(0, 0, 0) == before,
        "repeating unchanged shape parameters preserves the stitched cache");
  shaped.Shapes({.Kind = "sineRidge", .AmplitudeM = 60.0, .WavelengthM = 100.0});
  const auto after = shaped.StitchedField(0, 0, 0);
  CHECK(before && after && before->Sources().size() == 1 && after->Sources().size() == 1 &&
            before->Sources().front().From == Data::TileSourceIdentity::Origin::Shaped &&
            !(before->Sources().front() == after->Sources().front()) && unused.Calls == 0,
        "changing shaped-terrain parameters invalidates the stitched cache and source identity");

  AncestorFixture ancestor("parent-r1");
  TerrainTiles cropped(ancestor, EnuFrame::At(Geo{}), Cache());
  const Data::TileId child{.Zoom = 2, .X = 1, .Y = 1};
  const auto croppedField = cropped.StitchedField(child.Zoom, child.X, child.Y);
  CHECK(croppedField && !croppedField->Sources().empty() &&
            std::ranges::all_of(croppedField->Sources(),
                                [](const auto &source) {
                                  return source.Tile.Zoom == 1 &&
                                         source.SourceId == "ancestor-dem" &&
                                         source.Revision == "parent-r1";
                                }),
        "cropped child field retains the actual ancestor source address and revision");
  if (croppedField) {
    HeightField::Block block;
    CHECK(HeightField::CopiesField(*croppedField, child, block) &&
              HeightField::Of(child.Zoom, {block})->Qualified(),
          "a structure bake can qualify an identified ancestor DEM");
  }
  const int ancestorReads = ancestor.Calls;
  const auto cachedAncestor = cropped.StitchedField(child.Zoom, child.X, child.Y);
  CHECK(cachedAncestor && croppedField &&
            std::ranges::equal(cachedAncestor->Sources(), croppedField->Sources()) &&
            ancestor.Calls == ancestorReads,
        "stitched cache hit retains the cropped ancestor provenance without refetching");
  const TerrainGrid decodedAncestor = cropped.StitchedGrid(child.Zoom, child.X, child.Y);
  const TerrainField *decodedField = decodedAncestor.TryField();
  CHECK(decodedField && croppedField &&
            std::ranges::equal(decodedField->Sources(), croppedField->Sources()) &&
            ancestor.Calls == ancestorReads,
        "decoded-cache hit also retains every ancestor source without refetching");
  AncestorFixture revisedAncestor("parent-r2");
  TerrainTiles revisedCropped(revisedAncestor, EnuFrame::At(Geo{}), Cache());
  const auto revisedAncestorField = revisedCropped.StitchedField(child.Zoom, child.X, child.Y);
  CHECK(revisedAncestorField && croppedField &&
            !std::ranges::equal(revisedAncestorField->Sources(), croppedField->Sources()),
        "equal cropped heights from a different ancestor revision are distinct inputs");
  Fixture lruSource("r1");
  const auto tiny = std::make_shared<const TerrainField>(2, 2);
  TerrainTiles::Config limited;
  limited.StitchedFieldBytes = 2u * tiny->Bytes();
  TerrainTiles lru(lruSource, EnuFrame::At(Geo{}), limited);
  const Data::TileId a{.Zoom = 2, .X = 0, .Y = 0};
  const Data::TileId b{.Zoom = 2, .X = 1, .Y = 0};
  const Data::TileId c{.Zoom = 2, .X = 2, .Y = 0};
  lru.HoldsStitched(a, tiny);
  lru.HoldsStitched(b, tiny);
  CHECK(lru.HeldStitched(a) == tiny, "a held-field scan reads without promoting its eviction age");
  lru.HoldsStitched(c, tiny);
  CHECK(!lru.HeldStitched(a) && lru.HeldStitched(b) == tiny && lru.HeldStitched(c) == tiny,
        "a scan cannot make the oldest field survive budget eviction");
  lru.HoldsStitched(a, tiny);
  CHECK(lru.StitchedField(c.Zoom, c.X, c.Y) == tiny,
        "an explicit field request promotes the requested field");
  lru.HoldsStitched(b, tiny);
  CHECK(lru.HeldStitched(b) == tiny && lru.HeldStitched(c) == tiny && !lru.HeldStitched(a),
        "an explicit promotion survives the next eviction");
  lru.Shapes({.Kind = "sineRidge", .AmplitudeM = 1.0});
  CHECK(!lru.HeldStitched(a) && !lru.HeldStitched(c),
        "a shape change invalidates all stitched fields");
  return Report();
}
