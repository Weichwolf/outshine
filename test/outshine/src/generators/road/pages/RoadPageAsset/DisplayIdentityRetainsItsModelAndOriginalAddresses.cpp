#include "RoadPageAsset.h"
#include "ClusterCook.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kLimit = 4096;

RoadPageAsset Triangle() {
  return {.Address = {.PlanKey = std::string(64, 'a'),
                      .MaterialKey = std::string(64, 'b'),
                      .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49},
                      .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}},
          .Geometry = {.Mesh = {.PositionM = {0, 0, 0, 1, 0, 0, 0, 0, -1},
                                .NormalM = {0, 1, 0, 0, 1, 0, 0, 1, 0},
                                .ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                                .Index = {0, 1, 2}},
                       .Swept = {},
                       .Diagnostics = {},
                       .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}},
                       .Clusters = {}}};
}

void Roundtrip(const RoadPageAsset &page) {
  const auto bytes = EncodeRoadPageAsset(page, kLimit);
  CHECK(bytes.has_value(), "the complete page has a bounded portable representation");
  if (!bytes) { return; }
  const auto &mesh = page.Geometry.Mesh;
  const size_t payload =
      (mesh.PositionM.size() + mesh.NormalM.size() + mesh.ColourRgba.size()) * sizeof(float) +
      mesh.Index.size() * sizeof(uint32_t);
  const size_t header = bytes->size() - payload;
  CHECK(RoadPageHeaderFits(page.Address, header) && !RoadPageHeaderFits(page.Address, header - 1),
        "header admission checks exact framing bytes independently of replacement validity");
  const auto decoded = DecodeRoadPageAsset(*bytes, kLimit);
  CHECK(decoded && decoded->Address.UpperModelErrorM == page.Address.UpperModelErrorM &&
            decoded->Address.Appearance == page.Address.Appearance &&
            decoded->Address.DisplayPolicy == page.Address.DisplayPolicy &&
            RoadPageKey(decoded->Address) == RoadPageKey(page.Address) &&
            decoded->Geometry.Mesh.Index == page.Geometry.Mesh.Index,
        "independent decoding preserves model identity, appearance and geometry");
  auto truncated = *bytes;
  truncated.pop_back();
  CHECK(!DecodeRoadPageAsset(truncated, kLimit), "truncated geometry cannot become a cache hit");
}

void CheckPrepared(RoadPageAsset page) {
  const auto originalKey = RoadPageKey(page.Address);
  page.Address.Clustered = true;
  auto &mesh = page.Geometry.Mesh;
  auto prepared =
      CookClusters({.PositionsM = mesh.PositionM, .Indices = mesh.Index}, kClusterTriangles);
  CHECK(prepared, "the producer prepares native index ranges and culling bounds");
  if (!prepared) { return; }
  mesh.Index = std::move(prepared->Index);
  page.Geometry.Clusters = std::move(prepared->Clusters);
  const auto bytes = EncodeRoadPageAsset(page, kLimit);
  const auto decoded = bytes ? DecodeRoadPageAsset(*bytes, kLimit) : std::nullopt;
  CHECK(decoded && decoded->Address.Clustered &&
            decoded->Address.Appearance == page.Address.Appearance &&
            decoded->Address.UpperModelErrorM == page.Address.UpperModelErrorM &&
            decoded->Address.DisplayPolicy == page.Address.DisplayPolicy &&
            decoded->Geometry.Clusters == page.Geometry.Clusters &&
            decoded->Geometry.Mesh.Index == mesh.Index &&
            RoadPageKey(decoded->Address) == RoadPageKey(page.Address) &&
            RoadPageKey(page.Address) != originalKey,
        "prepared topology roundtrips with distinct identity for every model and appearance");
  if (bytes) {
    for (size_t size = 0; size < bytes->size(); ++size) {
      CHECK(!DecodeRoadPageAsset(std::span(*bytes).first(size), kLimit),
            "no truncated cluster payload becomes a ready product");
    }
  }
  page.Address.Clustered = false;
  CHECK(!EncodeRoadPageAsset(page, kLimit), "raw identity cannot hide prepared topology");
  page.Address.Clustered = true;
  ++page.Geometry.Clusters.front().First;
  CHECK(!EncodeRoadPageAsset(page, kLimit), "prepared ranges cannot lose source indices");
}
}

