#include "PreparedLitterField.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
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
  const LitterFieldAddress address{.Tile = {-65, -64}, .Level = 4};
  const auto key = LitterFieldKey(address);
  const auto otherTile = LitterFieldKey({.Tile = {-64, -64}, .Level = 4});
  const auto otherLevel = LitterFieldKey({.Tile = address.Tile, .Level = 5});
  CHECK(key && otherTile && otherLevel && *key != *otherTile && *key != *otherLevel,
        "lattice tile and selected mip bind persistent product identity");
  const auto package = PrepareLitterField(address, 1040);
  CHECK(package && package->Bytes.size() == 1040 && package->Records.size() == 1,
        "coarse request generates only its selected 1024 texels and header");
  if (!package) { return; }
  const auto view = DecodeLitterField(address, package->Bytes);
  CHECK(view && view->Side == 32 && view->Pixels.size() == 1024 &&
            view->Pixels.data() == package->Bytes.data() + kLitterFieldHeaderBytes,
        "decoding borrows the ready native pixels without another buffer");
  CHECK(package->Records[0].Key == *key && package->Records[0].Parent == *otherLevel &&
            package->Records[0].Level == 4 && package->Records[0].ByteCount == 1040,
        "native record carries selected LOD and its coarser address");
  CHECK(!DecodeLitterField({.Tile = address.Tile, .Level = 5}, package->Bytes),
        "another mip cannot consume a differently addressed package");
  CHECK(!DecodeLitterField({.Tile = {-64, -64}, .Level = 4}, package->Bytes),
        "another tile cannot consume a differently addressed package");
  auto damaged = package->Bytes;
  damaged[0] ^= 1;
  CHECK(!DecodeLitterField(address, damaged), "changed generator schema is refused");
  damaged = package->Bytes;
  damaged.push_back(0);
  CHECK(!DecodeLitterField(address, damaged), "trailing pixel bytes are refused");
  CHECK(!DecodeLitterField(address, std::span(package->Bytes).first(100)),
        "truncated image bytes are refused");
}

void RejectionsAndBoundary() {
  CHECK(!PrepareLitterField({.Tile = {0, 0}, .Level = 4}, 1039),
        "output admission is checked before generating the image");
  CHECK(!LitterFieldKey({.Tile = {0, 0}, .Level = 10}) &&
            !PrepareLitterField({.Tile = {0, 0}, .Level = 10}, kLitterFieldBytesMost),
        "unsupported mips cannot enter key or generator paths");
  CHECK(!LitterFieldKey({.Tile = {std::numeric_limits<int32_t>::max(), 0}, .Level = 0}),
        "lattice coordinate overflow is rejected before hashing");
  std::stop_source stopping;
  stopping.request_stop();
  CHECK(!PrepareLitterField(
            {.Tile = {0, 0}, .Level = 0}, kLitterFieldBytesMost, stopping.get_token()),
        "cancelled generation publishes no package");
  const auto last = PrepareLitterField({.Tile = {0, 0}, .Level = 9}, 17);
  CHECK(last && last->Bytes.size() == 17 && last->Records[0].Parent.empty(),
        "last mip needs one native texel and has no coarser dependency");
}
}

int main() {
  IdentityAndViews();
  RejectionsAndBoundary();
  return Report();
}
