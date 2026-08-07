/* THE GROUND CLASS, EVALUATED FROM THE VECTORS THEMSELVES — no class raster anywhere, on either side.
 *
 * WHY NO RASTER. A raster has a resolution, and a resolution is what made the class a function of the
 * viewer three times running: the mip level, the tile zoom, the consumer's own lattice. Remove the
 * raster and that whole failure class is gone STRUCTURALLY rather than forbidden by a rule. What
 * replaces it is the technique glyph rasterisers use: the outlines live in a storage buffer and every
 * fragment evaluates them analytically, so the answer is resolution-free and the boundary is exact.
 *
 * MEASURED, over the 3x3 z14 block at the reference standpoint (20.3 km^2, 2 246 area features,
 * 25 501 edges after every line is widened to its declared metre width): with a 16 m acceleration
 * cell a fragment sees a MEAN of 3.87 edges and 2.11 features, p99 16 edges, worst cell 45 edges and
 * 14 features. That is the number the whole approach stands on, and it is why no fragment ever tests
 * against everything.
 *
 * THE ACCELERATION STRUCTURE IS A GRID, and it carries exactly two things per cell:
 *   base   — the winning class of every feature that covers the cell WITHOUT a boundary in it, so the
 *            common case costs one byte and no geometry at all;
 *   seeds  — for each feature that does have a boundary in the cell, its winding number at the cell's
 *            south-west corner plus that cell's edges of it. A fragment walks corner -> (px, cy) -> p,
 *            two axis-aligned legs that cannot leave the cell, so only this cell's edges can cross
 *            them and the winding is exact.
 *
 * THE ORDER IS DECLARED, NOT INHERITED. Features are laid down by the `rank` on their row in
 * vegetation.json; the tile server has none inside a layer and relies on the provider's emission
 * order over features that overlap by 2.41 % of the tile.
 *
 * TWO TIERS, and the split is a property of the VECTOR FETCH and of nothing else: z14 within 1024 m
 * (a 3x3 block guarantees 1502.33 m) and z11 within 8192 m (a 3x3 block guarantees 12018.6 m). Beyond
 * that there is no datum and the declared default is the honest answer. */
#ifndef CLASSFIELD_H
#define CLASSFIELD_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "OsmField.h"

namespace outshine::World {

class VegetationTemplates;

class ClassField {
public:
  void SetVegetation(const VegetationTemplates *veg) { Veg_ = veg; }
  void Open(double lat, double lon);
  /* One budgeted pass: at most one vector tile per tier, and at most one tier rebuilt. */
  void Update(double camLat, double camLon);

  /* The whole structure, ready for a storage buffer. Dirty() falls to false once the caller has
   * uploaded it. */
  const uint32_t *Buffer() const { return Buf_.data(); }
  size_t BufferBytes() const { return Buf_.size() * sizeof(uint32_t); }
  bool Dirty() const { return Dirty_; }
  void ClearDirty() { Dirty_ = false; }

  /* The ECEF frame the buffer's metres are measured in. The fragment projects its own camera-relative
   * offset on these very axes, so CPU and GPU place a world point identically by construction. */
  const double *OriginEcef() const { return O_; }
  /* The camera's own place in this frame, the offset a fragment adds to its camera-relative one. */
  const double *Cam() const { return Cam_; }
  const double *EastEcef() const { return East_; }
  const double *NorthEcef() const { return North_; }

  bool Complete() const;
  int PendingTiles() const { return Near_.Field ? Near_.Field->PendingTiles() + Far_.Field->PendingTiles() : -1; }

