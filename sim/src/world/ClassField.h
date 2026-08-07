/* THE GROUND CLASS AS A WORLD-ANCHORED FIELD OF COVERAGE WEIGHTS, rasterised from the OSM vectors.
 *
 * WHY WEIGHTS AND NOT AN INDEX. A class index is nominal: the mean of "meadow" and "asphalt" is not a
 * class, so an index may not be filtered, so it has no resolution ladder — a coarse level would be a
 * different answer rather than a summary. A COVERAGE FRACTION is a quantity. It may be averaged, and
 * a coarser level of it is a correct aggregation of the finer one. That single change is what makes
 * the ladder below legitimate, and it is also what puts the class boundary INSIDE a texel: a texel
 * that a straight edge crosses at 30 % carries 0.30/0.70, so the edge is reconstructed to a fraction
 * of a texel from a NEAREST read. The fray is therefore one texel wide and it is centimetres.
 *
 * WHY IT CANNOT LIVE ON THE TILES. The per-tile class array is indexed by tile slot and its ground
 * resolution is the tile span over its texel count — 2.9342 m at the terrain tree's finest cut. A
 * 5.5 m street can never be free of its neighbours there. This field is anchored to the WORLD and
 * knows nothing about the terrain quadtree, its zoom range or its tiling: its lattice is an ENU grid
 * in metres at a fixed origin, its levels are metres, and the only thing it takes from the tile
 * server is the vector geometry itself.
 *
 * THE LADDER. kLevels levels, kSide x kSide texels each, texel = kTexel0M * 2^level, every level
 * centred on the camera and scrolled in WHOLE TEXELS, so a texel keeps its world position for as long
 * as it is resident and refilling is an edge strip and never the whole window.
 *
 * FOUR CHANNELS IS A DECLARED BOUND, not a natural number: `land` alone carries 15 kinds and the whole
 * table 99 rows, so a texel may hold more classes than fit. The measured overlap of `land` with itself
 * is 2.41 % of the reference tile over nine pairs, so a texel with more than three classes in it is
 * rare; the tail beyond the four largest is dropped and the four are renormalised. */
#ifndef CLASSFIELD_H
#define CLASSFIELD_H

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "OsmField.h"

namespace outshine::World {

class VegetationTemplates;

class ClassField {
public:
  static constexpr int kSide = 512;
  static constexpr int kLevels = 8;
  static constexpr int kChannels = 4;
  /* Sub-texels per texel per axis for the coverage measurement: 16 subcells give the weight a
   * quantisation of 1/16 texel, i.e. 1.6 cm at level 0 — an order under the 14.0 cm mean residual the
   * vector source itself carries against raw OSM. */
  static constexpr int kSub = 4;
  /* [SET] 0.25 m, BRACKETED by the source rather than chosen: the vector tiles quantise to 0.3668 m
   * at this latitude and their mean residual against raw OSM is 0.140 m. A lattice coarser than
   * 0.3668 m throws away structure the source has; one finer than 0.140 m resamples noise. 0.25 m
   * sits between them and is a round number of METRES, which an ENU lattice should be — it is not a
   * subdivision of any tile. */
  static constexpr double kTexel0M = 0.25;

  /* A rectangle of one level, in TOROIDAL texel coordinates, whose weights changed this pass. */
  struct Chunk { int Level, X, Y, W, H; };

  explicit ClassField(const VegetationTemplates *veg) : Veg_(veg) {}

  void Open(double lat, double lon);
  /* One budgeted pass: at most one vector tile per source, then at most kTexelBudget texels of
   * refill. Fills Written() with what has to reach the GPU. */
  void Update(double camLat, double camLon);

  const std::vector<Chunk> &Written() const { return Written_; }
  /* The whole level, kSide*kSide*kChannels bytes; a Chunk indexes into it at (Y*kSide + X)*kChannels. */
  const uint8_t *Ids(int level) const { return Levels_[level].Ids.data(); }
  const uint8_t *Weights(int level) const { return Levels_[level].Wts.data(); }

  /* Every level filled and no vector tile still out. The frame oracle waits on this. */
  bool Complete() const;
  int PendingTiles() const { return Near_.PendingTiles() + Far_.PendingTiles(); }

  /* The class with the largest coverage at a world point on one level, or -1 outside the level's
   * window. THE CACHE IS DERIVED: two levels must answer the same away from a boundary, and that is
   * the check, not an assumption. */
  int DominantAt(int level, double lat, double lon) const;
  bool WeightsAt(int level, double lat, double lon, uint8_t ids[kChannels],
                 uint8_t wts[kChannels]) const;

