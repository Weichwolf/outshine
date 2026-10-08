#include "Check.h"
#include "HeightSheets.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace {
using namespace outshine;

constexpr int kSide = 33;
constexpr int kPageSide = kSide + 2;

size_t Node(int i, int j) {
  return static_cast<size_t>((j + 1) * kPageSide + i + 1);
}

Sheet Page(Data::TileId tile, int sourceZoom, float height) {
  return {.Tile = tile,
          .Nodes = std::vector<float>(kPageSide * kPageSide, height),
          .Side = kSide,
          .Postings = kSide,
          .Virtual = true,
          .SourceZoom = sourceZoom};
}

const Sheet &Find(const Patchwork &patchwork, Data::TileId tile) {
  return *std::ranges::find(patchwork.Sheets, tile, &Sheet::Tile);
}

void SharedCorner(const std::array<Sheet, 4> &pages, float expected) {
  using namespace outshine::Test;
  std::array<size_t, 4> order{0, 1, 2, 3};
  HeightSheets sheets;
  sheets.Framed(TangentFrame::At({}));
  std::string error;
  do {
    Patchwork patchwork;
    for (const size_t i : order) { patchwork.Sheets.push_back(pages[i]); }
    CHECK(sheets.Stitch(patchwork, error), "a corner can be stitched in every input order");
    for (size_t i = 0; i < pages.size(); ++i) {
      const Sheet &page = Find(patchwork, pages[i].Tile);
      const int x = i % 2 == 0 ? 32 : 0;
      const int y = i < 2 ? 32 : 0;
      CHECK(page.Nodes[Node(x, y)] == expected,
            "all four pages share one physical corner, including a diagonal coarse neighbor");
      CHECK(page.Nodes[Node(16, 16)] == pages[i].Nodes[Node(16, 16)],
            "seam repair leaves interior terrain unchanged");
    }
    const auto once = patchwork.Sheets;
    CHECK(sheets.Stitch(patchwork, error), "stitching remains repeatable");
    for (size_t i = 0; i < once.size(); ++i) {
      CHECK(once[i].Nodes == patchwork.Sheets[i].Nodes,
            "a second stitch does not change the surface");
    }
  } while (std::next_permutation(order.begin(), order.end()));
}
}

int main() {
  using namespace outshine::Test;
  HeightSheets sheets;
  sheets.Framed(TangentFrame::At({}));
  std::string error;
  for (const bool dateline : {false, true}) {
    const Data::TileId west{.Zoom = 20, .X = dateline ? (1u << 20) - 1 : 552253, .Y = 367807};
    const Data::TileId east{.Zoom = 20, .X = dateline ? 0u : 552254, .Y = 367807};
    for (const bool reverse : {false, true}) {
      Patchwork patchwork{.Sheets = {Page(west, 16, 573.9629f), Page(east, 17, 575.6133f)}};
      if (reverse) { std::ranges::reverse(patchwork.Sheets); }
      CHECK(sheets.Stitch(patchwork, error), "unequal source resolutions remain stitchable");
      for (int j = 0; j < kSide; ++j) {
        CHECK(Find(patchwork, west).Nodes[Node(32, j)] == 575.6133f &&
                  Find(patchwork, east).Nodes[Node(0, j)] == 575.6133f,
              "equal mesh LODs use the finer DEM source along the entire shared boundary");
      }
    }
  }
  const std::array<Sheet, 4> peers{Page({.Zoom = 5, .X = 3, .Y = 3}, 3, 100),
                                   Page({.Zoom = 5, .X = 4, .Y = 3}, 4, 120),
                                   Page({.Zoom = 5, .X = 3, .Y = 4}, 4, 130),
                                   Page({.Zoom = 5, .X = 4, .Y = 4}, 5, 160)};
  SharedCorner(peers, 160);
  auto mixed = peers;
  mixed[0] = Page({.Zoom = 4, .X = 1, .Y = 1}, 4, 200);
  for (int j = -1; j <= kSide; ++j) {
    for (int i = -1; i <= kSide; ++i) {
      mixed[0].Nodes[Node(i, j)] = static_cast<float>(200 + 2 * i + 3 * j);
    }
  }
  SharedCorner(mixed, 360);
  return Report();
}