  /* THE ONE EVALUATOR, in C++ — the WGSL one reads the same bytes with the same rule. -1 = no datum
   * at this place, which is a state and not a default: the caller decides what to do with it. */
  /* Der Rahmen, in dem ClassAtEnu misst — ein Streuer braucht ihn, um vom Standpunkt aus zu zaehlen. */
  void Project(double lat, double lon, double *e, double *n) const;
  int ClassAt(double lat, double lon) const;
  /* Metres to the boundary of the winning class, and the class on the other side of it. */
  int ClassAt(double lat, double lon, double *distM, int *runnerUp) const;
  /* The same predicate on the structure's own metric frame, for a caller that already has one. */
  int ClassAtEnu(double e, double n, double *distM, int *runnerUp) const {
    return Evaluate(e, n, distM, runnerUp);
  }
  void ToEnu(double lat, double lon, double *e, double *n) const { Project(lat, lon, e, n); }

  double NoDataFraction() const { return Probe_ ? (double)ProbeNoData_ / (double)Probe_ : 0.0; }
  long UnknownKinds() const { return (long)Unknown_.size(); }
  long UnknownFeatures() const { return UnknownFeats_; }
  long MissingLayers() const { return Near_.Field ? Near_.Field->MissingLayers() + Far_.Field->MissingLayers() : 0; }
  long BadTiles() const { return Near_.Field ? Near_.Field->BadTiles() + Far_.Field->BadTiles() : 0; }
  double BuildMs() const { return BuildMsMax_; }
  long EdgeCount() const { return Edges_; }
  long SeedCount() const { return Seeds_; }
  int SeedOverflow() const { return Overflow_; }

private:
  struct Feat {
    uint32_t Idx;
    int Rank;
    uint16_t Tpl;
    uint8_t Type;
    float WidthM;
    float MinE, MinN, MaxE, MaxN;
  };
  struct Tier {
    std::unique_ptr<OsmField> Field;
    int Ring;
    double CellM;
    int Half;          /* cells from the grid centre; the grid is 2*Half square */
    double SlackM;     /* how far the camera may leave the grid centre before a re-anchor */
    std::vector<float> Pts;
    size_t PtsDone = 0;
    std::vector<Feat> Feats;
    size_t FeatsDone = 0;
    double OrgE = 0, OrgN = 0;
    bool Have = false;
    bool Stale = true;
    Tier(int zoom, int ring, double cellM, int half, double slackM)
        : Ring(ring), CellM(cellM), Half(half), SlackM(slackM), Zoom(zoom) {}
    int Zoom;
  };


  void Ingest(Tier &t);
  void BuildTier(Tier &t, double camE, double camN);
  void Pack();
  int Evaluate(double e, double n, double *distM, int *runnerUp) const;

  /* Per-tier packed block, kept apart so a rebuild of one does not touch the other. */
  struct Block {
    std::vector<uint32_t> Cells;   /* 2 u32: [base | seedCount<<8], seedFirst */
    std::vector<uint32_t> Seeds;   /* 2 u32: [tpl | rank<<8 | refCount<<16 | (wind+128)<<24], refFirst */
    std::vector<uint32_t> Refs;    /* edge index */
    std::vector<float> Edges;      /* x0,y0,x1,y1 */
    int W = 0, H = 0;
    double OrgE = 0, OrgN = 0, CellM = 1;
  };

  const VegetationTemplates *Veg_ = nullptr;
  /* Both tiers read the layers the DECLARATION names; the far one takes the area layers alone,
   * because at a kilometre a 7.5 m road is under a pixel and the street lines are two thirds of the
   * z11 edge count. */
  Tier Near_{14, 1, 16.0, 64, 448.0};
  Tier Far_{11, 1, 64.0, 128, 3800.0};
  Block NearB_, FarB_;
  std::vector<uint32_t> Buf_;
  bool Dirty_ = false;

  double O_[3] = {0, 0, 0}, East_[3] = {1, 0, 0}, North_[3] = {0, 1, 0};
  double Cam_[2] = {0, 0};
  bool Opened_ = false;

  std::unordered_set<std::string> Unknown_;
  long UnknownFeats_ = 0, Edges_ = 0, Seeds_ = 0, Probe_ = 0, ProbeNoData_ = 0;
  int Overflow_ = 0;
  double BuildMs_ = 0.0, BuildMsMax_ = 0.0;
};

} // namespace outshine::World
#endif
