#include <expected>
#include <algorithm>
#include <array>
#include <utility>
#include <cmath>
#include <cstdint>
#include <limits>
#include "math/Units.h"
#include "OsmField.h"

#include "Capacity.h"
#include "Log.h"
#include "OsmVector.h"
#include "TerrainLoader.h"
#include "TileGeodesy.h"

#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <ratio>

namespace outshine::Ground {

constexpr double kNoLeastYet = 1e9;

namespace {

uint64_t TileKey(int x, int y) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) | static_cast<uint32_t>(y);
}

}

OsmField::OsmField(int zoom, std::span<const std::string> layers)
    : Layers_(layers.begin(), layers.end()), Zoom_(zoom) {}

uint32_t OsmField::Intern(std::vector<std::string> &pool,
                          std::unordered_map<std::string, uint32_t> &index,
                          std::string_view s) {
  const std::string key(s);
  const auto it = index.find(key);
  if (it != index.end()) { return it->second; }
  const auto id = static_cast<uint32_t>(pool.size());
  pool.push_back(key);
  index.emplace(key, id);
  return id;
}

namespace Says {
constexpr std::string_view kInvalidVectorTile = "OSM tile contains invalid vector data";
constexpr std::string_view kUnsupportedVectorTile = "OSM tile version is unsupported";
constexpr std::string_view kTooManyVectorLayers = "OSM layer count exceeds native index capacity";
constexpr std::string_view kInvalidOsmPosition =
    "OSM requires finite canonical longitude/latitude and a signed-index-compatible zoom";
constexpr std::string_view kInvalidOsmRadius = "OSM tile radius must be nonnegative";
constexpr std::string_view kOsmTileBudgetExceeded =
    "OSM tile window exceeds its visit budget or counter capacity";
constexpr std::string_view kOutsideOsmCoverage =
    "OSM position lies outside the Mercator coverage band";
}

std::expected<TileAt, std::string_view> OsmField::Locate(LongitudeLatitude at, int zoom) noexcept {
  if (zoom < 0 || zoom > std::numeric_limits<int>::digits) {
    return std::unexpected(Says::kInvalidOsmPosition);
  }
  const auto index =
      Ground::TileIndex::Of({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg}, zoom);
  if (index.Where() == Ground::TileIndex::State::InvalidInput) {
    return std::unexpected(Says::kInvalidOsmPosition);
  }
  const auto tile = index.Tile();
  if (!tile) { return std::unexpected(Says::kOutsideOsmCoverage); }
  return TileAt{.X = static_cast<int>(tile->X), .Y = static_cast<int>(tile->Y)};
}

namespace {
static_assert(2 * std::numeric_limits<int>::digits < std::numeric_limits<uint64_t>::digits);

struct TileWindow {
  int64_t MinX, MaxX, MinY, MaxY;
};

struct TileWindowRequest {
  TileAt Centre;
  int Zoom;
  int Radius;
  size_t Budget;
};

std::expected<TileWindow, std::string_view> TileWindowFor(TileWindowRequest request) {
  if (request.Radius < 0) { return std::unexpected(Says::kInvalidOsmRadius); }
  const auto last = static_cast<int64_t>((uint64_t{1} << static_cast<unsigned>(request.Zoom)) - 1);
  const int64_t x = request.Centre.X;
  const int64_t y = request.Centre.Y;
  const TileWindow window{.MinX = std::max(int64_t{0}, x - request.Radius),
                          .MaxX = std::min(last, x + request.Radius),
                          .MinY = std::max(int64_t{0}, y - request.Radius),
                          .MaxY = std::min(last, y + request.Radius)};
  const auto width = static_cast<uint64_t>(window.MaxX - window.MinX + 1);
  const auto height = static_cast<uint64_t>(window.MaxY - window.MinY + 1);
  const auto count = width * height;
  if (count > request.Budget || count > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
    return std::unexpected(Says::kOsmTileBudgetExceeded);
  }
  return window;
}
}

