#include "OsmField.h"

#include "Capacity.h"
#include "Log.h"
#include "OsmVector.h"
#include "TerrainLoader.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <cstdio>

namespace outshine::Ground {

namespace {

uint64_t TileKey(int x, int y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; }

}

OsmField::OsmField(int zoom, std::span<const std::string> layers)
    : Layers_(layers.begin(), layers.end()), Zoom_(zoom) {}

uint32_t OsmField::Intern(std::vector<std::string> &pool,
                          std::unordered_map<std::string, uint32_t> &index, std::string_view s) {
  const std::string key(s);
  auto it = index.find(key);
  if (it != index.end()) return it->second;
  const uint32_t id = (uint32_t)pool.size();
  pool.push_back(key);
  index.emplace(key, id);
  return id;
}

int OsmField::Build(TilePool &tiles, double lat, double lon, int ringTiles) {
  uint32_t cx = 0, cy = 0;
  Pending_ = 0;

  Geo centre;
  centre.LonDeg = lon;
  centre.LatDeg = lat;
  if (!TileIndex::Of(centre, Zoom_).TryXy(&cx, &cy)) return 0;

  const long n = 1L << Zoom_;
  int added = 0;
  bool decoded = false;

  for (int dy = -ringTiles; dy <= ringTiles; dy++)
    for (int dx = -ringTiles; dx <= ringTiles; dx++) {
      const long tx = (long)cx + dx, ty = (long)cy + dy;
      if (tx < 0 || ty < 0 || tx >= n || ty >= n) continue;
      const uint64_t key = TileKey((int)tx, (int)ty);
      if (std::find(Settled_.begin(), Settled_.end(), key) != Settled_.end()) continue;

      if (decoded) { Pending_++; continue; }
      if (!AddTile(tiles, (int)tx, (int)ty, added)) { Pending_++; continue; }
      Settle((int)tx, (int)ty);
      decoded = true;
    }

  return added;
}

bool OsmField::Settled(int x, int y) const {
  return std::find(Settled_.begin(), Settled_.end(), TileKey(x, y)) != Settled_.end();
}

void OsmField::Settle(int x, int y) {
  const uint64_t key = TileKey(x, y);
  if (std::find(Settled_.begin(), Settled_.end(), key) == Settled_.end()) {
    Settled_.push_back(key);
  }
}

int OsmField::TileIndex(int x, int y) const {
  for (size_t i = 0; i < Tiles_.size(); i++)
    if (Tiles_[i].X == x && Tiles_[i].Y == y) return (int)i;
  return -1;
}

std::span<const OsmField::Feature> OsmField::OfTile(int index) const {
  if (index < 0 || (size_t)index >= Tiles_.size()) return {};
  const Tile &t = Tiles_[(size_t)index];
  return std::span<const Feature>(Features_.data() + t.FirstFeature, t.FeatureCount);
}

bool OsmField::AddTile(TilePool &tiles, int tx, int ty, int &added) {
  const Data::Request request(Data::DataKind::VectorMap,
                              Data::Address::Tile(Zoom_, (uint32_t)tx, (uint32_t)ty));
  const TilePool::Reply reply = tiles.Bytes(request, &Scratch_);

  if (reply == TilePool::Reply::Pending || reply == TilePool::Reply::Refused) return false;

  if (reply == TilePool::Reply::Absent || reply == TilePool::Reply::Undeclared) return true;
  added += Accept(tx, ty, Scratch_.Bytes);
  return true;
}

int OsmField::Accept(int tx, int ty, std::span<const uint8_t> vectorTile) {
  int added = 0;
  Settle(tx, ty);
  const int got = (int)vectorTile.size();

  const uint32_t tile = (uint32_t)Tiles_.size();
  Tiles_.push_back(Tile{Zoom_, tx, ty, (uint32_t)Features_.size(), 0});

  OsmVector mvt;
  for (uint16_t li = 0; li < (uint16_t)Layers_.size(); li++) {
    bool present = false;
    if (!mvt.Parse(vectorTile.data(), (size_t)got, Layers_[li].c_str(), &present)) {
      if (present) {
        Bad_++;
        Log::Error("world", "vectile_undecodable", {{"z", Zoom_}, {"x", tx}, {"y", ty},
                                                   {"bytes", got}, {"layer", Layers_[li]}});
      } else {
        Missing_++;
      }
      continue;
    }
    const double ext = (double)mvt.Extent();
    const std::vector<int32_t> &pts = mvt.Points();

    for (const OsmVector::Feature &sf : mvt.Features()) {
      Feature f{};
      f.Tile = tile;
      f.Layer = li;
      f.Type = (uint8_t)sf.Type;
      f.FirstRing = (uint32_t)Rings_.size();
      f.FirstTag = (uint32_t)Tags_.size();
      f.MinLat = f.MinLon = 1e9;
      f.MaxLat = f.MaxLon = -1e9;

      for (uint32_t r = 0; r < sf.RingCount; r++) {
        const OsmVector::Ring &sr = mvt.Rings()[sf.FirstRing + r];
        Ring ring{};
        ring.First = (uint32_t)(Points_.size() / 2);
        ring.Count = sr.Count;
        ring.Exterior = sr.Exterior;
        for (uint32_t k = 0; k < sr.Count; k++) {
          const double px = (double)pts[((size_t)sr.First + k) * 2];
          const double py = (double)pts[((size_t)sr.First + k) * 2 + 1];
          const Geo g = TileFracToGeo(Zoom_, (uint32_t)tx, (uint32_t)ty, px / ext, py / ext);
          Points_.push_back(g.LatDeg);
          Points_.push_back(g.LonDeg);
          f.MinLat = std::min(f.MinLat, g.LatDeg);
          f.MaxLat = std::max(f.MaxLat, g.LatDeg);
          f.MinLon = std::min(f.MinLon, g.LonDeg);
          f.MaxLon = std::max(f.MaxLon, g.LonDeg);
        }
        Rings_.push_back(ring);
      }
      f.RingCount = (uint32_t)Rings_.size() - f.FirstRing;

      for (uint32_t t = 0; t < mvt.TagCount(sf); t++) {
        const OsmVector::Tag tag = mvt.TagAt(sf, t);
        if (tag.Key.empty()) continue;
        Value v{};
        v.IsNum = tag.IsNum;
        if (tag.IsNum) v.Num = tag.Num; else v.Str = Intern(Strings_, StringIndex_, tag.Str);
        Tags_.push_back(Intern(Keys_, KeyIndex_, tag.Key));
        Tags_.push_back((uint32_t)Values_.size());
        Values_.push_back(v);
      }
      f.TagCount = (uint32_t)Tags_.size() - f.FirstTag;

      Features_.push_back(f);
      added++;
    }
    Log::Debug("world", "vectile", {{"z", Zoom_}, {"x", tx}, {"y", ty}, {"bytes", got},
                                   {"layer", Layers_[li]}, {"parsed", true},
                                   {"feats", (int)mvt.Features().size()},
                                   {"rings", (int)mvt.Rings().size()}});
  }
  Tiles_[tile].FeatureCount = (uint32_t)Features_.size() - Tiles_[tile].FirstFeature;
  return added;
}

size_t OsmField::HeapBytes() const {
  size_t strings = 0;
  for (const std::string &s : Keys_) strings += s.capacity();
  for (const std::string &s : Strings_) strings += s.capacity();
  for (const std::string &s : Layers_) strings += s.capacity();

  const size_t nodes = (KeyIndex_.size() + StringIndex_.size()) *
                       (sizeof(std::string) + sizeof(uint32_t) + 2 * sizeof(void *));
  return CapacityBytes(Features_) + CapacityBytes(Rings_) + CapacityBytes(Points_) +
         CapacityBytes(Tiles_) + CapacityBytes(Tags_) + CapacityBytes(Values_) +
         CapacityBytes(Settled_) + CapacityBytes(Scratch_.Bytes) + CapacityBytes(Keys_) +
         CapacityBytes(Strings_) + CapacityBytes(Layers_) + strings + nodes;
}

int OsmField::Layer(const char *name) const {
  for (size_t i = 0; i < Layers_.size(); i++)
    if (Layers_[i] == name) return (int)i;
  return -1;
}

double OsmField::Num(const Feature &f, const char *key, double def) const {
  for (uint32_t i = 0; i + 1 < f.TagCount; i += 2) {
    const uint32_t k = Tags_[f.FirstTag + i], v = Tags_[f.FirstTag + i + 1];
    if (Keys_[k] == key && Values_[v].IsNum) return Values_[v].Num;
  }
  return def;
}

std::string_view OsmField::Str(const Feature &f, const char *key) const {
  for (uint32_t i = 0; i + 1 < f.TagCount; i += 2) {
    const uint32_t k = Tags_[f.FirstTag + i], v = Tags_[f.FirstTag + i + 1];
    if (Keys_[k] == key && !Values_[v].IsNum) return Strings_[Values_[v].Str];
  }
  return {};
}

}
