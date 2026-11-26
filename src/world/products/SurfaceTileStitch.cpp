#include "SurfaceTileStitch.h"

#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine {
namespace {

enum class Edge { North, South, West, East };

struct EdgeDirection {
  Edge Here;
  Edge There;
  int64_t X;
  int64_t Y;
};

constexpr std::array kEdges{
    EdgeDirection{.Here = Edge::North, .There = Edge::South, .X = 0, .Y = -1},
    EdgeDirection{.Here = Edge::South, .There = Edge::North, .X = 0, .Y = 1},
    EdgeDirection{.Here = Edge::West, .There = Edge::East, .X = -1, .Y = 0},
    EdgeDirection{.Here = Edge::East, .There = Edge::West, .X = 1, .Y = 0}};

uint64_t Key(Data::TileId tile) {
  const auto level = static_cast<unsigned>(tile.Zoom);
  return (uint64_t{1} << (level * 2)) | (static_cast<uint64_t>(tile.X) << level) | tile.Y;
}

size_t EdgeIndex(Edge edge, size_t side, size_t sample) {
  const size_t last = side - 1;
  switch (edge) {
    case Edge::North: return sample;
    case Edge::South: return last * side + sample;
    case Edge::West: return sample * side;
    case Edge::East: return sample * side + last;
  }
  std::unreachable();
}

using TileIndex = std::unordered_map<uint64_t, SurfaceTile *>;

const SurfaceTile *
CoarserNeighbour(const TileIndex &tiles, Data::TileId fine, const EdgeDirection &edge) {
  const auto width = static_cast<int64_t>(uint64_t{1} << static_cast<unsigned>(fine.Zoom));
  const int64_t y = fine.Y + edge.Y;
  if (y < 0 || y >= width) { return nullptr; }
  const int64_t x = (fine.X + edge.X + width) % width;
  Data::TileId at{.Zoom = fine.Zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
  if (tiles.contains(Key(at))) { return nullptr; }
  while (at.Zoom > 0) {
    --at.Zoom;
    at.X >>= 1;
    at.Y >>= 1;
    const auto gap = static_cast<unsigned>(fine.Zoom - at.Zoom);
    if ((fine.X >> gap) == at.X && (fine.Y >> gap) == at.Y) { return nullptr; }
    if (const auto found = tiles.find(Key(at)); found != tiles.end()) { return found->second; }
  }
  return nullptr;
}

Vec3 Interpolate(const Vec3f &first, const Vec3f &next, double fraction) {
  Vec3 result;
  for (size_t at = 0; at < 3; ++at) {
    result[at] =
        static_cast<double>(first[at]) + (static_cast<double>(next[at]) - first[at]) * fraction;
  }
  return result;
}

EastNorthUp ToEnu(const Vec3 &eus) {
  return {.EastM = eus[0], .NorthM = -eus[2], .UpM = eus[1]};
}

Vec3f ToEus(EastNorthUp enu) {
  return {{static_cast<float>(enu.EastM),
           static_cast<float>(enu.UpM),
           static_cast<float>(-enu.NorthM)}};
}

void StitchEdge(SurfaceTile &fine, const SurfaceTile &coarse, const EdgeDirection &edge) {
  const auto from = TangentFrame::At(coarse.Origin);
  const auto into = TangentFrame::At(fine.Origin);
  const auto gap = static_cast<unsigned>(fine.Coverage.Zoom - coarse.Coverage.Zoom);
  const uint32_t scale = uint32_t{1} << gap;
  const bool horizontal = edge.Here == Edge::North || edge.Here == Edge::South;
  const auto cell = (horizontal ? fine.Coverage.X : fine.Coverage.Y) % scale;
  const size_t last = coarse.GridSide - 1;
  const bool stitchColour = fine.Colours.size() == fine.Vertices.size() &&
                            coarse.Colours.size() == coarse.Vertices.size();
  for (size_t sample = 0; sample < fine.GridSide; ++sample) {
    const double along =
        (static_cast<double>(cell) + static_cast<double>(sample) / (fine.GridSide - 1)) / scale *
        static_cast<double>(last);
    const auto first = static_cast<size_t>(along);
    const size_t aIndex = EdgeIndex(edge.There, coarse.GridSide, first);
    const size_t bIndex = EdgeIndex(edge.There, coarse.GridSide, std::min(first + 1, last));
    const auto &a = coarse.Vertices[aIndex];
    const auto &b = coarse.Vertices[bIndex];
    const size_t fineIndex = EdgeIndex(edge.Here, fine.GridSide, sample);
    auto &vertex = fine.Vertices[fineIndex];
    const double fraction = along - static_cast<double>(first);
    const auto position = Interpolate(a.PositionM, b.PositionM, fraction);
    vertex.PositionM = ToEus(into.ToLocalPosition(from.ToEcefPosition(ToEnu(position))));
    auto normal = ToEus(into.ToLocalDirection(
        from.ToEcefDirection(ToEnu(Interpolate(a.Normal, b.Normal, fraction)))));
    if (Normalise(normal)) { vertex.Normal = normal; }
    if (stitchColour) {
      auto &colour = fine.Colours[fineIndex];
      for (size_t channel = 0; channel < colour.size(); ++channel) {
        const auto value =
            std::lerp(coarse.Colours[aIndex][channel], coarse.Colours[bIndex][channel], fraction);
        colour[channel] = static_cast<uint8_t>(std::lround(value));
      }
    }
  }
}

}

void StitchSurfaceTiles(std::span<SurfaceTile> tiles) {
  TileIndex index;
  std::vector<SurfaceTile *> order;
  for (auto &tile : tiles) {
    if (tile.GridSide == 0) { continue; }
    order.push_back(&tile);
    index.emplace(Key(tile.Coverage), &tile);
  }
  std::ranges::sort(order, {}, [](const SurfaceTile *tile) { return Key(tile->Coverage); });
  for (auto *tile : order) {
    for (const auto &edge : kEdges) {
      if (const auto *neighbour = CoarserNeighbour(index, tile->Coverage, edge);
          neighbour != nullptr) {
        StitchEdge(*tile, *neighbour, edge);
      }
    }
  }
}

}
