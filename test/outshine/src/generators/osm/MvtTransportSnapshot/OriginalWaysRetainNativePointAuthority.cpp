#include "MvtTransportSourceFactsInternal.h"
#include "MvtTransportWireFixture.h"
#include "OsmField.h"
#include "StreetField.h"
#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include "Wayfinding.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stop_token>
#include <string>
#include <tuple>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
namespace Wire = outshine::Test::MvtTransportWire;
using WayState = MvtTransportOriginalWayState;
using PointState = MvtTransportOriginalPointState;

void CheckAuthority(const OsmField &field, const StreetField &streets, int scenario) {
  auto rows = MvtTransport::PrepareSources(field, streets, {});
  CHECK(rows.has_value(), "declared source rows prepare independently of native authority");
  if (!rows) { return; }
  CHECK(MvtTransport::RecordMissingTopology(*rows, {}).has_value(),
        "unpublished ways retain the existing explicit omission contract");
  std::ranges::sort(rows->Omissions, {}, [](const auto &row) {
    return std::tie(row.Source, row.Feature, row.Fragment, row.Reason);
  });
  auto network = Path::Network::Create({.CellM = 1}, {.RadiusM = kWgs84A});
  CHECK(network.has_value(), "the actual native graph owns the candidate rings");
  if (!network) { return; }
  const auto &way = streets.Ways().front();
  const auto ring = field.Points().subspan(static_cast<size_t>(way.FirstPoint) * 2,
                                           static_cast<size_t>(way.PointCount) * 2);
  const auto lay = [&](std::span<const double> points, size_t tag) {
    CHECK(network->Lay(points, {.HalfWidthM = way.HalfWidthM, .Tag = tag}),
          "native candidate retains the supplied full ring and explicit source tag");
  };
  if (scenario == 0) { lay(ring, 99); }
  if (scenario == 1 || scenario == 2) {
    std::vector<double> changed(ring.begin(), ring.end());
    changed[2] = std::nextafter(changed[2], std::numeric_limits<double>::infinity());
    lay(changed, 0);
  }
  if (scenario == 2 || scenario == 3) { lay(ring, 0); }
  if (scenario == 3) { lay(ring, 0); }
  std::string error;
  CHECK(network->Weave(error), "the native candidate graph weaves before point lookup");
  std::vector<uint64_t> mapped(network->NodeCount(), 0);
  if (scenario == 2) {
    const auto node = network->NodeOfWayPoint(0, 0);
    CHECK(node && *node < mapped.size(), "the first original point has an actual native node");
    if (node && *node < mapped.size()) { mapped[*node] = 1; }
  }
  const MvtTransport::SourceFactInput input{
      .SourceRows = *rows, .Segments = {}, .Network = *network, .Field = field, .Streets = streets};
  MvtTransportSourceFacts facts;
  CHECK(MvtTransport::FillMissingSourceWays(facts, input, mapped, {}).has_value(),
        "the production collector captures the declared original-way authority");
  const auto found = std::ranges::find_if(
      facts.MissingWays, [](const auto &missing) { return missing.Way.StreetWay == 0; });
  CHECK(found != facts.MissingWays.end(), "the first omitted source retains a separate record");
  if (found == facts.MissingWays.end()) { return; }
  constexpr std::array<WayState, 4> expected{WayState::AbsentInCapturedNetwork,
                                             WayState::InexactTagCandidate,
                                             WayState::Exact,
                                             WayState::AmbiguousExact};
  CHECK(
      found->OriginalWayState == expected[static_cast<size_t>(scenario)] &&
          found->OriginalPoints.size() == way.PointCount,
      "full-ring identity distinguishes absent, one-ULP mismatch, exact and ambiguous candidates");
  size_t published = 0;
  size_t unrepresented = 0;
  for (const auto &point : found->OriginalPoints) {
    if (scenario != 2) {
      CHECK(point.State == PointState::Unresolved &&
                point.Group == std::numeric_limits<uint32_t>::max(),
            "nonexact authority leaves every original point explicitly unresolved");
      continue;
    }
    published += static_cast<size_t>(point.State == PointState::PublishedGroup && point.Group == 0);
    unrepresented +=
        static_cast<size_t>(point.State == PointState::ValidNativeNodeWithoutPublishedGroup &&
                            point.Group == std::numeric_limits<uint32_t>::max());
  }
  if (scenario == 2) {
    CHECK(published > 0 && unrepresented > 0 && published + unrepresented == way.PointCount,
          "group zero is valid while other actual native points lack a published representative");
  }
  std::stop_source stopped;
  stopped.request_stop();
  MvtTransportSourceFacts cancelled;
  CHECK(!MvtTransport::FillMissingSourceWays(cancelled, input, mapped, stopped.get_token()),
        "cancellation never publishes incomplete original point facts");
}
}

int main() {
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            recipes.Load("data/world/vegetation.json", materials),
        "the actual source recipes load");
  if (!recipes.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(16, layers);
  CHECK(field.Accept(Wire::kTileX, Wire::kTileX, Wire::Tile()).has_value(),
        "the unchanged declared MVT input loads");
  StreetField streets;
  (void)streets.Ingest(field, recipes);
  CHECK(streets.Ways().size() >= 2 && streets.Ways().front().PointCount >= 3,
        "the fixture supplies a full ring with an interior point");
  if (streets.Ways().size() < 2 || streets.Ways().front().PointCount < 3) { return Report(); }
  for (const int scenario : {0, 1, 2, 3}) { CheckAuthority(field, streets, scenario); }
  return Report();
}
