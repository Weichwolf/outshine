#include "Check.h"
#include "OsmXmlReader.h"
#include "StructureBuildQueue.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <span>
#include <vector>

namespace {
using namespace outshine;

Ground::GeoBounds Inside(Data::TileId northWest, Data::TileId southEast) {
  const auto low = Ground::TileBounds(northWest), high = Ground::TileBounds(southEast);
  return {.MinLonDeg = low.MinLonDeg + (low.MaxLonDeg - low.MinLonDeg) * 0.2,
          .MinLatDeg = high.MinLatDeg + (high.MaxLatDeg - high.MinLatDeg) * 0.2,
          .MaxLonDeg = high.MaxLonDeg - (high.MaxLonDeg - high.MinLonDeg) * 0.2,
          .MaxLatDeg = low.MaxLatDeg - (low.MaxLatDeg - low.MinLatDeg) * 0.2};
}

std::shared_ptr<const outshine::Generators::Osm::SourceSnapshot>
Source(std::span<const Ground::GeoBounds> bounds) {
  std::ostringstream xml;
  xml << std::setprecision(17) << "<osm version='0.6'>";
  for (size_t at = 0; at < bounds.size(); ++at) {
    const auto &b = bounds[at];
    const std::array<LongitudeLatitude, 4> points{
        {{.LongitudeDeg = b.MinLonDeg, .LatitudeDeg = b.MinLatDeg},
         {.LongitudeDeg = b.MaxLonDeg, .LatitudeDeg = b.MinLatDeg},
         {.LongitudeDeg = b.MaxLonDeg, .LatitudeDeg = b.MaxLatDeg},
         {.LongitudeDeg = b.MinLonDeg, .LatitudeDeg = b.MaxLatDeg}}};
    for (size_t point = 0; point < points.size(); ++point) {
      xml << "<node id='" << 1 + at * 4 + point << "' lat='" << points[point].LatitudeDeg
          << "' lon='" << points[point].LongitudeDeg << "'/>";
    }
    xml << "<way id='" << 1000 + at << "'>";
    for (size_t point = 0; point < 5; ++point) {
      xml << "<nd ref='" << 1 + at * 4 + point % 4 << "'/>";
    }
    xml << "<tag k='building' v='yes'/></way>";
  }
  xml << "</osm>";
  auto elements = outshine::Generators::Osm::XmlReader::Read(
      xml.str(), {.DatasetId = "geometry", .Revision = "one"});
  if (!elements) { return nullptr; }
  return std::make_shared<const outshine::Generators::Osm::SourceSnapshot>(
      outshine::Generators::Osm::SourceSnapshot{
          .Elements = std::move(*elements),
          .Coverage = {{.WestDeg = -180, .SouthDeg = -80, .EastDeg = 180, .NorthDeg = 80}}});
}

std::expected<bool, std::string>
Prepare(StructureBuildQueue &queue,
        std::shared_ptr<const outshine::Generators::Osm::SourceSnapshot> source,
        int zoom) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    auto result = queue.PrepareOriginal(
        source,
        {.Heights = {.StoreyHeightM = 3, .BodyHeightM = 9}, .PointWidthM = 2, .PointsMost = 1024},
        zoom);
    if (!result || *result) { return result; }
    (void)queue.AwaitSlice(0.05);
  }
  return std::unexpected("original geometry preparation timed out");
}
}

int main() {
  using namespace outshine::Test;
  constexpr int zoom = 14;
  const Data::TileId first{.Zoom = zoom, .X = 8590, .Y = 5600};
  const Data::TileId distant{.Zoom = zoom, .X = 8610, .Y = 5618};
  const std::array sparse{Inside(first, first), Inside(first, first), Inside(distant, distant)};
  const auto source = Source(sparse);
  CHECK(source, "sparse original buildings parse inside a large declared source region");
  if (!source) { return Report(); }
  Tasks tasks(1);
  StructureBuildQueue queue;
  queue.Opens(&tasks, nullptr);
  const auto prepared = Prepare(queue, source, zoom);
  CHECK(prepared && *prepared, "actual native preparation admits sparse geometry at full zoom");
  const auto tiles = queue.OriginalHeightTiles(zoom);
  const std::array expected{first, distant};
  CHECK(tiles && std::ranges::equal(*tiles, expected),
        "overlapping footprints share fields and empty space between buildings requests none");
  if (!tiles) { return Report(); }
  const auto *resident = tiles->data();
  CHECK(Prepare(queue, source, zoom) && queue.OriginalHeightTiles(zoom)->data() == resident,
        "unchanged original demand retains the prepared field storage");
  CHECK(!queue.OriginalHeightTiles(zoom + 1), "unprepared zoom cannot reuse another field demand");
  const auto finer = Prepare(queue, source, zoom + 1);
  CHECK(finer && *finer && queue.OriginalHeightTiles(zoom + 1)->size() == 8 &&
            !queue.OriginalHeightTiles(zoom),
        "changing zoom regenerates the complete demand on the worker");
  const Data::TileId end{.Zoom = zoom, .X = first.X + 2, .Y = first.Y + 2};
  const std::array large{Inside(first, end)};
  const auto largeReady = Prepare(queue, Source(large), zoom);
  const auto largeTiles = queue.OriginalHeightTiles(zoom);
  CHECK(largeReady && *largeReady && largeTiles && largeTiles->size() == 9 &&
            std::ranges::find(*largeTiles,
                              Data::TileId{.Zoom = zoom, .X = first.X + 1, .Y = first.Y + 1}) !=
                largeTiles->end(),
        "wide footprints require interior terrain fields as well as their vertex fields");
  auto seam = Inside(first, first);
  seam.MinLonDeg = 179.999;
  seam.MaxLonDeg = -179.999;
  const std::array wrapped{seam};
  const auto wrappedReady = Prepare(queue, Source(wrapped), zoom);
  const auto wrappedTiles = queue.OriginalHeightTiles(zoom);
  const std::array seamExpected{Data::TileId{.Zoom = zoom, .X = 0, .Y = first.Y},
                                Data::TileId{.Zoom = zoom, .X = (1u << zoom) - 1, .Y = first.Y}};
  CHECK(wrappedReady && *wrappedReady && wrappedTiles &&
            std::ranges::equal(*wrappedTiles, seamExpected),
        "dateline footprints request both adjacent world-edge fields without a global rectangle");
  std::vector<Ground::GeoBounds> overBudget;
  for (uint32_t at = 0; at < 65; ++at) {
    const Data::TileId tile{.Zoom = zoom, .X = first.X + at * 2, .Y = first.Y};
    overBudget.push_back(Inside(tile, tile));
  }
  CHECK(!Prepare(queue, Source(overBudget), zoom) && queue.OriginalHeightTiles(zoom) &&
            std::ranges::equal(*queue.OriginalHeightTiles(zoom), seamExpected),
        "exceeding the existing field budget rejects replacement and preserves the prior demand");
  const auto empty = Prepare(queue, Source({}), zoom);
  CHECK(empty && *empty && queue.OriginalHeightTiles(zoom)->empty(),
        "empty original regions request no terrain fields");
  CHECK(queue.PrepareOriginal(nullptr, {}, zoom) && !queue.HasOriginal() &&
            queue.OriginalHeightTiles(zoom)->empty(),
        "source removal releases the prepared field demand");
  return Report();
}
