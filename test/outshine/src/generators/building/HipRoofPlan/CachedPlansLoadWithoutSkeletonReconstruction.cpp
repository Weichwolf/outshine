#include "Check.h"
#include "HipRoofCodec.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<EastNorth, 6> ring{{{0, 0}, {10, 0}, {10, 4}, {4, 4}, {4, 10}, {0, 10}}};
  const auto made = HipRoofPlan::Prepare(ring, {}, {});
  CHECK(made.has_value(), "concave plan is generated once before caching");
  if (!made) { return Report(); }
  HipRoofCodec::Product source =
      std::make_shared<const std::expected<HipRoofPlan, HipRoofError>>(*made);
  StructureBinary::Writer writer;
  CHECK(HipRoofCodec::Fields(writer, source), "complete prepared facets are serialized");
  StructureBinary::Reader reader(writer.Bytes);
  HipRoofCodec::Product restored;
  CHECK(HipRoofCodec::Fields(reader, restored), "decoder restores native plan directly from bytes");
  CHECK(reader.Remaining == 0 && restored && *restored, "entire ready roof payload is consumed");
  if (restored && *restored) {
    CHECK(HipRoofCodec::BoundTo(restored, ring, {}), "a cached plan owns every original eave");
    auto subdivided = std::vector<EastNorth>(ring.begin(), ring.end());
    subdivided.insert(subdivided.begin() + 1, EastNorth{5, 0});
    CHECK(!HipRoofCodec::BoundTo(restored, subdivided, {}),
          "unchanged footprint coverage cannot hide a missing source-eave owner");
    CHECK((**restored).Facets().size() == made->Facets().size(),
          "cache preserves source facet count");
    for (size_t at = 0; at < made->Facets().size(); ++at) {
      const auto &before = made->Facets()[at];
      const auto &after = (**restored).Facets()[at];
      CHECK(before.Ring == after.Ring && before.Gradient == after.Gradient &&
                before.InterceptM == after.InterceptM && before.SourceRing == after.SourceRing &&
                before.SourceEdge == after.SourceEdge,
            "cache preserves coordinates, planes and source-edge ownership exactly");
    }
    for (int x = 0; x < 20; ++x) {
      for (int y = 0; y < 20; ++y) {
        const EastNorth p{.EastM = x * 0.5 + 0.17, .NorthM = y * 0.5 + 0.23};
        CHECK(made->DistanceAt(p) == (**restored).DistanceAt(p),
              "cache-hit height queries reproduce prepared values exactly");
      }
    }
  }
  for (const HipRoofCodec::Product &state :
       {HipRoofCodec::Product{},
        std::make_shared<const std::expected<HipRoofPlan, HipRoofError>>(
            std::unexpected(HipRoofError::CapacityExceeded))}) {
    StructureBinary::Writer encoded;
    CHECK(HipRoofCodec::Fields(encoded, state), "unset/refused states serialize explicitly");
    StructureBinary::Reader input(encoded.Bytes);
    HipRoofCodec::Product output;
    CHECK(HipRoofCodec::Fields(input, output), "unset/refused states decode without generation");
    CHECK(!state ? !output : output && !*output && output->error() == state->error(),
          "cache preserves local refusal rather than retrying an unbuildable skeleton");
  }
  StructureBinary::Reader truncated(std::span(writer.Bytes).first(writer.Bytes.size() - 1));
  HipRoofCodec::Product incomplete;
  CHECK(!HipRoofCodec::Fields(truncated, incomplete),
        "truncated roof payload cannot publish a ready plan");
  StructureBinary::Reader bounded(writer.Bytes);
  bounded.AllocationLeft = 0;
  CHECK(!HipRoofCodec::Fields(bounded, incomplete),
        "decoded plan honors resident allocation budget before allocation");
  return Report();
}
