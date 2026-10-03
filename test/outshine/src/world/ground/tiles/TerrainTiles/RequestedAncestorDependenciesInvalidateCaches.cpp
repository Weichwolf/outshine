#include "tiles/TerrainTiles.h"
#include "TerrainRevisionIndex.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace {
using namespace outshine;
using namespace outshine::Ground;
constexpr uint8_t kPng[]{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x02, 0x00, 0x00, 0x00, 0x4b,
    0x6d, 0x29, 0xdc, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x68,
    0x60, 0x60, 0xc0, 0x8a, 0xb0, 0x8b, 0x0e, 0x5a, 0x09, 0x00, 0xa1, 0x7c, 0x20, 0x01, 0x64,
    0xc6, 0x93, 0x18, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

class CertifiedAncestor final : public TerrainSource {
public:
  explicit CertifiedAncestor(size_t capacity = 64)
      : Revisions(std::move(*TerrainRevisionIndex::Create(capacity))) {}

  std::unique_ptr<TerrainRevisionIndex> Revisions;
  unsigned Takes = 0;
  std::array<std::optional<TerrainRevisionIndex::Stamp>, 64> Cached;

  TerrainRevisionIndex::Validation
  InspectStamps(std::span<const TerrainRevisionIndex::Stamp> stamps) const override {
    return Revisions->InspectStamps(stamps);
  }

  bool AreCurrent(std::span<const TerrainRevisionIndex::Stamp> stamps) const override {
    return Revisions->AreCurrent(stamps);
  }

  TerrainBytes Take(Data::TileId requested) override {
    ++Takes;
    auto stamp = Revisions->CurrentStamp(requested);
    auto &cached = Cached[requested.X * 8u + requested.Y];
    if (!stamp) {
      stamp = cached ? *Revisions->RestoreCachedStamp(*cached)
                     : *Revisions->IssueDeliveryStamp(requested);
    }
    cached = stamp;
    const Data::TileId parent{.Zoom = 1, .X = 0, .Y = 0};
    return TerrainBytes::From(parent,
                              {kPng, kPng + sizeof(kPng)},
                              {.Kind = Data::DataKind::Elevation,
                               .Tile = parent,
                               .SourceId = "ancestor-dem",
                               .Revision = "r1"},
                              {},
                              stamp);
  }
};
}

int main() {
  using namespace outshine::Test;
  CertifiedAncestor source;
  TerrainCertificate detached;
  const auto decoded = std::make_shared<DecodedCache>(1024u * 1024u);
  {
    TerrainTiles tiles(
        source, EnuFrame::At(Geo{}), {.Shared = decoded, .StitchedFieldBytes = 1024u * 1024u});
    const Data::TileId centre{.Zoom = 3, .X = 2, .Y = 2};
    const auto first = tiles.StitchedField(centre.Zoom, centre.X, centre.Y);
    CHECK(first && first->Sources().size() == 1,
          "all requests share one delivered ancestor identity");
    CHECK(first && first->Certificate().IsComplete(), "complete stencil is certified");
    if (!first) { return Report(); }
    CHECK(first->Certificate().Dependencies().size() == 9,
          "nine requested addresses remain distinct");
    for (uint32_t x = 1; x <= 3; ++x) {
      for (uint32_t y = 1; y <= 3; ++y) {
        const auto stamp = source.Revisions->CurrentStamp({.Zoom = 3, .X = x, .Y = y});
        CHECK(stamp && std::ranges::find(first->Certificate().Dependencies(), *stamp) !=
                           first->Certificate().Dependencies().end(),
              "each requested dependency is represented");
      }
    }
    const auto before = source.Takes;
    CHECK(tiles.StitchedField(centre.Zoom, centre.X, centre.Y) == first,
          "valid stitched cache reuses the product");
    CHECK(source.Takes == before, "valid hit performs no terrain request");
    CHECK(source.Revisions->IssueDeliveryStamp({.Zoom = 3, .X = 7, .Y = 7}).has_value(),
          "unrelated region updates");
    CHECK(tiles.HeldStitched(centre) == first, "unrelated region preserves resident product");
    CHECK(source.Revisions->IssueDeliveryStamp({.Zoom = 3, .X = 1, .Y = 1}).has_value(),
          "corner address updates");
    CHECK(!tiles.HeldStitched(centre), "changed corner revokes the resident product");
    const auto replaced = tiles.StitchedField(centre.Zoom, centre.X, centre.Y);
    CHECK(replaced && replaced != first, "changed corner rebuilds the stitched product");
    CHECK(source.Takes == before + 1, "only changed raw dependency is requested again");
    CHECK(replaced && replaced->Certificate().IsComplete() &&
              source.AreCurrent(replaced->Certificate().Dependencies()),
          "rebuilt dependency set is current");
    CHECK(!source.AreCurrent(first->Certificate().Dependencies()),
          "old borrowed snapshot stays revoked");
    {
      TerrainTiles consumer(
          source, EnuFrame::At(Geo{}), {.Shared = decoded, .StitchedFieldBytes = 1024u * 1024u});
      const auto shared = consumer.StitchedField(centre.Zoom, centre.X, centre.Y);
      CHECK(shared && source.AreCurrent(shared->Certificate().Dependencies()),
            "a second consumer sees the updated raw dependency");
      CHECK(source.Takes == before + 1,
            "updated decoded inputs remain reusable across independent consumers");
    }
    if (replaced) {
      detached = replaced->Certificate();
      auto partial = *replaced;
      partial.MarkMissingBoundary();
      CHECK(!partial.Certificate().IsComplete(), "partial boundary cannot certify input");
    }
  }
  CHECK(detached.IsComplete() && source.AreCurrent(detached.Dependencies()),
        "certificate survives destruction of all decoded and stitched rasters");
  const auto stamp = source.Revisions->CurrentStamp({.Zoom = 3, .X = 2, .Y = 2});
  CHECK(stamp.has_value(), "centre metadata survives raster destruction");
  if (stamp) {
    CHECK(!TerrainCertificate::FromDelivery({.Zoom = 3, .X = 1, .Y = 1}, stamp).IsComplete(),
          "stamp for a different requested address cannot certify delivery");
    auto mixed = TerrainCertificate::FromDelivery(stamp->Requested, stamp);
    const auto next = source.Revisions->IssueDeliveryStamp(stamp->Requested);
    mixed.Merge(TerrainCertificate::FromDelivery(stamp->Requested,
                                                 next ? std::optional(*next) : std::nullopt));
    CHECK(!mixed.IsComplete(), "conflicting revisions cannot merge into a certificate");
  }
  {
    CertifiedAncestor constrained(1);
    TerrainTiles tiles(constrained,
                       EnuFrame::At(Geo{}),
                       {.DemCacheBytes = 1024u * 1024u, .StitchedFieldBytes = 1024u * 1024u});
    const auto usable = tiles.StitchedField(3, 2, 2);
    CHECK(usable && usable->Meshable() && !usable->HasMissingBoundary(),
          "one-entry metadata budget preserves complete usable terrain");
    CHECK(usable && !usable->Certificate().IsComplete(),
          "insufficient metadata cannot certify an optimistic cache hit");
    CHECK(usable && constrained.InspectStamps(usable->Certificate().Dependencies()) ==
                        TerrainRevisionIndex::Validation::Unknown,
          "unchanged cached bytes lose proof rather than becoming a known conflict");
  }
  return Report();
}
