#include "MvtLayer.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "MvtWireFixture.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
using Mvt::Bytes;

void Append(Bytes &out, uint8_t tag, std::span<const uint8_t> bytes) {
  out.push_back(tag);
  size_t length = bytes.size();
  while (length >= 128) {
    out.push_back(static_cast<uint8_t>((length & 127u) | 128u));
    length >>= 7u;
  }
  out.push_back(static_cast<uint8_t>(length));
  out.insert(out.end(), bytes.begin(), bytes.end());
}

struct Input {
  size_t TextLength;
  size_t Features;
};

Bytes Tile(Input input) {
  const auto [textLength, features] = input;
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  const Bytes text(textLength, 'k');
  Append(layer, 0x1a, text);
  Bytes value;
  Append(value, 0x0a, Bytes(textLength + 1, 'v'));
  Append(value, 0x0a, text);
  Append(layer, 0x22, value);
  for (size_t at = 0; at < features; ++at) {
    Append(layer, 0x12, Bytes{0x18, 2, 0x12, 2, 0, 0, 0x22, 6, 9, 2, 0, 10, 2, 0});
  }
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}

void CheckStorage(Input input) {
  const auto [textLength, features] = input;
  const auto bytes = Tile(input);
  BeginCppAllocationProbe();
  const auto plan = MvtLayer::Plan(bytes, "x");
  const auto scan = EndCppAllocationProbe();
  CHECK(plan && scan.Calls == 0 && scan.Bytes == 0,
        "the structural plan borrows tables and counts words without allocating storage");
  if (!plan) { return; }
  CHECK(plan->Features == features && plan->Keys == 1 && plan->Values == 1 &&
            plan->Tags == features * 2 && plan->TextBytes == textLength * 2,
        "selected tables and feature counts preserve repeated-string last-value semantics");
  CHECK(plan->ScratchTags == 2 && plan->ScratchGeometry == 6,
        "scratch capacity follows the largest actual feature words rather than all features");
  MvtLayer layer;
  BeginCppAllocationProbe();
  const auto refused = layer.Parse(bytes, "x", plan->PeakBytesMost - 1);
  const auto refusal = EndCppAllocationProbe();
  CHECK(!refused && refused.error() == MvtLayer::ParseError::CapacityExceeded &&
            refusal.Calls == 0 && refusal.Bytes == 0,
        "actual Parse refuses insufficient admission before any product allocation");
  BeginCppAllocationProbe();
  const auto accepted = layer.Parse(bytes, "x", plan->PeakBytesMost);
  const auto allocation = EndCppAllocationProbe();
  CHECK(accepted && allocation.Bytes <= plan->PeakBytesMost && allocation.Calls > 0,
        "all fresh decoder allocation requests fit the admitted structural plan");
  CHECK(layer.HeapBytes() <= plan->HeapBytesMost && layer.Features().size() == features &&
            layer.Points().size() == features * 4 && layer.Rings().size() == features,
        "the decoded native arrays fit the planned counts and retained heap bound");
  if (layer.Features().empty()) { return; }
  CHECK(layer.TagAt(layer.Features().front(), 0).String == std::string(textLength, 'k'),
        "allocation planning preserves the last occurrence of a repeated string value");
  const auto before = layer.HeapBytes();
  CHECK(!layer.Parse(bytes, "x", 0) && layer.HeapBytes() == before &&
            layer.Features().size() == features,
        "refusal leaves the previously published layer intact");
}

void BadInputAndUnselectedLayers() {
  const auto tile = Tile({.TextLength = 128, .Features = 2});
  for (size_t end = 0; end < tile.size(); ++end) {
    BeginCppAllocationProbe();
    const auto plan = MvtLayer::Plan(std::span(tile).first(end), "x");
    const auto allocations = EndCppAllocationProbe();
    CHECK(!plan && allocations.Calls == 0,
          "truncated envelopes are rejected without allocating intermediate arrays");
  }
  BeginCppAllocationProbe();
  const auto absent = MvtLayer::Plan(tile, "absent");
  const auto allocations = EndCppAllocationProbe();
  CHECK(!absent && absent.error() == MvtLayer::ParseError::MissingLayer && allocations.Calls == 0,
        "unselected payload contributes no arrays or copied strings");
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  Append(layer, 0x12, Bytes{0x18, 1, 0x22, 5, 0xf9, 0xff, 0xff, 0xff, 0x0f});
  Bytes invalidGeometry;
  Append(invalidGeometry, 0x1a, layer);
  const auto plan = MvtLayer::Plan(invalidGeometry, "x");
  MvtLayer decoded;
  CHECK(plan && plan->Points == 0 && plan->Rings == 0 &&
            !decoded.Parse(invalidGeometry, "x", plan->PeakBytesMost),
        "an enormous encoded command count sizes no arrays and fails geometry validation");
}
}

int main() {
  for (const size_t length : std::array<size_t, 9>{0, 15, 22, 23, 31, 32, 127, 128, 4096}) {
    CheckStorage({.TextLength = length, .Features = 3});
  }
  CheckStorage({.TextLength = 64, .Features = 2048});
  BadInputAndUnselectedLayers();
  return Report();
}
