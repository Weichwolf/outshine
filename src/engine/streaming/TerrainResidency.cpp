#include "TerrainResidency.h"

#include "ChunkSurface.h"
#include "Digest.h"
#include "Geodesy.h"
#include "GroundLattice.h"
#include "Heap.h"
#include "SceneRenderer.h"
#include "TerrainGrid.h"
#include "TileGeodesy.h"
#include "math/RenderFrame.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <ratio>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine {

namespace Says {
constexpr auto HeightPageIndexAllocationFailed = "height page index allocation failed";
constexpr auto HeightPageIndexCapacityExceeded = "height page index capacity exceeded";
}

namespace {

constexpr uint64_t kTileHashMix = 0x9E3779B185EBCA87ULL;

[[nodiscard]] Vec3 EcefOf(double lonDeg, double latDeg) {
  const Ground::Ecef at =
      Ground::GeoToEcefWgs84({.LongitudeDeg = lonDeg, .LatitudeDeg = latDeg, .HeightM = 0.0});
  return {{at.X, at.Y, at.Z}};
}

[[nodiscard]] double Dot(const Vec3 &a, const Vec3 &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

[[nodiscard]] float Dot2(std::array<float, 2> value) {
  return value[0] * value[0] + value[1] * value[1];
}

[[nodiscard]] double FractionOf(int k, uint32_t postings, int side) {
  return static_cast<double>(Ground::ChunkNodePosting(k, postings, side)) /
         static_cast<double>(postings - 1u);
}

struct SheetHash {
  [[nodiscard]] constexpr uint64_t operator()(const Data::TileId &tile) const noexcept {
    uint64_t hash = static_cast<uint32_t>(tile.Zoom);
    hash = (hash ^ tile.X) * kTileHashMix;
    return (hash ^ tile.Y) * kTileHashMix;
  }
};

using SheetIndex = FlatMap<size_t, Data::TileId, SheetHash>;

[[nodiscard]] std::expected<SheetIndex, std::string> IndexSheets(const Patchwork &patchwork) {
  SheetIndex wanted;
  for (size_t at = 0; at < patchwork.Sheets.size(); ++at) {
    const Data::TileId tile = patchwork.Sheets[at].Tile;
    const auto added = wanted.Emplace(tile, at);
    if (!added) {
      return std::unexpected(added.error() == FlatMapError::AllocationFailed
                                 ? Says::HeightPageIndexAllocationFailed
                                 : Says::HeightPageIndexCapacityExceeded);
    }
    if (!added->second) {
      return std::unexpected("ground patchwork repeats tile " + std::to_string(tile.Zoom) + "/" +
                             std::to_string(tile.X) + "/" + std::to_string(tile.Y) + " at sheets " +
                             std::to_string(*added->first) + " and " + std::to_string(at));
    }
  }
  return wanted;
}

}

TerrainResidency::TerrainResidency(const TerrainResidency &other)
    : Held_(other.Held_),
      Instances_(other.Instances_),
      Virtual_(other.Virtual_),
      Flat_(other.Flat_),
      GridPostings_(other.GridPostings_),
      Renderer_(other.Renderer_) {
  for (size_t at = 0; at < Held_.size(); ++at) {
    if (!PageIndex_.Emplace(Held_[at].Tile, at)) { Heap::Exhausted("height page index"); }
  }
}

std::expected<Render::HeightPageHandle, std::string>
TerrainResidency::PageFor(Data::TileId tile, std::span<const float> nodes) {
  if (const size_t *found = PageIndex_.Find(tile)) {
    Held &one = Held_[*found];
    if (one.Page && Renderer_->HasHeightPage(one.Page) && std::ranges::equal(one.Nodes, nodes)) {
      return one.Page;
    }
    std::vector<float> replacement(nodes.begin(), nodes.end());
    const auto page = Renderer_->PlaceHeightPage(replacement);
    if (!page) { return std::unexpected(page.error()); }
    if (Renderer_->HasHeightPage(one.Page)) { Renderer_->ReleaseHeightPage(one.Page); }
    one.Page = *page;
    one.Nodes = std::move(replacement);
    return one.Page;
  }
  std::vector<float> owned(nodes.begin(), nodes.end());
  const auto page = Renderer_->PlaceHeightPage(owned);
  if (!page) { return std::unexpected(page.error()); }
  const auto indexed = PageIndex_.Emplace(tile, Held_.size());
  if (!indexed) {
    Renderer_->ReleaseHeightPage(*page);
    return std::unexpected(indexed.error() == FlatMapError::AllocationFailed
                               ? Says::HeightPageIndexAllocationFailed
                               : Says::HeightPageIndexCapacityExceeded);
  }
  Held_.push_back({.Tile = tile, .Page = *page, .Nodes = std::move(owned)});
  return *page;
}

Render::TerrainTile TerrainResidency::TileOf(Data::TileId tile,
                                             Render::HeightPageHandle page,
                                             std::span<const float> nodes,
                                             const TangentFrame &frame) {
  const Ground::GeoBounds bounds = Ground::TileBounds(tile);
  const double midLon = 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg);
  const double midLat = 0.5 * (bounds.MinLatDeg + bounds.MaxLatDeg);
  const Vec3 centre = EcefOf(midLon, midLat);
  const EnuAxes tileFrame =
      EnuAxesEcef({.LongitudeDeg = midLon, .LatitudeDeg = midLat, .HeightM = 0.0});
  const auto corner = [&](double lonDeg, double latDeg) {
    const Vec3 away = EcefOf(lonDeg, latDeg) - centre;
    return std::array<float, 2>{{static_cast<float>(Dot(away, tileFrame.East)),
                                 static_cast<float>(Dot(away, tileFrame.North))}};
  };
  const std::array<float, 2> nw = corner(bounds.MinLonDeg, bounds.MaxLatDeg);
  const std::array<float, 2> ne = corner(bounds.MaxLonDeg, bounds.MaxLatDeg);
  const std::array<float, 2> sw = corner(bounds.MinLonDeg, bounds.MinLatDeg);
  const std::array<float, 2> se = corner(bounds.MaxLonDeg, bounds.MinLatDeg);

