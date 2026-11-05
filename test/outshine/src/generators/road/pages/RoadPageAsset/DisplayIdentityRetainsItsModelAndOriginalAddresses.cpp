#include "RoadPageAsset.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

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
                       .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}}};
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
            RoadPageKey(decoded->Address) == RoadPageKey(page.Address) &&
            decoded->Geometry.Mesh.Index == page.Geometry.Mesh.Index,
        "independent decoding preserves model identity, appearance and geometry");
  auto truncated = *bytes;
  truncated.pop_back();
  CHECK(!DecodeRoadPageAsset(truncated, kLimit), "truncated geometry cannot become a cache hit");
}
}

int main() {
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
