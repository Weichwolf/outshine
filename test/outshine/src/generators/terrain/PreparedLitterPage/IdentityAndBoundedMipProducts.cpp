#include "PreparedLitterPage.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <initializer_list>
#include <cstdint>
#include <span>
#include <limits>
#include <stop_token>
#include <string>
#include <vector>

namespace {
using namespace outshine::Generators;
using namespace outshine::Test;

void IdentityAndViews() {
  const LitterPageAddress address{.Tile = {-65, -64}, .Level = 4};
  const auto key = LitterPageKey(address);
  const auto otherTile = LitterPageKey({.Tile = {-64, -64}, .Level = 4});
  const auto otherLevel = LitterPageKey({.Tile = {-33, -32}, .Level = 5});
  const auto otherCore = LitterPageKey({.Tile = address.Tile, .Level = 4, .CoreTexels = 128});
  CHECK(key && otherTile && otherLevel && *key != *otherTile && *key != *otherLevel,
        "lattice tile and selected mip bind persistent product identity");
  CHECK(key && otherCore && *key != *otherCore, "page layout is part of persistent identity");
  const auto package = PrepareLitterPage(address, 4372);
  CHECK(package && package->Bytes.size() == 4372 && package->Records.size() == 1,
        "request generates only its selected page core, gutter and header");
  if (!package) { return; }
  const auto view = DecodeLitterPage(address, package->Bytes);
  CHECK(view && view->Side == 66 && view->Pixels.size() == 4356 &&
            view->Pixels.data() == package->Bytes.data() + kLitterPageHeaderBytes,
        "decoding borrows the ready native pixels without another buffer");
  CHECK(package->Records[0].Key == *key && package->Records[0].Parent == *otherLevel &&
            package->Records[0].Level == 4 && package->Records[0].ByteCount == 4372,
        "native record carries selected LOD and its coarser address");
  CHECK(!DecodeLitterPage({.Tile = address.Tile, .Level = 5}, package->Bytes),
        "another mip cannot consume a differently addressed package");
  CHECK(!DecodeLitterPage({.Tile = {-64, -64}, .Level = 4}, package->Bytes),
        "another tile cannot consume a differently addressed package");
  CHECK(!DecodeLitterPage({.Tile = address.Tile, .Level = 4, .CoreTexels = 128}, package->Bytes),
        "another page layout cannot consume the same body");
  auto damaged = package->Bytes;
  damaged[0] ^= 1;
  CHECK(!DecodeLitterPage(address, damaged), "changed generator schema is refused");
  damaged = package->Bytes;
  damaged[2] = 0x46;
  damaged[3] = 0x31;
  CHECK(!DecodeLitterPage(address, damaged), "the retired whole-field format is not a page");
  damaged = package->Bytes;
  damaged.push_back(0);
  CHECK(!DecodeLitterPage(address, damaged), "trailing pixel bytes are refused");
  CHECK(!DecodeLitterPage(address, std::span(package->Bytes).first(100)),
        "truncated image bytes are refused");
}

void RejectionsAndBoundary() {
  CHECK(!PrepareLitterPage({.Tile = {0, 0}, .Level = 4}, 4371),
        "output admission is checked before generating the image");
  CHECK(!LitterPageKey({.Tile = {0, 0}, .Level = 6}) &&
            !PrepareLitterPage({.Tile = {0, 0}, .Level = 6}, kLitterPageBytesMost),
        "unsupported mips cannot enter key or generator paths");
  CHECK(!PlanLitterPage({.Tile = {0, 0}, .Level = 0, .CoreTexels = 32}),
        "unsupported page layouts are refused before allocation");
  CHECK(!LitterPageKey({.Tile = {std::numeric_limits<int32_t>::max(), 0}, .Level = 0}),
        "lattice coordinate overflow is rejected before hashing");
  std::stop_source stopping;
  stopping.request_stop();
  CHECK(
      !PrepareLitterPage({.Tile = {0, 0}, .Level = 0}, kLitterPageBytesMost, stopping.get_token()),
      "cancelled generation publishes no package");
  const auto last = PrepareLitterPage({.Tile = {0, 0}, .Level = 5}, 4372);
  CHECK(last && last->Bytes.size() == 4372 && last->Records[0].Parent.empty(),
        "last needed mip keeps one bounded page and has no coarser dependency");
}

void SpatialPlans() {
  for (const uint32_t core : {64u, 128u}) {
    LitterPageAddress child{.Tile = {-65, 70}, .Level = 0, .CoreTexels = core};
    const auto package = PrepareLitterPage(child, kLitterPageBytesMost);
    CHECK(package, "both page candidates generate within the admitted output bound");
    if (!package) { return; }
    const auto view = DecodeLitterPage(child, package->Bytes);
    CHECK(view && view->Side == core + 2,
          "both candidate page sizes encode complete cores and gutters");
    for (uint32_t level = 0; level <= kLitterPageMipMax; ++level) {
      const auto plan = PlanLitterPage(child);
      CHECK(plan && plan->Side == core + 2 && plan->WorkingBytesMost > 0,
            "every ancestor has a finite selected-image and workspace plan");
      const auto parent = LitterPageParent(child);
      if (level == kLitterPageMipMax) {
        CHECK(!parent, "the terminal sampler mip has no generated parent");
      } else {
        CHECK(parent && parent->Level == child.Level + 1 && parent->CoreTexels == core &&
                  parent->Tile[0] ==
                      static_cast<int32_t>(std::floor(static_cast<double>(child.Tile[0]) / 2)),
              "spatial parents floor negative child coordinates and preserve layout");
        if (!parent) { return; }
        child = *parent;
      }
    }
  }
}
}

int main() {
  IdentityAndViews();
  RejectionsAndBoundary();
  SpatialPlans();
  return Report();
}
