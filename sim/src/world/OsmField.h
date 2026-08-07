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
  };

  explicit OsmField(std::initializer_list<const char *> layers);

  /* AT MOST ONE TILE PER CALL. Everything downstream of a decoded tile is main-thread work in the
   * frame that asked; a ring of nine landing together is one frame doing nine tiles' worth of it.
   * Returns the number of features added, 0 when nothing was decoded. Idempotent per tile. */
  int Build(double lat, double lon, int ringTiles);

  int Zoom() const { return Zoom_; }
  /* Tiles of the block the last Build() left undecoded. 0 = complete, -1 = never asked. */
  int PendingTiles() const { return Pending_; }

  const std::vector<Feature> &Features() const { return Features_; }
  const std::vector<Ring> &Rings() const { return Rings_; }
  const std::vector<double> &Points() const { return Points_; }
  const std::vector<Tile> &Tiles() const { return Tiles_; }

  /* -1 when the layer was never asked for. Resolve once, then compare Feature::Layer. */
  int Layer(const char *name) const;

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
  bool AddTile(int tx, int ty, int &added);

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
  std::vector<uint8_t> Scratch_;   /* the tile buffer, once — not once per frame */
  int Zoom_ = 0;
  int Pending_ = -1;
};

} // namespace outshine::World
#endif
