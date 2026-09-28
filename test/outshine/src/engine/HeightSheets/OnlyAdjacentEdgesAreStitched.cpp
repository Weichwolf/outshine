#include "Check.h"
#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {
using namespace outshine;

constexpr int kSide = 33;
constexpr int kPageSide = kSide + 2;

size_t Node(int i, int j) {
  return static_cast<size_t>((j + 1) * kPageSide + i + 1);
}

Sheet Page(Data::TileId tile, bool plane = false) {
  Sheet page{.Tile = tile,
             .Nodes = std::vector<float>(kPageSide * kPageSide, 100.0f),
             .Side = kSide,
             .Postings = kSide,
             .Virtual = true,
             .SourceZoom = tile.Zoom};
  if (plane) {
    for (int j = -1; j <= kSide; ++j) {
      for (int i = -1; i <= kSide; ++i) {
        page.Nodes[Node(i, j)] = static_cast<float>(200 + 2 * i + 3 * j);
      }
    }
  }
  return page;
}

const Sheet &Find(const Patchwork &patchwork, Data::TileId tile) {
  return *std::ranges::find(patchwork.Sheets, tile, &Sheet::Tile);
}

struct Edge {
  int X;
  int Y;
};

constexpr std::array<Edge, 4> kEdges{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};

Data::TileId FineAt(Edge edge, uint32_t scale) {
  return {.Zoom = 6,
          .X = edge.X < 0   ? 2 * scale
               : edge.X > 0 ? 3 * scale - 1
                            : 2 * scale + 1,
          .Y = edge.Y < 0   ? 2 * scale
               : edge.Y > 0 ? 3 * scale - 1
                            : 2 * scale + 1};
}

Data::TileId CoarseAt(Edge edge, int drop) {
  return {.Zoom = 6 - drop,
          .X = static_cast<uint32_t>(2 + edge.X),
          .Y = static_cast<uint32_t>(2 + edge.Y)};
}

bool Constant(const Sheet &page) {
  return std::ranges::all_of(page.Nodes, [](float height) { return height == 100.0f; });
}

bool ExpectedEdge(const Sheet &page, Data::TileId coarse, Edge edge, uint32_t scale) {
  for (int j = -1; j <= kSide; ++j) {
    for (int i = -1; i <= kSide; ++i) {
      const bool inside = i >= 0 && i < kSide && j >= 0 && j < kSide;
      const bool seam = inside && ((edge.X < 0 && i == 0) || (edge.X > 0 && i == kSide - 1) ||
                                   (edge.Y < 0 && j == 0) || (edge.Y > 0 && j == kSide - 1));
      double expected = 100.0;
      if (seam) {
        const double globalX = static_cast<double>(page.Tile.X) + static_cast<double>(i) / 32;
        const double globalY = static_cast<double>(page.Tile.Y) + static_cast<double>(j) / 32;
        const double coarseI = (globalX / scale - coarse.X) * 32;
        const double coarseJ = (globalY / scale - coarse.Y) * 32;
        expected = 200 + 2 * coarseI + 3 * coarseJ;
      }
      if (page.Nodes[Node(i, j)] != static_cast<float>(expected)) { return false; }
    }
  }
  return true;
}
}

int main() {
  using namespace outshine::Test;
  std::string error;
  HeightSheets sheets;
  sheets.Framed(TangentFrame::At({}));

  const Data::TileId inner{.Zoom = 6, .X = 19, .Y = 21};
  for (const bool reverse : {false, true}) {
    Patchwork patchwork{.Sheets = {Page({.Zoom = 3, .X = 2, .Y = 2}, true), Page(inner)}};
    if (reverse) { std::ranges::reverse(patchwork.Sheets); }
    CHECK(sheets.Stitch(patchwork, error) && Constant(Find(patchwork, inner)) &&
              sheets.Seams().Virtual.Edges == 0,
          "a covering ancestor cannot drag internal fine boundaries to its remote exterior");
  }

  for (const Edge edge : kEdges) {
    for (const int drop : {1, 2, 4}) {
      const uint32_t scale = 1u << static_cast<uint32_t>(drop);
      const Data::TileId fine = FineAt(edge, scale);
      const Data::TileId coarse = CoarseAt(edge, drop);
      for (const bool reverse : {false, true}) {
        Patchwork patchwork{.Sheets = {Page(coarse, true), Page(fine)}};
        if (reverse) { std::ranges::reverse(patchwork.Sheets); }
        CHECK(sheets.Stitch(patchwork, error) &&
                  ExpectedEdge(Find(patchwork, fine), coarse, edge, scale) &&
                  sheets.Seams().Virtual.Edges == 1,
              "a true coarse neighbor transfers its analytic plane only along the shared edge");

        const Data::TileId peer{.Zoom = fine.Zoom,
                                .X = static_cast<uint32_t>(static_cast<int>(fine.X) + edge.X),
                                .Y = static_cast<uint32_t>(static_cast<int>(fine.Y) + edge.Y)};
        Patchwork covered{.Sheets = {Page(coarse, true), Page(fine), Page(peer)}};
        if (reverse) { std::ranges::reverse(covered.Sheets); }
        CHECK(sheets.Stitch(covered, error) && Constant(Find(covered, fine)),
              "an equal-resolution neighbor takes precedence over an overlapping coarse page");
      }
    }
  }

  for (const bool west : {false, true}) {
    const Data::TileId fine{.Zoom = 6, .X = west ? 0u : 63u, .Y = 19};
    const Data::TileId coarse{.Zoom = 4, .X = west ? 15u : 0u, .Y = 4};
    Patchwork patchwork{.Sheets = {Page(fine), Page(coarse, true)}};
    CHECK(sheets.Stitch(patchwork, error), "a dateline neighbor remains stitchable");
    bool correct = true;
    for (int k = 0; k < kSide; ++k) {
      const double along = (3.0 * 32 + k) / 4;
      const double expected = 200 + (west ? 64 : 0) + 3 * along;
      correct = correct && Find(patchwork, fine).Nodes[Node(west ? 0 : 32, k)] == expected;
    }
    CHECK(correct && sheets.Seams().Virtual.Edges == 1,
          "longitude wrapping uses the opposite physical boundary and the correct subinterval");
  }

  for (const bool north : {false, true}) {
    const Data::TileId fine{.Zoom = 6, .X = 19, .Y = north ? 0u : 63u};
    const Data::TileId opposite{.Zoom = 4, .X = 4, .Y = north ? 15u : 0u};
    Patchwork patchwork{.Sheets = {Page(fine), Page(opposite, true)}};
    CHECK(sheets.Stitch(patchwork, error) && Constant(Find(patchwork, fine)),
          "a polar boundary never wraps to terrain at the opposite pole");
  }
  return Report();
}