  /* The shader needs the camera's own position on each level's lattice, in texels, measured from that
   * level's window origin — everything else it derives from the ECEF offset it already has. */
  void WindowFrac(float out[kLevels * 2]) const;
  const double *OriginEcef() const { return O_; }
  const double *EastEcef() const { return East_; }
  const double *NorthEcef() const { return North_; }

  /* Three outcomes, three counters. NoData is the truth about OSM and not a fault; UnknownKinds is
   * the one that hid 81 barrier ways; BadTiles is a broken fetch or an unusable geometry. */
  double NoDataFraction() const { return SubTotal_ ? (double)SubNoData_ / (double)SubTotal_ : 0.0; }
  long UnknownKinds() const { return (long)Unknown_.size(); }
  long UnknownFeatures() const { return UnknownFeats_; }
  long RasterTexels() const { return Texels_; }
  double RasterMs() const { return RasterMs_; }

private:
  struct Feat {
    uint32_t Idx;
    int Rank;
    uint16_t Tpl;
    uint8_t Type;
    float WidthM;
    float MinE, MinN, MaxE, MaxN;
  };
  struct Source {
    OsmField Field;
    int Ring;
    std::vector<float> Pts;    /* (e,n) metres, parallel to Field.Points() pairs */
    size_t PtsDone = 0;
    std::vector<Feat> Feats;   /* rank ascending: the painter's order, declared and not inherited */
    size_t FeatsDone = 0;
    Source(int zoom, std::initializer_list<const char *> layers, int ring)
        : Field(zoom, layers), Ring(ring) {}
  };
  struct Rect { int I0, J0, I1, J1; };
  struct Level {
    int OriginI = 0, OriginJ = 0;
    bool Have = false;
    std::vector<uint8_t> Ids, Wts;
    std::vector<Rect> Dirty;
  };

  void Project(double lat, double lon, double *e, double *n) const;
  void Ingest(Source &src);
  void PushDirty(int level, double minE, double minN, double maxE, double maxN);
  void Scroll(int level, double camE, double camN);
  void Raster(int level, const Rect &r);
  void PaintRing(const float *pts, uint32_t first, uint32_t count, uint8_t v);
  void PaintQuad(const float q[8], uint8_t v);
  void Flush(uint8_t v);
  static const Source &SourceFor(const ClassField &f, int level) {
    return level < kNearLevels ? f.Near_ : f.Far_;
  }

  /* Levels 0..4 (texel <= 4 m, half-window <= 1024 m) come off the z14 tiles, whose 3x3 block
   * guarantees 1502.33 m in every direction. Levels 5..7 (half-window <= 8192 m) come off z11, whose
   * 3x3 block guarantees 12018.6 m. Both are properties of the VECTOR FETCH and of nothing else. */
  static constexpr int kNearLevels = 5;
  static constexpr int kTexelBudget = 1 << 17;
  static constexpr int kChunkSide = 128;
  static constexpr int kChunkTexels = 1 << 14;

  const VegetationTemplates *Veg_;
  Source Near_{14, {"land", "streets", "water_polygons", "water_lines", "sites", "street_polygons",
                    "buildings"}, 1};
  Source Far_{11, {"land", "streets", "water_polygons", "water_lines"}, 1};
  Level Levels_[kLevels];
  std::vector<Chunk> Written_;

  /* The chunk being rasterised: sub-cell ids, and the scanline scratch that fills them. */
  std::vector<uint8_t> Sub_;
  std::vector<float> Edges_;       /* x0,y0,x1,y1 per edge, sub-cell units */
  std::vector<float> Xs_;
  std::vector<int> Dirs_;
  int SubW_ = 0, SubH_ = 0;
  double SubOrgE_ = 0.0, SubOrgN_ = 0.0, SubStep_ = 1.0;

  double O_[3] = {0, 0, 0}, East_[3] = {1, 0, 0}, North_[3] = {0, 1, 0};
  double CamE_ = 0.0, CamN_ = 0.0;
  bool Opened_ = false;

  std::unordered_set<std::string> Unknown_;
  long UnknownFeats_ = 0;
  long SubNoData_ = 0, SubTotal_ = 0, Texels_ = 0;
  double RasterMs_ = 0.0;
};

} // namespace outshine::World
#endif
