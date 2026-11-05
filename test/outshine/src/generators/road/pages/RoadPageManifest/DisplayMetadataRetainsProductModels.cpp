#include "RoadPageManifest.h"
#include "BinaryValueArchive.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kLimit = 4096;

std::optional<RoadPageManifest> Manifest(bool appearance) {
  RoadPageManifest manifest;
  manifest.PlanKey.assign(64, 'a');
  manifest.Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49};
  if (!manifest.Materials.addSurface("streets", Material{})) { return std::nullopt; }
  const auto material = EncodeGeometryProduct(manifest.Materials, kLimit);
  if (!material) { return std::nullopt; }
  manifest.MaterialKey = Sha256Hex(material->data(), material->size());
  RoadPageAddress address{.PlanKey = manifest.PlanKey,
                          .MaterialKey = manifest.MaterialKey,
                          .MaterialIndex = 0,
                          .Anchor = manifest.Anchor,
                          .Level = 0,
                          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}};
  if (appearance) { address.Appearance = RoadPageAppearanceV2{.Key = std::string(64, 'b')}; }
  manifest.Pages.push_back({.Key = RoadPageKey(address),
                            .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                            .ByteCount = 512,
                            .Level = 0,
                            .Parts = address.Parts,
                            .Appearance = address.Appearance});
  if (appearance) {
    manifest.Appearance = RoadPageAppearanceV2{RoadPageManifestAppearanceKey(manifest.Pages)};
  }
  return manifest;
}

void CheckModels(bool appearance) {
  auto manifest = Manifest(appearance);
  CHECK(manifest.has_value(), "the native material and original address form a manifest");
  if (!manifest) { return; }
  const auto original = EncodeRoadPageManifest(*manifest, kLimit);
  CHECK(original.has_value(), "original-only manifests retain their format");
  if (!original) { return; }
  BinaryValueReader reader(*original);
  uint64_t version = 0;
  CHECK(reader(version) && version == (appearance ? 0x0002314d4452ULL : 0x0001314d4452ULL),
        "absent model metadata keeps the existing v1/v2 manifest version");
  auto coarse = manifest->Pages.front();
  coarse.UpperModelErrorM = 1;
  coarse.Key = RoadPageKey({.PlanKey = manifest->PlanKey,
                            .MaterialKey = manifest->MaterialKey,
                            .MaterialIndex = 0,
                            .Anchor = manifest->Anchor,
                            .Level = coarse.Level,
                            .Parts = coarse.Parts,
                            .Appearance = coarse.Appearance,
                            .UpperModelErrorM = coarse.UpperModelErrorM});
  manifest->Pages.push_back(coarse);
  if (appearance) {
    manifest->Appearance = RoadPageAppearanceV2{RoadPageManifestAppearanceKey(manifest->Pages)};
  }
  const auto bytes = EncodeRoadPageManifest(*manifest, kLimit);
  const auto decoded = bytes ? DecodeRoadPageManifest(*bytes, kLimit) : std::nullopt;
  CHECK(decoded && decoded->Pages.size() == 2 && !decoded->Pages[0].UpperModelErrorM &&
            decoded->Pages[1].UpperModelErrorM == 1 &&
            decoded->Pages[0].Parts == decoded->Pages[1].Parts &&
            EncodeRoadPageManifest(*decoded, kLimit) == bytes,
        "metadata independently identifies original and display variants of the same sources");
  if (bytes) {
    for (size_t size = 0; size < bytes->size(); ++size) {
      CHECK(!DecodeRoadPageManifest(std::span(*bytes).first(size), kLimit),
            "no truncated manifest may become complete metadata");
    }
  }
  for (const double error : {2.0, 0.0, -1.0, std::numeric_limits<double>::infinity()}) {
    manifest->Pages[1].UpperModelErrorM = error;
    CHECK(!EncodeRoadPageManifest(*manifest, kLimit),
          "an invalid or differently keyed model cannot masquerade as the stored product");
  }
  manifest->Pages.pop_back();
  if (appearance) {
    manifest->Appearance = RoadPageAppearanceV2{RoadPageManifestAppearanceKey(manifest->Pages)};
  }
  CHECK(EncodeRoadPageManifest(*manifest, kLimit) == original,
        "removing display variants restores byte-identical original metadata");
}
}

int main() {
  CheckModels(false);
  CheckModels(true);
  return Report();
}
