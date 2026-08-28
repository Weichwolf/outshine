#ifndef OUTSHINE_WORLD_GROUND_OSMFIELD_H
#define OUTSHINE_WORLD_GROUND_OSMFIELD_H

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "OsmLayer.h"
#include "Span.h"
#include "TilePool.h"

namespace outshine::Ground {

class OsmField {
public:
  struct Ring {
    uint32_t First = 0, Count = 0;
    bool Exterior = true;
  };
  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    uint32_t FirstTag = 0, TagCount = 0;
    uint32_t Tile = 0;
    uint16_t Layer = 0;
    uint8_t Type = 0;
    double MinLat = 0, MinLon = 0, MaxLat = 0, MaxLon = 0;
  };
  struct Tile {
    int Z = 0, X = 0, Y = 0;
    uint32_t FirstFeature = 0, FeatureCount = 0;
  };

  OsmField(int zoom, std::span<const std::string> layers);

  [[nodiscard]] int Build(TilePool &tiles, double lat, double lon, int ringTiles);

  [[nodiscard]] int Accept(int tx, int ty, std::span<const uint8_t> vectorTile);

  [[nodiscard]] int Zoom() const { return Zoom_; }

  [[nodiscard]] long MissingLayers() const { return Missing_; }
  [[nodiscard]] long BadTiles() const { return Bad_; }

  [[nodiscard]] int PendingTiles() const { return Pending_; }
  [[nodiscard]] int RefusedTiles() const { return Refused_; }

  [[nodiscard]] bool Settled(int x, int y) const;

  [[nodiscard]] int TileIndex(int x, int y) const;

  [[nodiscard]] std::span<const Feature> OfTile(int index) const;

  [[nodiscard]] std::span<const Feature> Features() const { return Features_; }
  [[nodiscard]] std::span<const Ring> Rings() const { return Rings_; }
  [[nodiscard]] std::span<const double> Points() const { return Points_; }
  [[nodiscard]] std::span<const Tile> Tiles() const { return Tiles_; }

  [[nodiscard]] size_t KeyCount() const { return Keys_.size(); }
  [[nodiscard]] std::string_view KeyAt(size_t at) const { return Keys_[at]; }

  [[nodiscard]] size_t HeapBytes() const;

  [[nodiscard]] int Layer(const char *name) const;
  [[nodiscard]] int Layer(OsmLayer layer) const { return Layer(OsmLayerName(layer)); }
  [[nodiscard]] std::string_view LayerName(int i) const { return Layers_[(size_t)i]; }

  [[nodiscard]] double Num(const Feature &f, const char *key, double def) const;

  [[nodiscard]] std::string_view Str(const Feature &f, const char *key) const;

private:
  struct Value {
    double Num = 0.0;
    uint32_t Str = 0;
    bool IsNum = false;
  };

  uint32_t Intern(std::vector<std::string> &pool, std::unordered_map<std::string, uint32_t> &index,
                  std::string_view s);
  [[nodiscard]] bool AddTile(TilePool &tiles, int tx, int ty, int &added, bool &refused);
  void Settle(int x, int y);

  std::vector<std::string> Layers_;
  std::vector<Feature> Features_;
  std::vector<Ring> Rings_;
  std::vector<double> Points_;
  std::vector<Tile> Tiles_;
  std::vector<uint32_t> Tags_;
  std::vector<std::string> Keys_;
  std::vector<std::string> Strings_;
  std::vector<Value> Values_;
  std::unordered_map<std::string, uint32_t> KeyIndex_, StringIndex_;
  std::vector<uint64_t> Settled_;
  TilePool::Landing Scratch_;
  int Zoom_;
  int Pending_ = -1;
  int Refused_ = 0;
  long Missing_ = 0, Bad_ = 0;
};

}
#endif
