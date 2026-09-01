#ifndef OUTSHINE_WORLD_GROUND_CLASSFIELD_H
#define OUTSHINE_WORLD_GROUND_CLASSFIELD_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "ClassBuilder.h"
#include "OsmField.h"
#include "TilePool.h"
#include "TangentFrame.h"

namespace outshine::Ground {

class VegetationTemplates;

class ClassField {
public:
  void Declares(std::span<const OsmField::Declared> these) {
    Declared_.assign(these.begin(), these.end());
  }

  void SetVegetation(const VegetationTemplates *veg) { Veg_ = veg; }

  void Open(double lat, double lon);

  void Update(TilePool &tiles, double camLat, double camLon, double budgetMs);

  std::shared_ptr<const ClassStructure> Read() const {
    const std::lock_guard<std::mutex> lk(Mu_);
    return Published_;
  }

  const double *OriginEcef() const { return Frame_.OriginEcef(); }

  const double *Cam() const { return Cam_; }

  const double *EastEcef() const { return Frame_.EastEcef(); }

  const double *NorthEcef() const { return Frame_.NorthEcef(); }

  void Project(double lat, double lon, double *e, double *n) const {
    Frame_.Project(lat, lon, e, n);
  }

  [[nodiscard]] int
  ClassAt(const ClassStructure &held, double lat, double lon, double *edgeM, int *runnerUp) const {
    double e = 0.0;
    double n = 0.0;
    Project(lat, lon, &e, &n);
    return held.Evaluate(e, n, edgeM, runnerUp);
  }

  [[nodiscard]] int ClassAt(double lat, double lon, double *edgeM, int *runnerUp) const {
    const std::shared_ptr<const ClassStructure> held = Read();
    if (!held) { return -1; }
    return ClassAt(*held, lat, lon, edgeM, runnerUp);
  }

  void FromEnu(double e, double n, double *lat, double *lon) const { Frame_.Geo(e, n, lat, lon); }

  void ToEnu(double lat, double lon, double *e, double *n) const { Project(lat, lon, e, n); }

  [[nodiscard]] bool Complete() const;

  int PendingTiles() const {
    return Fine_.Field ? Fine_.Field->PendingTiles() + Coarse_.Field->PendingTiles() : -1;
  }

  long UnknownKinds() const { return static_cast<long>(Unknown_.size()); }

  long UnknownFeatures() const { return UnknownFeats_; }

  long MissingLayers() const {
    return Fine_.Field ? Fine_.Field->MissingLayers() + Coarse_.Field->MissingLayers() : 0;
  }

  long BadTiles() const {
    return Fine_.Field ? Fine_.Field->BadTiles() + Coarse_.Field->BadTiles() : 0;
  }

  double LastStreamMs() const { return StreamMs_; }

  double LastIngestMs() const { return IngestMs_; }

  double MaxBuildMs() const { return BuildMsMax_; }

  size_t FeaturesHeld() const { return Fine_.Field ? Fine_.Field->Features().size() : 0u; }

  size_t FeaturesTaken() const { return Fine_.Feats.size(); }

  long FineSubmits() const { return Submits_[0]; }

  long CoarseSubmits() const { return Submits_[1]; }

  size_t HeapBytes() const;

private:
  std::vector<OsmField::Declared> Declared_;

  struct Tier {
    std::unique_ptr<OsmField> Field;
    int TileRadius;
    double CellM;
    int HalfCells;
    double SlackM;
    int Zoom;
    std::vector<float> Pts;
    uint64_t Generation = 0;
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

    [[nodiscard]] size_t HeapBytes() const;
  };

  void Ingest(Tier &t);
  void SubmitDue(double camE, double camN);
  ClassBuilder::Job LendTo(Tier &t, ClassGrain grain, double camE, double camN);

  Tier &TierOf(ClassGrain grain) { return grain == ClassGrain::Fine ? Fine_ : Coarse_; }

  const VegetationTemplates *Veg_ = nullptr;

  Tier Fine_{14, 1, 16.0, 64, 448.0};
  Tier Coarse_{11, 1, 64.0, 128, 3800.0};

  ClassBuilder Builder_;
  std::optional<ClassGrain> Submitted_;

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

} // namespace outshine::Ground
#endif
