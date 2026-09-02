#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <chrono>
#include "Units.h"
#include "OsmField.h"

#include "Capacity.h"
#include "Log.h"
#include "OsmVector.h"
#include "TerrainLoader.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <cstdio>
#include <span>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <ratio>

namespace outshine::Ground {

constexpr double kNoLeastYet = 1e9;

constexpr uint64_t kGoldenWord = 0x9e3779b97f4a7c15ULL;
constexpr uint64_t kStirPrime = 0x100000001b3ULL;
constexpr double kFixedPointPerDeg = 1.0e7;
constexpr double kFixedPointPerM = 1.0e3;

namespace {

uint64_t TileKey(int x, int y) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) | static_cast<uint32_t>(y);
}

} // namespace

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

int OsmField::Build(TilePool &tiles, double lat, double lon, int ringTiles, double budgetMs) {
  uint32_t cx = 0;
  uint32_t cy = 0;
  Pending_ = 0;
  Refused_ = 0;

  Geo centre;
  centre.LonDeg = lon;
  centre.LatDeg = lat;
  if (!TileIndex::Of(centre, Zoom_).TryXy(&cx, &cy)) { return 0; }
  CentreX_ = static_cast<int>(cx);
  CentreY_ = static_cast<int>(cy);

  const long n = 1L << static_cast<uint32_t>(Zoom_);
  int added = 0;
  const std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();

  for (int dy = -ringTiles; dy <= ringTiles; dy++) {
    for (int dx = -ringTiles; dx <= ringTiles; dx++) {
      const long tx = static_cast<long>(cx) + dx;
      const long ty = static_cast<long>(cy) + dy;
      if (tx < 0 || ty < 0 || tx >= n || ty >= n) { continue; }
      const uint64_t key = TileKey(static_cast<int>(tx), static_cast<int>(ty));
      if (std::ranges::find(Settled_, key) != Settled_.end()) { continue; }

      const double spentMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      const bool mayDecode = budgetMs <= 0.0 || spentMs < budgetMs;
      bool refused = false;
      if (!AddTile(tiles, static_cast<int>(tx), static_cast<int>(ty), added, refused, mayDecode)) {
        if (refused) {
          Refused_++;
        } else {
          Pending_++;
        }
        continue;
      }
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

bool OsmField::AddTile(TilePool &tiles, int tx, int ty, int &added, bool &refused, bool mayDecode) {
  const Data::Request request(
      Data::DataKind::VectorMap,
      Data::Address::Tile(Zoom_, static_cast<uint32_t>(tx), static_cast<uint32_t>(ty)));
  const TilePool::Reply reply = tiles.Bytes(request, &Scratch_);

  refused = reply == TilePool::Reply::Refused;
  if (reply == TilePool::Reply::Pending || refused) { return false; }

  if (reply == TilePool::Reply::Absent || reply == TilePool::Reply::Undeclared) { return true; }
  if (!mayDecode) { return false; }
  added += Accept(tx, ty, Scratch_.Bytes);
  return true;
}

int OsmField::Accept(int tx, int ty, std::span<const uint8_t> vectorTile) {
  int added = 0;
  Settle(tx, ty);
  const int got = static_cast<int>(vectorTile.size());

  const auto tile = static_cast<uint32_t>(Tiles_.size());
  Tiles_.push_back(Tile{.Z = Zoom_,
                        .X = tx,
                        .Y = ty,
                        .FirstFeature = static_cast<uint32_t>(Features_.size()),
                        .FeatureCount = 0});

  OsmVector mvt;
  for (uint16_t li = 0; li < static_cast<uint16_t>(Layers_.size()); li++) {
    bool present = false;
    if (!mvt.Parse(vectorTile.data(), static_cast<size_t>(got), Layers_[li].c_str(), &present)) {
      if (present) {
        Bad_++;
        Log::Error("world",
                   "vectile_undecodable",
                   {{"z", Zoom_}, {"x", tx}, {"y", ty}, {"bytes", got}, {"layer", Layers_[li]}});
      } else {
        Missing_++;
      }
      continue;
    }
    const auto ext = static_cast<double>(mvt.Extent());
    const std::vector<int32_t> &pts = mvt.Points();

    for (const OsmVector::Feature &sf : mvt.Features()) {
      Feature f{};
      f.Tile = tile;
      f.Layer = li;
      f.Type = static_cast<uint8_t>(sf.Type);
      f.FirstRing = static_cast<uint32_t>(Rings_.size());
      f.FirstTag = static_cast<uint32_t>(Tags_.size());
      f.MinLat = f.MinLon = kNoLeastYet;
      f.MaxLat = f.MaxLon = -kNoLeastYet;

      for (uint32_t r = 0; r < sf.RingCount; r++) {
        const OsmVector::Ring &sr = mvt.Rings()[sf.FirstRing + r];
        Ring ring{};
        ring.First = static_cast<uint32_t>(Points_.size() / 2);
        ring.Count = sr.Count;
        ring.Exterior = sr.Exterior;
        for (uint32_t k = 0; k < sr.Count; k++) {
          const auto px = static_cast<double>(pts[(static_cast<size_t>(sr.First) + k) * 2]);
          const auto py = static_cast<double>(pts[(static_cast<size_t>(sr.First) + k) * 2 + 1]);
          const Geo g = TileFracToGeo(
              Zoom_, static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), px / ext, py / ext);
          Points_.push_back(g.LatDeg);
          Points_.push_back(g.LonDeg);
          f.MinLat = std::min(f.MinLat, g.LatDeg);
          f.MaxLat = std::max(f.MaxLat, g.LatDeg);
          f.MinLon = std::min(f.MinLon, g.LonDeg);
          f.MaxLon = std::max(f.MaxLon, g.LonDeg);
        }
        Rings_.push_back(ring);
      }
      f.RingCount = static_cast<uint32_t>(Rings_.size()) - f.FirstRing;

      for (uint32_t t = 0; t < outshine::Ground::OsmVector::TagCount(sf); t++) {
        const OsmVector::Tag tag = mvt.TagAt(sf, t);
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
      added++;
    }
    Log::Debug("world",
               "vectile",
               {{"z", Zoom_},
                {"x", tx},
                {"y", ty},
                {"bytes", got},
                {"layer", Layers_[li]},
                {"parsed", true},
                {"feats", static_cast<int>(mvt.Features().size())},
                {"rings", static_cast<int>(mvt.Rings().size())}});
  }
  Tiles_[tile].FeatureCount = static_cast<uint32_t>(Features_.size()) - Tiles_[tile].FirstFeature;
  return added;
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

void OsmField::Declare(std::span<const Declared> these, double latDeg, double lonDeg) {
  const double turn = std::numbers::pi;
  const double side = std::ldexp(1.0, Zoom_);
  const double bent = latDeg * turn / kDegPerHalfTurn;
  Declare(these,
          static_cast<int>(std::floor((lonDeg + kDegPerHalfTurn) / kDegPerTurn * side)),
          static_cast<int>(std::floor(
              (1.0 - std::log(std::tan(bent) + 1.0 / std::cos(bent)) / turn) / 2.0 * side)));
}

void OsmField::Declare(std::span<const Declared> these, int tx, int ty) {
  Features_.clear();
  Rings_.clear();
  Points_.clear();
  Tiles_.clear();
  Tags_.clear();
  Settled_.clear();
  CentreX_ = tx;
  CentreY_ = ty;

  const auto number = [this](double how) {
    Values_.push_back(Value{.Num = how, .Str = 0, .IsNum = true});
    return static_cast<uint32_t>(Values_.size() - 1u);
  };
  const auto words = [this](std::string_view how) {
    Values_.push_back(
        Value{.Num = 0.0, .Str = Intern(Strings_, StringIndex_, how), .IsNum = false});
    return static_cast<uint32_t>(Values_.size() - 1u);
  };

  for (const Declared &one : these) {
    if (one.LatLon.size() < 4) { continue; }
    Feature made;
    made.FirstRing = static_cast<uint32_t>(Rings_.size());
    made.RingCount = 1;
    made.FirstTag = static_cast<uint32_t>(Tags_.size());
    made.Tile = 0;
    made.Layer = static_cast<uint16_t>(Layer(one.Layer.c_str()) < 0 ? 0 : Layer(one.Layer.c_str()));
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

  {
    uint64_t said = kGoldenWord;
    const auto stir = [&said](uint64_t by) { said = (said ^ by) * kStirPrime; };
    stir(static_cast<uint64_t>(static_cast<uint32_t>(tx)));
    stir(static_cast<uint64_t>(static_cast<uint32_t>(ty)));
    for (const Declared &one : these) {
      stir(std::hash<std::string>{}(one.Value));
      stir(std::hash<std::string>{}(one.Key));
      stir(std::hash<std::string>{}(one.Layer));
      stir(static_cast<uint64_t>(std::llround(one.WidthM * kFixedPointPerM)));
      stir(static_cast<uint64_t>(std::llround(one.HeightM * kFixedPointPerM)));
      stir(static_cast<uint64_t>(one.Area ? 1 : 0) | static_cast<uint64_t>(one.Bridge ? 2 : 0) |
           static_cast<uint64_t>(one.Tunnel ? 4 : 0));
      stir(static_cast<uint64_t>(static_cast<int64_t>(one.Level)));
      for (const double held : one.LatLon) {
        stir(static_cast<uint64_t>(std::llround(held * kFixedPointPerDeg)));
      }
    }
    if (said != Said_) {
      Said_ = said;
      ++Generation_;
    }
  }
  Tiles_.push_back(Tile{.Z = Zoom_,
                        .X = tx,
                        .Y = ty,
                        .FirstFeature = 0,
                        .FeatureCount = static_cast<uint32_t>(Features_.size())});
  Settle(tx, ty);
  Pending_ = 0;
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

} // namespace outshine::Ground
