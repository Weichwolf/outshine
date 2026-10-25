#include "TransportRoadPageProducer.h"
#include "TransportPlanCodec.h"
#include "TransportPlanFixture.h"
#include "RoadPageManifest.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <optional>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr float kColour = 0.5f;
constexpr size_t kAppearanceBytes = 4 + 64;

std::string MaterialKey(float roughness) {
  Geometry materials;
  Material material;
  material.Roughness = roughness;
  if (!materials.addSurface("streets", material)) { return {}; }
  const auto bytes = EncodeGeometryProduct(materials, kLimit);
  return bytes ? Sha256Hex(bytes->data(), bytes->size()) : std::string{};
}

void CheckAddress(const TransportPlanLease &plan) {
  std::vector<RoadPagePartAppearance> selected{
      {.Part = {.Kind = RoadPagePartKind::Interval, .Index = 0},
       .WearsLinear = {{kColour, kColour, kColour}}}};
  const auto key = TransportPlanContentKey(plan->Definition, plan->InputBinding);
  CHECK(key.has_value(), "the unchanged functional plan has a colour-free key");
  if (!key) { return; }
  const RoadPageAddress address{.PlanKey = *key,
                                .MaterialKey = MaterialKey(kColour),
                                .MaterialIndex = 0,
                                .Anchor = plan->Anchor,
                                .Level = 0,
                                .Parts = {selected[0].Part},
                                .Appearance =
                                    RoadPageAppearanceV2{RoadPageAppearanceKey(selected)}};
  const auto firstKey = RoadPageKey(address);
  CHECK(!firstKey.empty(), "v2 has a mandatory valid appearance binding");
  const auto producer = TransportRoadPageProducer::Bind(plan, address, selected);
  CHECK(producer.has_value(), "the immutable plan and exact selected bindings form a producer");
  if (!producer) { return; }
  const RoadPageRequest request{
      .Address = address, .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit};
  const auto mesh = producer->Produce(request, {});
  CHECK(mesh && !mesh->Mesh.Index.empty(), "the bound producer builds real native road geometry");
  if (mesh) {
    RoadPageAsset packet{.Address = address, .Geometry = *mesh};
    const auto encoded = EncodeRoadPageAsset(packet, kLimit);
    const auto decoded = encoded ? DecodeRoadPageAsset(*encoded, kLimit) : std::nullopt;
    CHECK(decoded && decoded->Address.Appearance == address.Appearance,
          "mandatory page appearance survives decoding");
    packet.Address.Appearance = LegacyRoadPageV1{};
    const auto legacy = EncodeRoadPageAsset(packet, kLimit);
    CHECK(encoded && legacy && encoded->size() == legacy->size() + kAppearanceBytes,
          "v2 adds only its single framed SHA-256 appearance key");
    if (encoded) {
      constexpr size_t appearanceOffset = 8 + 2 * (4 + 64);
      auto nullKey = *encoded;
      std::fill_n(nullKey.begin() + appearanceOffset + 4, 64, uint8_t{'0'});
      CHECK(!DecodeRoadPageAsset(nullKey, kLimit), "null appearance is rejected on the wire");
      auto missing = *encoded;
      missing.erase(missing.begin() + appearanceOffset,
                    missing.begin() + appearanceOffset + kAppearanceBytes);
      CHECK(!DecodeRoadPageAsset(missing, kLimit),
            "v2 never falls back when its binding is missing");
    }
  }
  auto changed = address;
  selected[0].WearsLinear[0] += kColour;
  changed.Appearance = RoadPageAppearanceV2{RoadPageAppearanceKey(selected)};
  const auto recoloured = TransportRoadPageProducer::Bind(plan, changed, selected);
  CHECK(
      changed.PlanKey == address.PlanKey && changed.MaterialKey == address.MaterialKey &&
          RoadPageKey(changed) != firstKey && recoloured &&
          !producer->Produce(
              {.Address = changed, .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit}, {}),
      "only consumed lane colour changes the page, never its functional or PBR key");
  if (recoloured) {
    const auto product = recoloured->Produce(
        {.Address = changed, .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit}, {});
    CHECK(product && mesh && product->Mesh.PositionM == mesh->Mesh.PositionM &&
              product->Mesh.NormalM == mesh->Mesh.NormalM &&
              product->Mesh.Index == mesh->Mesh.Index &&
              product->Mesh.ColourRgba != mesh->Mesh.ColourRgba,
          "the colour contrast preserves exact geometry and changes actual colour words");
  }
  changed = address;
  changed.MaterialKey = MaterialKey(1);
  CHECK(changed.PlanKey == address.PlanKey && changed.Appearance == address.Appearance &&
            RoadPageKey(changed) != firstKey,
        "an independent PBR binding changes only the render page");
  changed = address;
  changed.Appearance = RoadPageAppearanceV2{};
  CHECK(RoadPageKey(changed).empty(), "empty v2 appearance cannot be a wildcard cache hit");
  changed.Appearance = RoadPageAppearanceV2{std::string(64, '0')};
  CHECK(RoadPageKey(changed).empty(), "an all-zero v2 appearance cannot be a null cache hit");
  changed.Appearance = LegacyRoadPageV1{};
  CHECK(!RoadPageKey(changed).empty() && RoadPageKey(changed) != firstKey &&
            !TransportRoadPageProducer::Bind(plan, changed, selected),
        "legacy is explicit and cannot substitute for a new bound producer");
  changed = address;
  changed.Level = 1;
  CHECK(!TransportRoadPageProducer::Bind(plan, changed, selected),
        "unsupported representations cannot silently alias the implemented level");
  auto partial = std::make_shared<TransportPlan>(*plan);
  partial->Coverage = TransportCoverage::Partial;
  CHECK(!TransportRoadPageProducer::Bind(partial, address, selected),
        "partial functional coverage is never offered as a complete page producer");
  CHECK(
      producer->InputCapacityBytes() >= TransportPlanCapacityBytes(*plan),
      "admission conservatively charges the retained plan capacity separately from physical peaks");
}

void CheckPalette() {
  const auto bindings = [] {
    return std::array{
        RoadPagePartAppearance{.Part = {.Kind = RoadPagePartKind::Interval, .Index = 0},
                               .WearsLinear = {{kColour, kColour, kColour}}}};
  };
  CHECK(RoadPageAppearanceKey(bindings()) ==
            "05fe954f2a3dfae083e0eed72b44c02cd18465c450719786bb4ffcc2b22364cf",
        "independently framed native value words fix the v2 domain and field order");
  auto invalid = bindings();
  invalid[0].WearsLinear[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(RoadPageAppearanceKey(invalid).empty(),
        "nonfinite consumed surface colours cannot be bound");
  std::array duplicated{bindings()[0], bindings()[0]};
  CHECK(RoadPageAppearanceKey(duplicated).empty(), "duplicated selections cannot hide ownership");
}
}

int main() {
  const auto fixture = TransportPlanFixture();
  CHECK(fixture.has_value(), "the existing complete functional fixture is available");
  if (fixture) { CheckAddress(std::make_shared<const TransportPlan>(*fixture)); }
  CheckPalette();
  return Report();
}
