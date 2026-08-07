#include "OsmField.h"

#include "Log.h"
#include "OsmVector.h"
#include "TerrainLoader.h"
#include "geo.h"

#include <algorithm>

namespace outshine::World {

namespace {

constexpr int kMaxTileBytes = 4 << 20;

uint64_t TileKey(int x, int y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; }

}  // namespace

OsmField::OsmField(int zoom, std::initializer_list<const char *> layers) : Zoom_(zoom) {
  Layers_.reserve(layers.size());
  for (const char *l : layers) Layers_.emplace_back(l);
}

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

int OsmField::Build(double lat, double lon, int ringTiles) {
  uint32_t cx = 0, cy = 0;
  osmmesh_geo_to_tile(lon, lat, (uint8_t)Zoom_, &cx, &cy);
  if (Scratch_.empty()) Scratch_.resize((size_t)kMaxTileBytes);

  const long n = 1L << Zoom_;
  int added = 0;
  bool decoded = false;
  Pending_ = 0;

  for (int dy = -ringTiles; dy <= ringTiles; dy++)
    for (int dx = -ringTiles; dx <= ringTiles; dx++) {
      const long tx = (long)cx + dx, ty = (long)cy + dy;
      if (tx < 0 || ty < 0 || tx >= n || ty >= n) continue;
      const uint64_t key = TileKey((int)tx, (int)ty);
      if (std::find(Done_.begin(), Done_.end(), key) != Done_.end()) continue;
      /* The scan RUNS ON past the one tile this pass decodes, because a caller that has to know the
       * block is complete needs the count of what is still out, not the first miss. */
      if (decoded) { Pending_++; continue; }
      if (!AddTile((int)tx, (int)ty, added)) { Pending_++; continue; }
      Done_.push_back(key);
      decoded = true;
    }

  return added;
}

bool OsmField::AddTile(int tx, int ty, int &added) {
  const int got = fb_stream_vector(Zoom_, (uint32_t)tx, (uint32_t)ty, Scratch_.data(), kMaxTileBytes);
  if (got == 0) return false;   /* pending — retry next pass, do NOT mark done */

  const uint32_t tile = (uint32_t)Tiles_.size();
  Tiles_.push_back(Tile{Zoom_, tx, ty});

  OsmVector mvt;
  for (uint16_t li = 0; li < (uint16_t)Layers_.size(); li++) {
    if (got < 0 || !mvt.Parse(Scratch_.data(), (size_t)got, Layers_[li].c_str())) {
      Log::Debug("world", "vectile", {{"z", Zoom_}, {"x", tx}, {"y", ty}, {"bytes", got},
                                     {"layer", Layers_[li]}, {"parsed", false}});
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
          const osmmesh_geo g = osmmesh_tile_frac_to_geo((uint8_t)Zoom_, (uint32_t)tx, (uint32_t)ty,
                                                         px / ext, py / ext);
          Points_.push_back(g.lat);
          Points_.push_back(g.lon);
          f.MinLat = std::min(f.MinLat, g.lat);
          f.MaxLat = std::max(f.MaxLat, g.lat);
          f.MinLon = std::min(f.MinLon, g.lon);
          f.MaxLon = std::max(f.MaxLon, g.lon);
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
  return true;
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

} // namespace outshine::World
