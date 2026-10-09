#include "OsmField.h"
#include "OsmStorageUsage.h"
#include "PreparedOsmTiles.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <expected>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
std::expected<bool, std::string_view> OsmField::PollTile(TilePool &tiles, TileAt at) {
  if (Zoom_ < 0 || Zoom_ >= std::numeric_limits<int>::digits || at.X < 0 || at.Y < 0 ||
      std::cmp_greater_equal(at.X, uint64_t{1} << static_cast<unsigned>(Zoom_)) ||
      std::cmp_greater_equal(at.Y, uint64_t{1} << static_cast<unsigned>(Zoom_))) {
    return std::unexpected("invalid OSM tile address");
  }
  if (Settled(at.X, at.Y)) { return true; }
  auto fetched = AddTile(tiles, at);
  if (!fetched) { return std::unexpected(fetched.error()); }
  if (fetched->Refused) { return std::unexpected("OSM tile source refused demand"); }
  if (!fetched->Held) { return false; }
  if (auto published = PublishParsed(nullptr, std::nullopt); !published) {
    return std::unexpected(published.error());
  }
  Settle(at.X, at.Y);
  Stage_ = SnapshotStage::Complete;
  PublishedSettledTiles_ = Settled_.size();
  return true;
}

std::expected<OsmField::Fetched, std::string_view> OsmField::AddNativeTile(TilePool &tiles,
                                                                           TileAt at) {
  auto delivery = Prepared_->Acquire(tiles, at, *this);
  if (!delivery) { return std::unexpected(delivery.error()); }
  using State = PreparedOsmTiles::State;
  switch (delivery->Status) {
    case State::Pending: return Fetched{};
    case State::Refused: return Fetched{.Held = false, .Refused = true};
    case State::Absent: return Fetched{.Held = true};
    case State::Ready: break;
  }
  const auto &native = delivery->Product;
  const auto &source = native->Tiles_.front();
  const int count = static_cast<int>(native->Features_.size());
  ParsedTiles_.push_back({.At = at,
                          .LayerBytes = 0,
                          .Layers = {},
                          .Source = source.Source,
                          .InputDigest = source.InputDigest,
                          .Native = std::move(delivery->Product)});
  return Fetched{.Held = true, .Added = count, .Parsed = true};
}

bool OsmField::AppendNativeTile(const OsmField &tile, OsmStorageUsage &usage) {
  if (!usage.TryAdd({.Features = tile.Features_.size(),
                     .Rings = tile.Rings_.size(),
                     .Points = tile.Points_.size() / 2,
                     .Tags = tile.Tags_.size(),
                     .Values = tile.Values_.size(),
                     .Keys = tile.Keys_.size(),
                     .Strings = tile.Strings_.size(),
                     .Tiles = 1})) {
    return false;
  }
  const auto ringBase = static_cast<uint32_t>(Rings_.size());
  const auto pointBase = static_cast<uint32_t>(Points_.size() / 2);
  const auto tagBase = static_cast<uint32_t>(Tags_.size());
  const auto valueBase = static_cast<uint32_t>(Values_.size());
  std::vector<uint32_t> keys;
  std::vector<uint32_t> strings;
  keys.reserve(tile.Keys_.size());
  strings.reserve(tile.Strings_.size());
  for (const auto &key : tile.Keys_) { keys.push_back(Intern(Keys_, KeyIndex_, key)); }
  for (const auto &value : tile.Strings_) {
    strings.push_back(Intern(Strings_, StringIndex_, value));
  }
  for (auto value : tile.Values_) {
    if (!value.IsNum) { value.Str = strings[value.Str]; }
    Values_.push_back(value);
  }
  for (size_t at = 0; at < tile.Tags_.size(); at += 2) {
    Tags_.push_back(keys[tile.Tags_[at]]);
    Tags_.push_back(tile.Tags_[at + 1] + valueBase);
  }
  Points_.insert(Points_.end(), tile.Points_.begin(), tile.Points_.end());
  for (auto ring : tile.Rings_) {
    ring.First += pointBase;
    Rings_.push_back(ring);
  }
  auto source = tile.Tiles_.front();
  source.FirstFeature = static_cast<uint32_t>(Features_.size());
  const auto tileIndex = static_cast<uint32_t>(Tiles_.size());
  Tiles_.push_back(std::move(source));
  for (auto feature : tile.Features_) {
    feature.Tile = tileIndex;
    feature.FirstRing += ringBase;
    feature.FirstTag += tagBase;
    Features_.push_back(feature);
  }
  Extent_ = std::min(Extent_, tile.Extent_);
  Missing_ += tile.Missing_;
  return true;
}
}
