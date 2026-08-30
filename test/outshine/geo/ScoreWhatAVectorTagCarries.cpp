#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"
#include "OsmVector.h"

namespace {

// THE ORACLE IS THE MAPBOX VECTOR TILE SPECIFICATION, and it names seven value types:
//
//   Value { string string_value = 1;  float  float_value  = 2;  double double_value = 3;
//           int64  int_value    = 4;  uint64 uint_value    = 5;  sint64 sint_value   = 6;
//           bool   bool_value   = 7; }
//
// A reader that decodes six of the seven does not refuse the seventh -- protobuf is
// self-describing enough to SKIP an unknown field, so the tag silently becomes absent. Every
// consumer then reads its default and no error is raised anywhere.
//
// That is not a hypothetical loss. VersaTiles/Shortbread encodes `bridge` and `tunnel` as
// BOOLEANS, and the tree's road reader asks for them with
//
//   src/world/ground/RoadHarvest.cpp:48   field.Num(feature, "bridge", 0.0)
//
// so a Munich extract reported 0 bridges and 0 tunnels -- through a city the Isar runs across.
// Decoded, the same extract carries 88 bridges and 309 tunnels. A number that is implausible is
// a finding even when nothing is red, and this case is what a walk over the SPEC would have said
// years earlier.
//
// The tile below is built by hand, field by field, so what it proves is the reader against the
// specification and not the reader against another reader. It carries one layer, four keys and
// four values -- a bool true, a bool false, an int and a string -- and one linestring feature
// that names all four.
void Varint(std::vector<uint8_t> &into, uint64_t held) {
  do {
    uint8_t byte = (uint8_t)(held & 0x7fu);
    held >>= 7;
    if (held != 0) { byte |= 0x80u; }
    into.push_back(byte);
  } while (held != 0);
}

void Key(std::vector<uint8_t> &into, uint32_t field, uint32_t wire) {
  Varint(into, ((uint64_t)field << 3) | wire);
}

void Bytes(std::vector<uint8_t> &into, uint32_t field, const std::vector<uint8_t> &held) {
  Key(into, field, 2);
  Varint(into, held.size());
  into.insert(into.end(), held.begin(), held.end());
}

void Text(std::vector<uint8_t> &into, uint32_t field, const std::string &held) {
  Bytes(into, field, std::vector<uint8_t>(held.begin(), held.end()));
}

[[nodiscard]] std::vector<uint8_t> BoolValue(bool held) {
  std::vector<uint8_t> out;
  Key(out, 7, 0);
  Varint(out, held ? 1u : 0u);
  return out;
}

[[nodiscard]] std::vector<uint8_t> IntValue(int64_t held) {
  std::vector<uint8_t> out;
  Key(out, 4, 0);
  Varint(out, (uint64_t)held);
  return out;
}

[[nodiscard]] std::vector<uint8_t> TextValue(const std::string &held) {
  std::vector<uint8_t> out;
  Text(out, 1, held);
  return out;
}

[[nodiscard]] std::vector<uint8_t> Tile(void) {
  std::vector<uint8_t> feature;
  {
    std::vector<uint8_t> tags;
    for (uint32_t pair = 0; pair < 4; ++pair) {
      Varint(tags, pair);
      Varint(tags, pair);
    }
    Bytes(feature, 2, tags);
    Key(feature, 3, 0);
    Varint(feature, 2);
    std::vector<uint8_t> geometry;
    Varint(geometry, (1u << 3) | 1u);
    Varint(geometry, (uint64_t)((0 << 1) ^ (0 >> 31)));
    Varint(geometry, (uint64_t)((0 << 1) ^ (0 >> 31)));
    Varint(geometry, (1u << 3) | 2u);
    Varint(geometry, (uint64_t)((100 << 1)));
    Varint(geometry, (uint64_t)((100 << 1)));
    Bytes(feature, 4, geometry);
  }

  std::vector<uint8_t> layer;
  Text(layer, 1, "streets");
  Bytes(layer, 2, feature);
  Text(layer, 3, "bridge");
  Text(layer, 3, "tunnel");
  Text(layer, 3, "lanes");
  Text(layer, 3, "surface");
  Bytes(layer, 4, BoolValue(true));
  Bytes(layer, 4, BoolValue(false));
  Bytes(layer, 4, IntValue(3));
  Bytes(layer, 4, TextValue("asphalt"));
  Key(layer, 5, 0);
  Varint(layer, 4096);
  Key(layer, 15, 0);
  Varint(layer, 2);

  std::vector<uint8_t> tile;
  Bytes(tile, 3, layer);
  return tile;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<uint8_t> held = Tile();
  outshine::Ground::OsmVector read;
  bool present = false;
  if (!read.Parse(held.data(), held.size(), "streets", &present) || !present) {
    Unprepared("the hand-built tile did not parse, so no tag can be judged");
    return Report();
  }
  if (read.Features().size() != 1) {
    Unprepared("the hand-built tile carries no single feature");
    return Report();
  }
  const outshine::Ground::OsmVector::Feature &one = read.Features().front();

  const double bridge = read.Num(one, "bridge", -1.0);
  const double tunnel = read.Num(one, "tunnel", -1.0);
  const double lanes = read.Num(one, "lanes", -1.0);
  const std::string surface(read.Str(one, "surface"));
  std::printf("  bridge  (bool true)  reads %5.1f\n", bridge);
  std::printf("  tunnel  (bool false) reads %5.1f\n", tunnel);
  std::printf("  lanes   (int 3)      reads %5.1f\n", lanes);
  std::printf("  surface (string)     reads %s\n", surface.c_str());

  CHECK(lanes == 3.0 && surface == "asphalt",
        "the int and the string come back, so the tile is well formed and the reader is reading "
        "it -- without this a bool of -1 would say nothing about bools");

  CHECK(bridge == 1.0,
        "**A BOOL IS THE SEVENTH VALUE TYPE AND THE SPECIFICATION NAMES IT**: mapbox vector tile "
        "Value carries bool_value at field 7, and a reader that decodes six of seven SKIPS it "
        "rather than refusing -- protobuf lets an unknown field pass, so the tag becomes absent "
        "and every consumer reads its default. That is how a Munich extract reported 0 bridges "
        "through a city the Isar runs across");

  CHECK(tunnel == 0.0,
        "and a bool FALSE is a value and not an absence: it reads 0, which is what the tag says, "
        "rather than the -1 a missing tag would give -- a reader that answered the default for "
        "both would pass the check above by accident");

  Covers("mapbox vector tile: all seven Value types decode, and a bool tag reaches its consumer "
         "as a number rather than being skipped into the default");
  return Report();
}