std::expected<int, std::string_view>
OsmField::Build(TilePool &tiles, LongitudeLatitude at, int ringTiles, size_t tileBudget) {
  const auto centre = Locate(at, Zoom_);
  if (!centre) { return std::unexpected(centre.error()); }
  const auto window =
      TileWindowFor({.Centre = *centre, .Zoom = Zoom_, .Radius = ringTiles, .Budget = tileBudget});
  if (!window) { return std::unexpected(window.error()); }
  Pending_ = 0;
  Refused_ = 0;
  CentreX_ = centre->X;
  CentreY_ = centre->Y;
  int added = 0;

  for (int64_t ty = window->MinY; ty <= window->MaxY; ++ty) {
    for (int64_t tx = window->MinX; tx <= window->MaxX; ++tx) {
      const uint64_t key = TileKey(static_cast<int>(tx), static_cast<int>(ty));
      if (std::ranges::find(Settled_, key) != Settled_.end()) { continue; }

      const auto got = AddTile(tiles, {.X = static_cast<int>(tx), .Y = static_cast<int>(ty)});
      if (!got) { return std::unexpected(got.error()); }
      if (!got->Held) {
        if (got->Refused) {
          Refused_++;
        } else {
          Pending_++;
        }
        continue;
      }
      added += got->Added;
      Settle(static_cast<int>(tx), static_cast<int>(ty));
    }
  }

  return added;
}

bool OsmField::Settled(int x, int y) const {
  return std::ranges::find(Settled_, TileKey(x, y)) != Settled_.end();
}

void OsmField::Settle(int x, int y) {
  const uint64_t key = TileKey(x, y);
  if (std::ranges::find(Settled_, key) == Settled_.end()) { Settled_.push_back(key); }
}

int OsmField::TileIndex(int x, int y) const {
  for (size_t i = 0; i < Tiles_.size(); i++) {
    if (Tiles_[i].X == x && Tiles_[i].Y == y) { return static_cast<int>(i); }
  }
  return -1;
}

std::span<const OsmField::Feature> OsmField::OfTile(int index) const {
  if (index < 0 || static_cast<size_t>(index) >= Tiles_.size()) { return {}; }
  const Tile &t = Tiles_[static_cast<size_t>(index)];
  return {Features_.data() + t.FirstFeature, t.FeatureCount};
}

std::expected<OsmField::Fetched, std::string_view> OsmField::AddTile(TilePool &tiles, TileAt at) {
  const Data::Fetch request(Data::DataKind::VectorMap,
                            Data::Address::At(Data::TileId{.Zoom = Zoom_,
                                                           .X = static_cast<uint32_t>(at.X),
                                                           .Y = static_cast<uint32_t>(at.Y)}));
  const TilePool::Reply reply = tiles.Bytes(request, &Scratch_);

  const bool refused = reply == TilePool::Reply::Refused;
  if (reply == TilePool::Reply::Pending || refused) {
    return Fetched{.Held = false, .Refused = refused};
  }
  if (reply == TilePool::Reply::Absent || reply == TilePool::Reply::Undeclared) {
    return Fetched{.Held = true};
  }
  const auto accepted = Accept(at.X, at.Y, Scratch_.Bytes);
  if (!accepted) { return std::unexpected(accepted.error()); }
  return Fetched{.Held = true, .Added = *accepted};
}

namespace {
using VectorLayers = std::vector<std::optional<OsmVector>>;

[[nodiscard]] std::expected<VectorLayers, std::string_view>
ReadVectorLayers(std::span<const uint8_t> bytes, std::span<const std::string> names) {
  if (names.size() > static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1) {
    return std::unexpected(Says::kTooManyVectorLayers);
  }
  VectorLayers layers;
  layers.reserve(names.size());
  for (const auto &name : names) {
    OsmVector layer;
    const auto result = layer.Parse(bytes, name);
    if (result) {
      layers.emplace_back(std::move(layer));
    } else if (result.error() == OsmVector::ParseError::MissingLayer) {
      layers.emplace_back(std::nullopt);
    } else {
      return std::unexpected(result.error() == OsmVector::ParseError::UnsupportedVersion
                                 ? Says::kUnsupportedVectorTile
                                 : Says::kInvalidVectorTile);
    }
  }
  return layers;
}
}