  const EastNorthUp at = frame.ToLocalPosition(centre);
  Render::TerrainTile made;
  const std::array<const Vec3 *, 3> axes = {{&tileFrame.East, &tileFrame.North, &tileFrame.Up}};
  for (size_t column = 0; column < axes.size(); ++column) {
    made.Row[column * 4u] = static_cast<float>(Dot(frame.EastEcef(), *axes[column]));
    made.Row[column * 4u + 1u] = static_cast<float>(Dot(frame.UpEcef(), *axes[column]));
    made.Row[column * 4u + 2u] =
        static_cast<float>(RenderFrame::ZOfNorth(Dot(frame.NorthEcef(), *axes[column])));
  }
  made.Row[12] = static_cast<float>(at.EastM);
  made.Row[13] = static_cast<float>(at.UpM);
  made.Row[14] = static_cast<float>(RenderFrame::ZOfNorth(at.NorthM));
  made.Row[15] = 1.0f;
  made.Corners = {{nw[0], nw[1], ne[0], ne[1], sw[0], sw[1], se[0], se[1]}};
  made.Page = page;
  made.SagInv = static_cast<float>(1.0 / std::sqrt(Dot(centre, centre)));
  const auto steps = static_cast<float>(Render::GroundLattice::kSide - 1);
  made.StepE = 0.5f * ((ne[0] - nw[0]) + (se[0] - sw[0])) / steps;
  made.StepN = 0.5f * ((nw[1] - sw[1]) + (ne[1] - se[1])) / steps;
  const auto [low, high] = std::ranges::minmax_element(nodes);
  const float skirt = Render::GroundLattice::kSkirtSteps * std::max(made.StepE, made.StepN);
  const float sag = 0.5f * std::max({Dot2(nw), Dot2(ne), Dot2(sw), Dot2(se)}) * made.SagInv;
  made.LowM = (nodes.empty() ? 0.0f : *low) - skirt - sag;
  made.HighM = nodes.empty() ? 0.0f : *high;
  return made;
}

bool TerrainResidency::PublishGrid(const Patchwork &patchwork, std::string &error) {
  for (const Sheet &sheet : patchwork.Sheets) {
    if (sheet.Side != Render::GroundLattice::kSide || sheet.Virtual || sheet.Postings < 2 ||
        sheet.Postings == GridPostings_) {
      continue;
    }
    std::vector<float> fractions;
    fractions.reserve(static_cast<size_t>(Render::GroundLattice::kSide));
    for (int k = 0; k < Render::GroundLattice::kSide; ++k) {
      fractions.push_back(
          static_cast<float>(FractionOf(k, sheet.Postings, Render::GroundLattice::kSide)));
    }
    if (!Renderer_->SetGroundGrid(fractions, error)) { return false; }
    GridPostings_ = sheet.Postings;
    return true;
  }
  return true;
}

bool TerrainResidency::ReindexPages(std::string &error) {
  PageIndex_.Clear();
  for (size_t at = 0; at < Held_.size(); ++at) {
    const auto indexed = PageIndex_.Emplace(Held_[at].Tile, at);
    if (!indexed) {
      error = indexed.error() == FlatMapError::AllocationFailed
                  ? Says::HeightPageIndexAllocationFailed
                  : Says::HeightPageIndexCapacityExceeded;
      return false;
    }
  }
  return true;
}

bool TerrainResidency::Publish(const Patchwork &patchwork,
                               const TangentFrame &frame,
                               std::string &error) {
  if (!BeginPublish(patchwork, error)) { return false; }
  const auto advanced = AdvancePublish(patchwork, frame, patchwork.Sheets.size());
  if (!advanced) {
    error = advanced.error();
    return false;
  }
  return *advanced;
}

