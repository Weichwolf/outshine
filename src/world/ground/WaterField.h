#ifndef OUTSHINE_WORLD_GROUND_WATERFIELD_H
#define OUTSHINE_WORLD_GROUND_WATERFIELD_H

#include <span>
#include "OsmField.h"
#include "VegetationTemplates.h"

#include <cstdint>
#include <cstddef>
#include <chrono>
#include <optional>
#include <vector>

#include "Capacity.h"
#include "TileRanges.h"
#include "GroundQuery.h"
#include "TileWatermark.h"

namespace outshine::Ground {

class WaterField {
public:
  struct IngestMetrics {
    double TotalMs = 0.0;
    double AdmissionMs = 0.0;
    double ValidationMs = 0.0;
    double LongestQueryMs = 0.0;
    double MaterializationMs = 0.0;
    size_t ValidationPoints = 0;
  };

  struct Surface {
    uint32_t FirstPoint = 0, PointCount = 0;
    float LevelM = 0.0f;
  };

  struct Course {
    uint32_t FirstPoint = 0, PointCount = 0;
    uint32_t FirstLevel = 0;
    float HalfWidthM = 0.0f;
  };

  uint32_t Ingest(const GroundQuery &ground, const OsmField &field, const VegetationTemplates &veg);

  [[nodiscard]] const IngestMetrics &WorstIngest() const noexcept { return WorstIngest_; }

  [[nodiscard]] static std::optional<float> SurfaceLevel(std::span<double> heights);

  [[nodiscard]] const std::vector<Surface> &Surfaces() const { return Surfaces_; }

  [[nodiscard]] std::span<const Surface> OfTile(int tile) const {
    if (tile < 0) { return {}; }
    const TileRanges::Range r = ByTile_.At(static_cast<uint32_t>(tile));
    return {Surfaces_.data() + r.First, r.Count};
  }

  [[nodiscard]] const std::vector<Course> &Courses() const { return Courses_; }

  [[nodiscard]] const std::vector<float> &Levels() const { return Levels_; }

  void Settle() {
    Surfaces_.shrink_to_fit();
    Courses_.shrink_to_fit();
    Levels_.shrink_to_fit();
  }

  [[nodiscard]] size_t HeapBytes() const {
    size_t staged = CapacityBytes(Candidates_);
    for (const Candidate &candidate : Candidates_) {
      staged += CapacityBytes(candidate.Rings);
      for (const RingSamples &ring : candidate.Rings) { staged += CapacityBytes(ring.Heights); }
    }
    return CapacityBytes(Surfaces_) + CapacityBytes(Courses_) + CapacityBytes(Levels_) +
           Mark_.HeapBytes() + ByTile_.HeapBytes() + staged;
  }

  [[nodiscard]] long NoGroundCount() const { return NoGround_; }

  [[nodiscard]] long OutlierCount() const { return Outliers_; }

  [[nodiscard]] int Deferrals() const { return Mark_.Deferrals(); }

  [[nodiscard]] bool Ingested(const OsmField &field) const { return Mark_.Done(field.Features()); }

  [[nodiscard]] bool IngestedWithin(const OsmField &field, int rings) const {
    return Mark_.AcceptedWithin(
        field.Features(), field.Tiles(), field.CentreX(), field.CentreY(), rings);
  }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  struct RingSamples {
    size_t Feature = 0;
    size_t Ring = 0;
    std::vector<std::optional<double>> Heights;
  };

  struct Candidate {
    uint32_t Tile = 0;
    uint64_t LastSeen = 0;
    size_t From = 0, To = 0;
    size_t Feature = 0, Ring = 0, Point = 0;
    std::vector<RingSamples> Rings;
  };

  void AddCourse(const OsmField &field,
                 const OsmField::Feature &feature,
                 const OsmField::Ring &ring,
                 const VegetationTemplates &vegetation,
                 std::span<double> heights);
  void AddSurface(const OsmField::Ring &ring, std::span<double> heights);
  [[nodiscard]] static bool AdvanceCandidate(const GroundQuery &ground,
                                             const OsmField &field,
                                             OnLayers on,
                                             Candidate &candidate,
                                             std::chrono::steady_clock::time_point began,
                                             size_t &steps,
                                             IngestMetrics &metrics);
  void MaterializeCandidate(const OsmField &field,
                            OnLayers on,
                            const VegetationTemplates &vegetation,
                            const Candidate &candidate);
  std::vector<Surface> Surfaces_;
  std::vector<Course> Courses_;
  std::vector<float> Levels_;
  TileRanges ByTile_;
  TileWatermark Mark_;
  std::vector<Candidate> Candidates_;
  long NoGround_ = 0, Outliers_ = 0;
  IngestMetrics WorstIngest_;
  std::optional<uint64_t> SourceGeneration_;
  uint64_t Admission_ = 0;
};

}
#endif