std::expected<int, std::string_view>
OsmField::Accept(int tx, int ty, std::span<const uint8_t> vectorTile) {
  const auto layers = ReadVectorLayers(vectorTile, Layers_);
  if (!layers) {
    ++Bad_;
    return std::unexpected(layers.error());
  }
  const size_t first = Features_.size();
  Tiles_.push_back(Tile{.Z = Zoom_,
                        .X = tx,
                        .Y = ty,
                        .FirstFeature = static_cast<uint32_t>(first),
                        .FeatureCount = 0});
  for (size_t i = 0; i < layers->size(); ++i) {
    const auto &layer = (*layers)[i];
    if (layer) {
      AppendLayer(*layer, static_cast<uint16_t>(i));
    } else {
      ++Missing_;
    }
  }
  const size_t added = Features_.size() - first;
  Tiles_.back().FeatureCount = static_cast<uint32_t>(added);
  Settle(tx, ty);
  return static_cast<int>(added);
}

void OsmField::AppendLayer(const OsmVector &layer, uint16_t layerIndex) {
  const auto tile = static_cast<uint32_t>(Tiles_.size() - 1);
  const auto ext = static_cast<double>(layer.Extent());
  Extent_ = std::min(Extent_, layer.Extent());
  const auto &pts = layer.Points();
  const int tx = Tiles_.back().X;
  const int ty = Tiles_.back().Y;
  for (const OsmVector::Feature &sf : layer.Features()) {
    Feature f{};
    f.Tile = tile;
    f.Layer = layerIndex;
    f.Type = static_cast<uint8_t>(sf.Type);
    f.FirstRing = static_cast<uint32_t>(Rings_.size());
    f.FirstTag = static_cast<uint32_t>(Tags_.size());
    f.MinLat = f.MinLon = kNoLeastYet;
    f.MaxLat = f.MaxLon = -kNoLeastYet;

    for (uint32_t r = 0; r < sf.RingCount; r++) {
      const OsmVector::Ring &sr = layer.Rings()[sf.FirstRing + r];
      Ring ring{};
      ring.First = static_cast<uint32_t>(Points_.size() / 2);
      ring.Count = sr.Count;
      ring.Exterior = sr.Exterior;
      for (uint32_t k = 0; k < sr.Count; k++) {
        const auto px = static_cast<double>(pts[(static_cast<size_t>(sr.First) + k) * 2]);
        const auto py = static_cast<double>(pts[(static_cast<size_t>(sr.First) + k) * 2 + 1]);
        const Geo g = TileFracToGeo(
            {.X = static_cast<double>(tx) + px / ext, .Y = static_cast<double>(ty) + py / ext},
            Zoom_);
        Points_.push_back(g.LatitudeDeg);
        Points_.push_back(g.LongitudeDeg);
        f.MinLat = std::min(f.MinLat, g.LatitudeDeg);
        f.MaxLat = std::max(f.MaxLat, g.LatitudeDeg);
        f.MinLon = std::min(f.MinLon, g.LongitudeDeg);
        f.MaxLon = std::max(f.MaxLon, g.LongitudeDeg);
      }
      Rings_.push_back(ring);
    }
    f.RingCount = static_cast<uint32_t>(Rings_.size()) - f.FirstRing;

    for (uint32_t t = 0; t < outshine::Ground::OsmVector::TagCount(sf); t++) {
      const OsmVector::Tag tag = layer.TagAt(sf, t);
      if (tag.Key.empty()) { continue; }
      Value v{};
      v.IsNum = tag.IsNum;
      if (tag.IsNum) {
        v.Num = tag.Num;
      } else {
        v.Str = Intern(Strings_, StringIndex_, tag.Str);
      }
      Tags_.push_back(Intern(Keys_, KeyIndex_, tag.Key));
      Tags_.push_back(static_cast<uint32_t>(Values_.size()));
      Values_.push_back(v);
    }
    f.TagCount = static_cast<uint32_t>(Tags_.size()) - f.FirstTag;

    Features_.push_back(f);
  }
}

void OsmField::Settle() {
  Features_.shrink_to_fit();
  Rings_.shrink_to_fit();
  Points_.shrink_to_fit();
  Tiles_.shrink_to_fit();
  Tags_.shrink_to_fit();
  Values_.shrink_to_fit();
  Settled_.shrink_to_fit();
  Scratch_.Bytes.shrink_to_fit();
  Keys_.shrink_to_fit();
}