bool TerrainResidency::BeginPublish(const Patchwork &patchwork, std::string &error) {
  if (Renderer_ == nullptr) { return true; }
  auto wanted = IndexSheets(patchwork);
  if (!wanted) {
    error = std::move(wanted.error());
    return false;
  }
  std::erase_if(Held_, [&](const Held &one) {
    if (wanted->Holds(one.Tile)) { return false; }
    Renderer_->ReleaseHeightPage(one.Page);
    return true;
  });
  if (!ReindexPages(error)) { return false; }
  Flat_ = 0;
  Instances_.clear();
  Virtual_.clear();
  NextSheet_ = 0;
  Publishing_ = true;
  LongestBatchMs_ = 0.0;
  FinalizeMs_ = 0.0;
  return true;
}

std::expected<bool, std::string> TerrainResidency::AdvancePublish(const Patchwork &patchwork,
                                                                  const TangentFrame &frame,
                                                                  size_t sheetsMost) {
  if (Renderer_ == nullptr) { return true; }
  if (!Publishing_ || (sheetsMost == 0 && !patchwork.Sheets.empty())) {
    return std::unexpected("height pages are not prepared for a bounded publication");
  }
  const auto batchAt = std::chrono::steady_clock::now();
  const size_t end = std::min(NextSheet_ + sheetsMost, patchwork.Sheets.size());
  for (; NextSheet_ < end; ++NextSheet_) {
    const Sheet &sheet = patchwork.Sheets[NextSheet_];
    if (sheet.Side != Render::GroundLattice::kSide ||
        sheet.Nodes.size() != Render::GroundLattice::kPageNodes) {
      ++Flat_;
      continue;
    }
    const auto page = PageFor(sheet.Tile, sheet.Nodes);
    if (!page) { return std::unexpected(page.error()); }
    (sheet.Virtual ? Virtual_ : Instances_)
        .push_back(TileOf(sheet.Tile, *page, sheet.Nodes, frame));
  }
  LongestBatchMs_ =
      std::max(LongestBatchMs_,
               std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - batchAt)
                   .count());
  if (NextSheet_ < patchwork.Sheets.size()) { return false; }
  const auto finalizeAt = std::chrono::steady_clock::now();
  std::ranges::sort(Held_, [](const Held &left, const Held &right) {
    if (left.Tile.Zoom != right.Tile.Zoom) { return left.Tile.Zoom < right.Tile.Zoom; }
    if (left.Tile.X != right.Tile.X) { return left.Tile.X < right.Tile.X; }
    return left.Tile.Y < right.Tile.Y;
  });
  std::string error;
  if (!ReindexPages(error) || !PublishGrid(patchwork, error) ||
      !Renderer_->SetTerrainTiles(Instances_, Virtual_, error)) {
    return std::unexpected(std::move(error));
  }
  FinalizeMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - finalizeAt)
          .count();
  Publishing_ = false;
  return true;
}

void TerrainResidency::Clear() {
  if (Renderer_ != nullptr) {
    for (const Held &one : Held_) { Renderer_->ReleaseHeightPage(one.Page); }
    std::string ignored;
    (void)Renderer_->SetTerrainTiles({}, {}, ignored);
  }
  Renderer_ = nullptr;
  Held_.clear();
  PageIndex_.Clear();
  Instances_.clear();
  Virtual_.clear();
  GridPostings_ = 0;
  NextSheet_ = 0;
  Publishing_ = false;
  LongestBatchMs_ = 0.0;
  FinalizeMs_ = 0.0;
}

uint64_t TerrainResidency::Digest() const noexcept {
  uint64_t digest = kDigestBasis;
  const auto fold = [&digest](uint32_t word) { digest = (digest ^ word) * kDigestPrime; };
  const auto foldFloat = [&fold](float value) { fold(std::bit_cast<uint32_t>(value)); };
  for (const std::vector<Render::TerrainTile> *tiles : {&Instances_, &Virtual_}) {
    for (const Render::TerrainTile &one : *tiles) {
      for (const float value : one.Row) { foldFloat(value); }
      for (const float value : one.Corners) { foldFloat(value); }
      foldFloat(one.SagInv);
      foldFloat(one.StepE);
      foldFloat(one.StepN);
      foldFloat(one.LowM);
      foldFloat(one.HighM);
    }
  }
  for (const Held &held : Held_) {
    fold(static_cast<uint32_t>(held.Tile.Zoom));
    fold(held.Tile.X);
    fold(held.Tile.Y);
    for (const float value : held.Nodes) { foldFloat(value); }
  }
  return digest;
}

uint64_t TerrainResidency::TileHash::operator()(const Data::TileId &tile) const noexcept {
  uint64_t hash = static_cast<uint32_t>(tile.Zoom);
  hash = (hash ^ tile.X) * kTileHashMix;
  return (hash ^ tile.Y) * kTileHashMix;
}

size_t TerrainResidency::HeapBytes() const noexcept {
  size_t bytes = Held_.capacity() * sizeof(Held) + PageIndex_.HeapBytes() +
                 Instances_.capacity() * sizeof(Render::TerrainTile) +
                 Virtual_.capacity() * sizeof(Render::TerrainTile);
  for (const Held &held : Held_) { bytes += held.Nodes.capacity() * sizeof(float); }
  return bytes;
}

}