int main() {
  for (const bool appearance : {false, true}) {
    auto prepared = Triangle();
    if (appearance) { prepared.Address.Appearance = RoadPageAppearanceV2{std::string(64, 'c')}; }
    CheckPrepared(prepared);
    prepared.Address.UpperModelErrorM = 1;
    CheckPrepared(prepared);
    prepared.Address.DisplayPolicy = RoadDisplayPolicy::UpperWithOriginalBodies;
    CheckPrepared(prepared);
  }
  auto page = Triangle();
  CHECK(RoadPageKey(page.Address) ==
            "14c446b49c1e38a9e800223545ac0355ad96b55e3be26764c66b2ee813b643a3",
        "independently encoded original v2 address keeps its existing key");
  Roundtrip(page);
  const std::string originalKey = RoadPageKey(page.Address);
  page.Address.UpperModelErrorM = 1;
  const std::string displayKey = RoadPageKey(page.Address);
  CHECK(displayKey.size() == 64 && displayKey != originalKey,
        "a bounded Upper display product cannot alias its original");
  Roundtrip(page);
  page.Address.UpperModelErrorM = 0.25;
  CHECK(RoadPageKey(page.Address) != displayKey,
        "different model budgets have distinct identities");
  Roundtrip(page);
  page.Address.UpperModelErrorM.reset();
  page.Address.Appearance = RoadPageAppearanceV2{.Key = std::string(64, 'c')};
  CHECK(RoadPageKey(page.Address) ==
            "0dc1dd00223452520fe3db38157f4ea499e33dd3fd8d877875c2bd31237a1455",
        "independently encoded original v3 appearance address keeps its existing key");
  Roundtrip(page);
  page.Address.UpperModelErrorM = 1;
  Roundtrip(page);
  const auto upperKey = RoadPageKey(page.Address);
  CHECK(upperKey == "fc8044587115e14ae827fbf81444795065ceb871ba5d2ff7dfd1be6a45a274ec",
        "the independently encoded v4 Upper-only address retains its existing key");
  page.Address.DisplayPolicy = RoadDisplayPolicy::UpperWithOriginalBodies;
  CHECK(RoadPageKey(page.Address) != upperKey,
        "keeping original bodies requires a distinct explicit product identity");
  Roundtrip(page);
  page.Address.DisplayPolicy = static_cast<RoadDisplayPolicy>(99);
  CHECK(RoadPageKey(page.Address).empty() && !EncodeRoadPageAsset(page, kLimit),
        "unknown display policies cannot identify native products");
  page.Address.DisplayPolicy = RoadDisplayPolicy::UpperWithOriginalBodies;
  page.Address.UpperModelErrorM.reset();
  CHECK(RoadPageKey(page.Address).empty(), "a mixed policy requires an explicit Upper model");
  page.Address.DisplayPolicy = RoadDisplayPolicy::UpperOnly;
  for (const double bad : {0.0,
                           -1.0,
                           std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
    page.Address.UpperModelErrorM = bad;
    CHECK(RoadPageKey(page.Address).empty() && !EncodeRoadPageAsset(page, kLimit),
          "nonpositive or nonfinite display precision cannot identify a product");
  }
  page = Triangle();
  page.Geometry.Swept.Refused = 1;
  page.Geometry.Diagnostics.push_back({.Part = page.Address.Parts.front(),
                                       .Source = 1,
                                       .Stats = {.Refused = 1, .Rejections = {}},
                                       .Empty = true});
  CHECK(EncodeRoadPageAsset(page, kLimit).has_value(),
        "original pages retain explicit omission diagnostics");
  page.Address.UpperModelErrorM = 1;
  CHECK(!EncodeRoadPageAsset(page, kLimit),
        "incomplete pages cannot advertise a bounded replacement");
  page = Triangle();
  page.Address.UpperModelErrorM = 1;
  page.Geometry.Mesh.Index.clear();
  CHECK(!EncodeRoadPageAsset(page, kLimit),
        "a nonempty source set cannot be replaced by an empty page");
  return Report();
}