size_t OsmField::HeapBytes() const {
  size_t strings = 0;
  for (const std::string &s : Keys_) { strings += s.capacity(); }
  for (const std::string &s : Strings_) { strings += s.capacity(); }
  for (const std::string &s : Layers_) { strings += s.capacity(); }

  const size_t nodes = (KeyIndex_.size() + StringIndex_.size()) *
                       (sizeof(std::string) + sizeof(uint32_t) + 2 * sizeof(void *));
  return CapacityBytes(Features_) + CapacityBytes(Rings_) + CapacityBytes(Points_) +
         CapacityBytes(Tiles_) + CapacityBytes(Tags_) + CapacityBytes(Values_) +
         CapacityBytes(Settled_) + CapacityBytes(Scratch_.Bytes) + CapacityBytes(Keys_) +
         CapacityBytes(Strings_) + CapacityBytes(Layers_) + strings + nodes;
}

int OsmField::Layer(const char *name) const {
  for (size_t i = 0; i < Layers_.size(); i++) {
    if (Layers_[i] == name) { return static_cast<int>(i); }
  }
  return -1;
}

std::expected<void, std::string_view> OsmField::Declare(std::span<const Declared> these,
                                                        LongitudeLatitude at) {
  const auto tile = Locate(at, Zoom_);
  if (!tile) { return std::unexpected(tile.error()); }
  Declare(these, *tile);
  return {};
}

void OsmField::AppendDeclaredFeature(const Declared &one) {
  const auto number = [this](double how) {
    Values_.push_back(Value{.Num = how, .Str = 0, .IsNum = true});
    return static_cast<uint32_t>(Values_.size() - 1u);
  };
  const auto words = [this](std::string_view how) {
    Values_.push_back(
        Value{.Num = 0.0, .Str = Intern(Strings_, StringIndex_, how), .IsNum = false});
    return static_cast<uint32_t>(Values_.size() - 1u);
  };

  if (one.LatLon.size() < 4) { return; }
  Feature made;
  made.FirstRing = static_cast<uint32_t>(Rings_.size());
  made.RingCount = 1;
  made.FirstTag = static_cast<uint32_t>(Tags_.size());
  made.Tile = 0;
  const int layer = Layer(one.Layer.c_str());
  made.Layer = static_cast<uint16_t>(layer < 0 ? 0 : layer);
  made.Type = one.Area ? 3u : 2u;
  made.MinLat = made.MaxLat = one.LatLon[0];
  made.MinLon = made.MaxLon = one.LatLon[1];

  Ring ring;
  ring.First = static_cast<uint32_t>(Points_.size() / 2u);
  ring.Count = static_cast<uint32_t>(one.LatLon.size() / 2u);
  ring.Exterior = one.Area;
  for (size_t at = 0; at + 1 < one.LatLon.size(); at += 2) {
    made.MinLat = std::min(made.MinLat, one.LatLon[at]);
    made.MaxLat = std::max(made.MaxLat, one.LatLon[at]);
    made.MinLon = std::min(made.MinLon, one.LatLon[at + 1]);
    made.MaxLon = std::max(made.MaxLon, one.LatLon[at + 1]);
    Points_.push_back(one.LatLon[at]);
    Points_.push_back(one.LatLon[at + 1]);
  }
  Rings_.push_back(ring);

  Tags_.push_back(Intern(Keys_, KeyIndex_, one.Key));
  Tags_.push_back(words(one.Value));
  if (one.WidthM > 0.0) {
    Tags_.push_back(Intern(Keys_, KeyIndex_, "width"));
    Tags_.push_back(number(one.WidthM));
  }
  if (one.HeightM > 0.0) {
    Tags_.push_back(Intern(Keys_, KeyIndex_, "height"));
    Tags_.push_back(number(one.HeightM));
  }
  if (one.Bridge) {
    Tags_.push_back(Intern(Keys_, KeyIndex_, "bridge"));
    Tags_.push_back(number(1.0));
  }
  if (one.Tunnel) {
    Tags_.push_back(Intern(Keys_, KeyIndex_, "tunnel"));
    Tags_.push_back(number(1.0));
  }
  if (one.Level != 0) {
    Tags_.push_back(Intern(Keys_, KeyIndex_, "layer"));
    Tags_.push_back(number(static_cast<double>(one.Level)));
  }
  made.TagCount = static_cast<uint32_t>(Tags_.size()) - made.FirstTag;
  Features_.push_back(made);
}

