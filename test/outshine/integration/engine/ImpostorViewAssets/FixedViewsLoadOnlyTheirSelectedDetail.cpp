#include "ImpostorViewAssets.h"
#include "ImpostorSurface.h"
#include "AssetCache.h"
#include "Check.h"
#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "Material.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <sqlite3.h>

namespace {
using namespace outshine;
using namespace outshine::Content;
using namespace outshine::Test;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr std::string_view kSource = "source-cell-8/fixed-capture-1";
const Box kBounds{.Min = {{6378000, -16, -16}}, .Max = {{6378032, 16, 16}}};

std::optional<ImpostorAtlas> Atlas() {
  std::vector<ImpostorAtlas::View> views;
  for (const Vec3 direction : {Vec3{{0, 0, 1}}, Vec3{{1, 0, 0}}}) {
    ImpostorAtlas::View view{.TowardEye = direction, .Texels = {}, .Materials = {}};
    view.Texels.resize(64);
    for (size_t at = 0; at < view.Texels.size(); ++at) {
      if (at % 3 == 0) { continue; }
      view.Texels[at] = {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1};
    }
    views.push_back(std::move(view));
  }
  std::string error;
  return ImpostorAtlas::Create(8, {{1, 2, 3}}, 16, {Material{}}, std::move(views), error);
}

void InvalidDescriptions(AssetCache &cache, const ImpostorViewAssets::Description &description) {
  auto invalid = description;
  invalid.Directions[0] = {};
  CHECK(!ImpostorViewAssets::Publish(cache, kSource, invalid, kBounds),
        "zero capture directions cannot create a selection index");
  invalid = description;
  invalid.Directions[0][0] = std::numeric_limits<double>::infinity();
  CHECK(!ImpostorViewAssets::Publish(cache, kSource, invalid, kBounds),
        "non-finite directions are rejected before publication");
  invalid = description;
  invalid.Pixels = 0;
  CHECK(!ImpostorViewAssets::Publish(cache, kSource, invalid, kBounds) &&
            !ImpostorViewAssets::Publish(cache, {}, description, kBounds),
        "invalid dimensions and missing source provenance create no assets");
}

void PublishAll(const std::string &path) {
  auto cache = AssetCache::Open(path);
  const auto atlas = Atlas();
  CHECK(cache && atlas, "native package store and source capture open");
  if (!cache || !atlas) { return; }
  const ImpostorViewAssets::Description description{.Centre = atlas->CentreM(),
                                                    .HalfExtentM = atlas->HalfExtentM(),
                                                    .Pixels = 8,
                                                    .Directions = {{{0, 0, 1}}, {{1, 0, 0}}}};
  InvalidDescriptions(**cache, description);
  const auto products = ImpostorViewAssets::Publish(**cache, kSource, description, kBounds);
  CHECK(products.has_value(), "small fixed-view metadata is independently published");
  if (!products) { return; }
  CHECK(!products->Load({.View = 0, .Mip = 0}, kLimit)->has_value(),
        "metadata availability never invents ready child coverage");
  std::vector<std::string> packages;
  for (size_t view = 0; view < description.Directions.size(); ++view) {
    for (uint32_t mip = 0; mip <= 3; ++mip) {
      const ImpostorViewAssets::Selection selected{.View = view, .Mip = mip};
      auto surface = Render::BuildFractionalImpostorSurface(*atlas, view, mip);
      CHECK(surface && products->Publish(selected, std::move(*surface), kLimit),
            "each fixed view and detail publishes its own complete native product");
      const auto record = (*cache)->Find(products->Key(selected));
      CHECK(record && *record && (**record).Bounds == kBounds && (**record).Level == mip &&
                (**record).OffsetBytes == 0,
            "spatial bounds remain world coordinates while cards retain their local frame");
      if (record && *record) { packages.push_back((**record).Package); }
    }
  }
  std::ranges::sort(packages);
  CHECK(packages.size() == 8 && std::ranges::adjacent_find(packages) == packages.end(),
        "all views and details occupy distinct independently readable packages");
  auto wrong = Render::BuildImpostorSurface(*atlas, 0, Render::ImpostorSurfaceDetail::Flat);
  CHECK(wrong && !products->Publish({.View = 0, .Mip = 0}, std::move(*wrong), kLimit),
        "old masked cards cannot be published under the fractional policy");
  auto tooFine = Render::BuildFractionalImpostorSurface(*atlas, 0);
  CHECK(tooFine && !products->Publish({.View = 0, .Mip = 3}, std::move(*tooFine), kLimit),
        "requested coarse products cannot silently contain full-resolution images");
}

void ReadOffline(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache.has_value(), "a fresh source-free reader opens stored packages");
  if (!cache) { return; }
  const auto opened = ImpostorViewAssets::Open(**cache, kSource);
  CHECK(opened && *opened, "only bounded metadata is needed to choose view and detail");
  if (!opened || !*opened) { return; }
  const auto &products = **opened;
  const auto selection = products.Select({{0, 0, 100}}, 1);
  CHECK((selection && *selection == ImpostorViewAssets::Selection{.View = 0, .Mip = 3}),
        "eye direction and projected support choose an existing fixed product before IO");
  CHECK((products.Select({{1, 0, 0}}, 2) == ImpostorViewAssets::Selection{.View = 1, .Mip = 2} &&
         products.Select({{1, 0, 0}}, 2.01) == ImpostorViewAssets::Selection{.View = 1, .Mip = 1}),
        "detail never chooses fewer source texels than the projected diameter");
  CHECK(!products.Select({}, 1) && !products.Select({{1, 0, 0}}, 0) &&
            !products.Select({{1, 0, 0}}, std::numeric_limits<double>::quiet_NaN()),
        "invalid camera demand cannot become an invented selection");
  if (!selection) { return; }
  const auto small = (*cache)->Find(products.Key(*selection));
  if (!small || !*small) {
    CHECK(false, "selected product must be indexed");
    return;
  }
  const auto budget = static_cast<size_t>((**small).ByteCount);
  const auto loaded = products.Load(*selection, budget);
  CHECK(loaded && *loaded && (**loaded).Views.size() == 1 &&
            (**loaded).Views[0].Surface.imageAt(0).WidthPx == 1,
        "a tight one-product budget loads the selected ready geometry without a producer");
  CHECK(!products.Load({.View = 0, .Mip = 0}, budget),
        "the fine package cannot fit the same coarse-product allocation budget");
  CHECK(!products.Load({.View = 0, .Mip = 4}, budget) &&
            !products.Load({.View = 2, .Mip = 0}, budget),
        "invalid view and mip fail before package IO");
  sqlite3 *database = nullptr;
  const std::string corrupt = "UPDATE packages SET crc=-1 WHERE key IN "
                              "(SELECT package FROM assets WHERE key='" +
                              products.Key({.View = 1, .Mip = 0}) + "')";
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK &&
            sqlite3_exec(database, corrupt.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
        "fixture corrupts only an unselected independent package");
  CHECK(sqlite3_close(database) == SQLITE_OK, "corruption fixture releases its connection");
  CHECK(products.Load(*selection, budget)->has_value() &&
            !products.Load({.View = 1, .Mip = 0}, kLimit)->has_value(),
        "unselected corruption cannot poison the visible view; its own load is a miss");
  auto changed = products.Metadata();
  std::ranges::reverse(changed.Directions);
  const auto next = ImpostorViewAssets::Publish(**cache, kSource, std::move(changed), kBounds);
  CHECK(next && next->Key(*selection) != products.Key(*selection) &&
            !next->Load(*selection, kLimit)->has_value() &&
            products.Load(*selection, budget)->has_value(),
        "changed fixed-view metadata cannot reinterpret old products; held readers remain valid");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-fixed-view-assets-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = (root / "assets.sqlite").string();
  PublishAll(path);
  ReadOffline(path);
  std::filesystem::remove_all(root);
  return Report();
}
