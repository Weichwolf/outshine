#include "RoadPageManifest.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include "Check.h"
#include <cstddef>
#include <variant>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kLimit = 4096;
constexpr size_t kLegacyPageBytes = 396;
constexpr size_t kLegacyManifestBytes = 1122;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};
}

int main() {
  RoadPageManifest manifest;
  manifest.PlanKey.assign(64, 'a');
  manifest.Anchor = kAnchor;
  CHECK(manifest.Materials.addSurface("streets", Material{}), "the original surface is valid");
  const auto material = EncodeGeometryProduct(manifest.Materials, kLimit);
  if (!material) { return 1; }
  manifest.MaterialKey = Sha256Hex(material->data(), material->size());
  RoadPageAsset page;
  page.Address = {.PlanKey = manifest.PlanKey,
                  .MaterialKey = manifest.MaterialKey,
                  .MaterialIndex = 0,
                  .Anchor = manifest.Anchor,
                  .Level = 0,
                  .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}};
  page.Geometry.Parts = page.Address.Parts;
  page.Geometry.Mesh.PositionM = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  page.Geometry.Mesh.NormalM = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  page.Geometry.Mesh.ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  page.Geometry.Mesh.Index = {0, 1, 2};
  const auto bytes = EncodeRoadPageAsset(page, kLimit);
  if (!bytes) { return 1; }
  const Box bounds{.Min = {{1, 2, 3}}, .Max = {{4, 5, 6}}};
  manifest.Pages.push_back({.Key = RoadPageKey(page.Address),
                            .Bounds = bounds,
                            .ByteCount = bytes->size(),
                            .Level = 0,
                            .Parts = page.Address.Parts});
  const auto packet = EncodeRoadPageManifest(manifest, kLimit);
  if (!packet) { return 1; }
  CHECK(RoadPageKey(page.Address) ==
            "ddd0ee519d60ebce6c88333d9cff68b2d16c667cf15e92e65e7872b07b21585d",
        "the pre-appearance v1 address retains its original key");
  CHECK(bytes->size() == kLegacyPageBytes &&
            Sha256Hex(bytes->data(), bytes->size()) ==
                "857bca1e16ba93459a4be6d456cde2fa47dc53ce915de4ec95f06bb3d1bb42fc",
        "the pre-appearance v1 page retains every encoded word");
  CHECK(packet->size() == kLegacyManifestBytes &&
            RoadPageManifestKey(manifest, kLimit) ==
                "377fbc8c938857a139e9e966cad45d130c6a6bfc470d6ec37002796ee89fbafa",
        "the pre-appearance v1 manifest retains every encoded word and dependency key");
  const auto restored = DecodeRoadPageAsset(*bytes, kLimit);
  const auto restoredManifest = DecodeRoadPageManifest(*packet, kLimit);
  CHECK(restored && restoredManifest &&
            std::holds_alternative<LegacyRoadPageV1>(restored->Address.Appearance) &&
            std::holds_alternative<LegacyRoadPageV1>(restoredManifest->Appearance) &&
            EncodeRoadPageAsset(*restored, kLimit) == bytes &&
            EncodeRoadPageManifest(*restoredManifest, kLimit) == packet,
        "legacy state is explicit and original packets round-trip without a global migration");
  return Report();
}