bool OsmField::MatchesDeclaredFeature(const Feature &feature, const Declared &input) const {
  const int layer = std::max(Layer(input.Layer.c_str()), 0);
  if (std::cmp_not_equal(feature.Layer, layer) || feature.Type != (input.Area ? 3u : 2u) ||
      feature.RingCount != 1 || feature.TagCount < 2) {
    return false;
  }
  const Ring &ring = Rings_[feature.FirstRing];
  if (ring.Exterior != input.Area || static_cast<size_t>(ring.Count) * 2 != input.LatLon.size()) {
    return false;
  }
  const auto points =
      std::span(Points_).subspan(static_cast<size_t>(ring.First) * 2, input.LatLon.size());
  if (!std::ranges::equal(points, input.LatLon) || Keys_[Tags_[feature.FirstTag]] != input.Key) {
    return false;
  }
  const Value &label = Values_[Tags_[feature.FirstTag + 1]];
  if (label.IsNum || Strings_[label.Str] != input.Value) { return false; }
  const std::array<std::pair<const char *, double>, 5> numeric{
      {{"width", input.WidthM},
       {"height", input.HeightM},
       {"bridge", input.Bridge ? 1.0 : 0.0},
       {"tunnel", input.Tunnel ? 1.0 : 0.0},
       {"layer", static_cast<double>(input.Level)}}};
  uint32_t expectedTags = 2;
  for (const auto &[name, value] : numeric) {
    if (Num(feature, name, 0.0) != value) { return false; }
    if (value != 0.0) { expectedTags += 2; }
  }
  return feature.TagCount == expectedTags;
}

bool OsmField::MatchesDeclaration(std::span<const Declared> input, TileAt over) const {
  if (Tiles_.size() != 1 || Features_.size() != input.size() || CentreX_ != over.X ||
      CentreY_ != over.Y || Tiles_.front().X != over.X || Tiles_.front().Y != over.Y) {
    return false;
  }
  for (size_t at = 0; at < input.size(); ++at) {
    if (!MatchesDeclaredFeature(Features_[at], input[at])) { return false; }
  }
  return true;
}

void OsmField::Declare(std::span<const Declared> these, TileAt over) {
  if (MatchesDeclaration(these, over)) {
    Pending_ = 0;
    return;
  }
  Features_.clear();
  Rings_.clear();
  Points_.clear();
  Tiles_.clear();
  Tags_.clear();
  Values_.clear();
  Keys_.clear();
  Strings_.clear();
  KeyIndex_.clear();
  StringIndex_.clear();
  Settled_.clear();
  CentreX_ = over.X;
  CentreY_ = over.Y;

  for (const Declared &one : these) { AppendDeclaredFeature(one); }

  Tiles_.push_back(Tile{.Z = Zoom_,
                        .X = over.X,
                        .Y = over.Y,
                        .FirstFeature = 0,
                        .FeatureCount = static_cast<uint32_t>(Features_.size())});
  Settle(over.X, over.Y);
  Pending_ = 0;
  ++Generation_;
}

double OsmField::Num(const Feature &f, const char *key, double def) const {
  for (uint32_t i = 0; i + 1 < f.TagCount; i += 2) {
    const uint32_t k = Tags_[f.FirstTag + i];
    const uint32_t v = Tags_[f.FirstTag + i + 1];
    if (Keys_[k] == key && Values_[v].IsNum) { return Values_[v].Num; }
  }
  return def;
}

std::string_view OsmField::Str(const Feature &f, const char *key) const {
  for (uint32_t i = 0; i + 1 < f.TagCount; i += 2) {
    const uint32_t k = Tags_[f.FirstTag + i];
    const uint32_t v = Tags_[f.FirstTag + i + 1];
    if (Keys_[k] == key && !Values_[v].IsNum) { return Strings_[Values_[v].Str]; }
  }
  return {};
}

}
