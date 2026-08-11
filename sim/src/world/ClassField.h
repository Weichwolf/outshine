/* The ground class, evaluated from the OSM vectors themselves — there is no class raster on either
 * side. This class owns the streaming and decides when a grid is due; ClassBuilder lays it down.
 * doc/todo.md §1 carries the shape.
 *
 * WHY NO RASTER, because the alternative keeps suggesting itself: a raster has a resolution, and a
 * resolution made the class a function of the viewer three times running — the mip level, the tile
 * zoom, the consumer's own lattice. Evaluating the outlines analytically removes that failure class
 * structurally instead of forbidding it. It is affordable because of one measurement: over the 3x3 z14
 * block at the reference standpoint (20.3 km^2, 2 246 area features, 25 501 edges) a fragment sees a
 * mean of 3.87 edges and 2.11 features with a 16 m acceleration cell, p99 16 edges, worst cell 45.
 *
 * THE ORDER IS DECLARED, NOT INHERITED: features are laid down by the `rank` on their row in
 * vegetation.json, because the tile server has none inside a layer and would leave the winner to the
 * provider's emission order over features that overlap by 2.41 % of the tile. */
#ifndef CLASSFIELD_H
#define CLASSFIELD_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "ClassBuilder.h"
#include "OsmField.h"
#include "TangentFrame.h"

namespace outshine::World {

class VegetationTemplates;

class ClassField {
public:
  void SetVegetation(const VegetationTemplates *veg) { Veg_ = veg; }
  void Open(double lat, double lon);
  /* One budgeted pass: at most one vector tile per tier, and at most one tier submitted. */
  void Update(double camLat, double camLon);

  /* WHAT A READER HOLDS. Null until the first grid has been laid down; from then on the handle keeps
   * the structure it names alive and unwritable for as long as the caller keeps it. */
  std::shared_ptr<const ClassStructure> Read() const {
    std::lock_guard<std::mutex> lk(Mu_);
    return Published_;
  }

  /* The plane the structure's metres are measured in. Fixed at Open() and handed to every structure
   * published from here, so a reader that holds the words holds the plane they mean. */
  const double *OriginEcef() const { return Frame_.OriginEcef(); }
  /* The camera's own place in this frame, the offset a fragment adds to its camera-relative one. */
  const double *Cam() const { return Cam_; }
  const double *EastEcef() const { return Frame_.EastEcef(); }
  const double *NorthEcef() const { return Frame_.NorthEcef(); }
  void Project(double lat, double lon, double *e, double *n) const { Frame_.Project(lat, lon, e, n); }
  /* The inverse: a scatterer works in ENU and needs degrees to ask for the height. */
  void FromEnu(double e, double n, double *lat, double *lon) const { Frame_.Geo(e, n, lat, lon); }
  void ToEnu(double lat, double lon, double *e, double *n) const { Project(lat, lon, e, n); }

  bool Complete() const;
  int PendingTiles() const {
    return Fine_.Field ? Fine_.Field->PendingTiles() + Coarse_.Field->PendingTiles() : -1;
  }

  long UnknownKinds() const { return (long)Unknown_.size(); }
  long UnknownFeatures() const { return UnknownFeats_; }
  long MissingLayers() const {
    return Fine_.Field ? Fine_.Field->MissingLayers() + Coarse_.Field->MissingLayers() : 0;
  }
  long BadTiles() const {
    return Fine_.Field ? Fine_.Field->BadTiles() + Coarse_.Field->BadTiles() : 0;
  }
  /* What the render thread still pays for the class: streaming the vectors and projecting them. The
   * grid is not in either number — that is what the split is for. */
  double LastStreamMs() const { return StreamMs_; }
  double LastIngestMs() const { return IngestMs_; }
  double MaxBuildMs() const { return BuildMsMax_; }
  /* Submits per grain. The scheduler always offers the fine grain first, so a coarse count that
   * stops climbing is starvation and not quiet. */
  long FineSubmits() const { return Submits_[0]; }
  long CoarseSubmits() const { return Submits_[1]; }

  /* Everything the class holds on the heap: both vector streams, both tiers' projected outlines, the
   * builder's grids and scratch, and the structure a reader may still be holding. */
  size_t HeapBytes() const;

private:
  /* One grain of the class as this side holds it: the vector stream it reads, the outlines it has
   * projected, and where they stand. The two tiers differ only in resolution and reach. */
  struct Tier {
    std::unique_ptr<OsmField> Field;
    int TileRadius;    /* tiles around the camera the vector stream fetches */
    double CellM;
    int HalfCells;
    double SlackM;     /* how far the camera may leave the grid centre before a re-anchor */
    int Zoom;
    std::vector<float> Pts;
    size_t PtsDone = 0;
    std::vector<ClassBuilder::Ring> Rings;
    size_t RingsDone = 0;
    std::vector<ClassBuilder::Feature> Feats;
    size_t FeatsDone = 0;
    double OrgE = 0, OrgN = 0;
    bool Have = false;
    bool Stale = true;
    bool ArraysLent = false;
    Tier(int zoom, int tileRadius, double cellM, int halfCells, double slackM)
        : TileRadius(tileRadius), CellM(cellM), HalfCells(halfCells), SlackM(slackM), Zoom(zoom) {}
    size_t HeapBytes() const;
  };

  void Ingest(Tier &t);
  bool SubmitDue(double camE, double camN);
  ClassBuilder::Job LendTo(Tier &t, ClassGrain grain, double camE, double camN);
  Tier &TierOf(ClassGrain grain) { return grain == ClassGrain::Fine ? Fine_ : Coarse_; }

  const VegetationTemplates *Veg_ = nullptr;
  /* The split is a property of the VECTOR FETCH and of nothing else: a 3x3 z14 block guarantees
   * 1502.33 m and a 3x3 z11 block 12018.6 m; beyond that there is no datum and the declared default is
   * the honest answer. The coarse grain takes the area layers alone, because at a kilometre a 7.5 m
   * road is under a pixel and the street lines are two thirds of the z11 edge count. */
  Tier Fine_{14, 1, 16.0, 64, 448.0};
  Tier Coarse_{11, 1, 64.0, 128, 3800.0};

  ClassBuilder Builder_;
  std::optional<ClassGrain> Submitted_;
  /* Every reader is on the render thread today, so this lock is never contended. It is here because
   * the handle it guards is what a reader off the thread will take, and a lock added later is a lock
   * that was missing in between. */
  mutable std::mutex Mu_;
  std::shared_ptr<const ClassStructure> Published_;

  TangentFrame Frame_;
  double Cam_[2] = {0, 0};
  bool Opened_ = false;

  std::unordered_set<std::string> Unknown_;
  long UnknownFeats_ = 0;
  double StreamMs_ = 0.0, IngestMs_ = 0.0, BuildMsMax_ = 0.0;
  long Submits_[2] = {0, 0};
};

} // namespace outshine::World
#endif
