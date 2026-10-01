#include "BuildingField.h"
#include "Check.h"

#include <algorithm>
#include <string>
#include <memory>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Field = Ground::BuildingField;
  auto source = std::make_shared<const Data::SourceProvenance>(
      Data::SourceProvenance{.DatasetId = "external-world", .Revision = "one"});
  const std::weak_ptr<const Data::SourceProvenance> held = source;
  auto geometry = std::make_shared<Ground::BuildingGeometry>();
  geometry->Origin = {.Provenance = source,
                      .Bounds = {.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}};
  Field field;
  Field::Baked product{.Coordinates = geometry};
  field.PreparesAcceptances({.Tiles = 1});
  field.Take(0);
  auto pending = field.PrepareAcceptance(0, product, {}, true);
  field.CommitAcceptance(std::move(pending), product);
  CHECK(field.AcceptedTiles().size() == 1,
        "native product publication retains its reservation without a vector-tile field");
  const uint64_t firstKey = field.InputOfTile(0)->SourceKey;
  const uint64_t firstRevision = field.Revision();
  Field snapshot = field.SnapshotAccepted();
  source.reset();
  geometry = std::make_shared<Ground::BuildingGeometry>(*geometry);
  geometry->Origin.Bounds.EastDeg = 2;
  product.Coordinates = geometry;
  auto changed = field.PrepareAcceptance(0, product, {}, true);
  field.ReplaceAcceptance(std::move(changed), product);
  CHECK(field.InputOfTile(0)->SourceKey != firstKey && field.Revision() != firstRevision,
        "different original source coverage invalidates an otherwise equal product");
  geometry = std::make_shared<Ground::BuildingGeometry>(*geometry);
  geometry->Origin.Provenance = std::make_shared<const Data::SourceProvenance>(
      Data::SourceProvenance{.DatasetId = "external-world", .Revision = "two"});
  geometry->Origin.Bounds.EastDeg = 1;
  product.Coordinates = geometry;
  auto replacement = field.PrepareAcceptance(0, product, {}, true);
  field.ReplaceAcceptance(std::move(replacement), product);
  CHECK(field.InputOfTile(0)->SourceKey != firstKey,
        "original dataset revision participates without a vector tile surrogate");
  const uint64_t revisionKey = field.InputOfTile(0)->SourceKey;
  auto payloadSource = std::make_shared<Data::SourceProvenance>(*geometry->Origin.Provenance);
  payloadSource->PayloadSha256 = {std::string(64, 'a'), std::string(64, 'b')};
  geometry = std::make_shared<Ground::BuildingGeometry>(*geometry);
  geometry->Origin.Provenance = payloadSource;
  product.Coordinates = geometry;
  auto pinned = field.PrepareAcceptance(0, product, {}, true);
  field.ReplaceAcceptance(std::move(pinned), product);
  const uint64_t payloadKey = field.InputOfTile(0)->SourceKey;
  CHECK(payloadKey != revisionKey, "actual original payload pins qualify the published product");
  auto reorderedSource = std::make_shared<Data::SourceProvenance>(*payloadSource);
  std::ranges::reverse(reorderedSource->PayloadSha256);
  geometry = std::make_shared<Ground::BuildingGeometry>(*geometry);
  geometry->Origin.Provenance = reorderedSource;
  product.Coordinates = geometry;
  auto reordered = field.PrepareAcceptance(0, product, {}, true);
  field.ReplaceAcceptance(std::move(reordered), product);
  CHECK(field.InputOfTile(0)->SourceKey == payloadKey,
        "acquisition completion order does not change product identity");
  auto changedSource = std::make_shared<Data::SourceProvenance>(*reorderedSource);
  changedSource->PayloadSha256.front() = std::string(64, 'c');
  geometry = std::make_shared<Ground::BuildingGeometry>(*geometry);
  geometry->Origin.Provenance = changedSource;
  product.Coordinates = geometry;
  auto changedPayload = field.PrepareAcceptance(0, product, {}, true);
  field.ReplaceAcceptance(std::move(changedPayload), product);
  CHECK(field.InputOfTile(0)->SourceKey != payloadKey,
        "changed original bytes invalidate equal dataset revision and bounds");
  const auto *old = snapshot.InputOfTile(0);
  CHECK(!held.expired() && old && old->SourceKey == firstKey && old->Coordinates &&
            old->Coordinates->Origin.Provenance->Revision == "one",
        "published world snapshot retains generic provenance after producer replacement");
  snapshot.ResetDerived();
  CHECK(held.expired(), "retiring the last product releases its source provenance");
  return Report();
}
