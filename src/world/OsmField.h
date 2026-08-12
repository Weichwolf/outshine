/* The decoded OSM vector geometry of the world, in geodetic coordinates, across tile seams.
 *
 * OsmVector reads ONE tile into ONE layer's tile-local extent units and forgets it again. OsmField is
 * the other half: it streams the tiles, converts every ring to lat/lon once, and keeps them, so that
 * the extruded footprint, the ground class at a point and the kerb drawn along a street are three
 * READERS of one geometry instead of three parses that can disagree about where the street is.
 *
 * WHY LAT/LON AND NOT A LOCAL METRIC FRAME. Anything anchored drifts: a pedestrian walking out of the
 * anchor's neighbourhood forces a re-anchor, and a re-anchor rewrites every stored point. Geodetic
 * coordinates have no origin to lose. The cost is that a consumer wanting metres converts — which the
 * consumers do anyway, each into its own frame (ECEF prisms, ENU metres, screen space).
 *
 * WHAT A SEAM STILL COSTS. MVT clips every feature at the tile border, so a forest crossing a seam
 * arrives as two polygons sharing an artificial edge. Their UNION is right, which is all a
 * point-in-polygon query needs; an outline generator that follows a ring will see that edge as real. */
#ifndef OSMFIELD_H
#define OSMFIELD_H

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "OsmLayer.h"
#include "Span.h"
#include "TilePool.h"

namespace outshine::World {

class OsmField {
public:
  struct Ring {
    uint32_t First = 0, Count = 0;   /* into Points() as lat/lon pairs, ring not closed */
    bool Exterior = true;
  };
  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    uint32_t FirstTag = 0, TagCount = 0;   /* into Tags(), key/value index pairs */
    uint32_t Tile = 0;                     /* into Tiles(); features of one tile are contiguous */
    uint16_t Layer = 0;
    uint8_t Type = 0;                      /* 1 point, 2 line, 3 polygon */
    double MinLat = 0, MinLon = 0, MaxLat = 0, MaxLon = 0;
  };
  struct Tile {
    int Z = 0, X = 0, Y = 0;
    uint32_t FirstFeature = 0, FeatureCount = 0;
  };

  /* `zoom` is the VECTOR SOURCE's own maxzoom for this field, and it belongs here rather than to
   * any consumer: a coarse field over a wide area and a fine one over a narrow one are the same class
   * reading two generalisations of one dataset. */
  OsmField(int zoom, const std::vector<std::string> &layers);

  /* AT MOST ONE TILE PER CALL. Everything downstream of a decoded tile is main-thread work in the
   * frame that asked; a ring of nine landing together is one frame doing nine tiles' worth of it.
   * Returns the number of features added, 0 when nothing was decoded. Idempotent per tile. */
  int Build(double lat, double lon, int ringTiles);

  int Zoom() const { return Zoom_; }
  /* A layer this tile stream simply does not carry is normal and counted; bytes that would not decode
   * are an error and logged with the tile. */
  long MissingLayers() const { return Missing_; }
  long BadTiles() const { return Bad_; }
  /* Tiles of the block the last Build() left undecoded. 0 = complete, -1 = never asked. */
  int PendingTiles() const { return Pending_; }
  /* Whether this one tile's vectors have been decoded and kept. A consumer that works a tile at a
   * time asks about ITS tile: the block's pending count is about the camera's neighbourhood and says
   * nothing about a tile out at the edge of a ring. */
  [[nodiscard]] bool Decoded(int x, int y) const;
  /* WHICH TILE THIS IS, or none. A tile the provider answered "absent" for is decoded and carries no
   * geometry, so "no index" and "not decoded" are two different answers and Decoded() is the one that
   * separates them. */
  int TileIndex(int x, int y) const;
  /* ONE TILE'S FEATURES, AS A SLICE. They are appended in tile order and never reordered, so the
   * range exists by construction and a consumer working one tile at a time never scans the field.
   * The span is good until the next Build(). */
  Span<const Feature> OfTile(int index) const;

  const std::vector<Feature> &Features() const { return Features_; }
  const std::vector<Ring> &Rings() const { return Rings_; }
  const std::vector<double> &Points() const { return Points_; }
  const std::vector<Tile> &Tiles() const { return Tiles_; }

  /* Everything the decoded geometry and its string pools hold. It only grows: a tile decoded once is
   * kept, which is what makes a seam-crossing feature one geometry rather than two parses. */
  size_t HeapBytes() const;

  /* -1 when the layer was never asked for. Resolve once, then compare Feature::Layer. */
  int Layer(const char *name) const;
  int Layer(OsmLayer layer) const { return Layer(OsmLayerName(layer)); }
  const std::string &LayerName(int i) const { return Layers_[(size_t)i]; }

  double Num(const Feature &f, const char *key, double def) const;
  /* Empty when absent or numeric. Points into this field and survives until it is destroyed —
   * Build() only appends, so a view taken from an earlier tile stays good. */
  std::string_view Str(const Feature &f, const char *key) const;

private:
  struct Value {
    double Num = 0.0;
    uint32_t Str = 0;
    bool IsNum = false;
  };

  uint32_t Intern(std::vector<std::string> &pool, std::unordered_map<std::string, uint32_t> &index,
                  std::string_view s);
  [[nodiscard]] bool AddTile(int tx, int ty, int &added);

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
  std::vector<uint64_t> Done_;
  TilePool::Landing Scratch_;   /* the tile buffer, once — not once per frame */
  int Zoom_;
  int Pending_ = -1;
  long Missing_ = 0, Bad_ = 0;
};

} // namespace outshine::World
#endif
