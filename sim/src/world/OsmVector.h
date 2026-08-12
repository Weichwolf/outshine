/* A read-only Mapbox-Vector-Tile reader over a borrowed byte range — layers, features, tags, rings.
 * Stdlib only: an MVT is protobuf varints and a six-command geometry stream, not a reason to take a
 * dependency, and the tile server already speaks it (/t/vector/{z}/{x}/{y}).
 *
 * Rings come out in TILE-LOCAL EXTENT UNITS and nothing here knows about the earth: the projection is
 * the caller's, because the same rings feed buildings (ECEF prisms), landcover (a raster fill) and the
 * map sheet (screen space), and each wants a different one. */
#ifndef OSMVECTOR_H
#define OSMVECTOR_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::World {

class OsmVector {
public:
  struct Ring {
    uint32_t First = 0, Count = 0;   /* into Points() as x,y pairs */
    bool Exterior = true;            /* MVT winding: clockwise in y-down tile space */
  };
  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    uint32_t FirstTag = 0, TagCount = 0;   /* into Tags(), key/value index pairs */
    int Type = 0;                          /* 1 point, 2 line, 3 polygon */
  };

  /* `name` selects ONE layer; everything else in the tile is skipped without being decoded.
   * `present` separates the two things a bare false conflates: a tile that simply has no such layer
   * (Manhattan has no `water_lines`, and 13 of the schema's layers are absent from the demo tile) and
   * bytes that would not decode. One is a counter, the other is an error. */
  [[nodiscard]] bool Parse(const uint8_t *bytes, size_t len, const char *layer, bool *present = nullptr);

  int Extent() const { return Extent_; }
  const std::vector<Feature> &Features() const { return Features_; }
  const std::vector<Ring> &Rings() const { return Rings_; }
  const std::vector<int32_t> &Points() const { return Points_; }

  /* The value of `key` on `f`, or `def`/empty. The two are disjoint: a tag decoded as a string never
   * answers Num and a tag decoded as a number never answers Str, so a caller cannot silently read 0
   * off "forest". The view points into this reader and dies with the next Parse(). */
  double Num(const Feature &f, const char *key, double def) const;
  std::string_view Str(const Feature &f, const char *key) const;

  /* Enumeration, for a consumer that copies tags it does not know the names of. */
  struct Tag {
    std::string_view Key, Str;
    double Num = 0.0;
    bool IsNum = false;
  };
  uint32_t TagCount(const Feature &f) const { return f.TagCount / 2; }
  Tag TagAt(const Feature &f, uint32_t i) const;

private:
  int Extent_ = 4096;
  std::vector<Feature> Features_;
  std::vector<Ring> Rings_;
  std::vector<int32_t> Points_;
  std::vector<uint32_t> Tags_;
  std::vector<std::string> Keys_;
  std::vector<double> Values_;
  std::vector<std::string> ValueStrs_;
  std::vector<bool> ValueIsNum_;
};

} // namespace outshine::World
#endif
