#ifndef OUTSHINE_GENERATORS_TERRAIN_CLASSIFICATIONRASTERIZER_H
#define OUTSHINE_GENERATORS_TERRAIN_CLASSIFICATIONRASTERIZER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>
#include "ClassStructure.h"

namespace outshine::Generators {
class ClassificationRasterizer {
public:
  struct Ring {
    uint32_t First = 0, Count = 0;
  };

  enum class Shape : uint8_t { Point = 1, Line = 2, Polygon = 3 };

  struct Feature {
    uint32_t FirstRing = 0, RingCount = 0;
    int Rank = 0;
    uint16_t ClassRow = 0;
    Shape Form = Shape::Polygon;
    float WidthM = 0.0f;
    float MinE = 0, MinN = 0, MaxE = 0, MaxN = 0;
  };

  struct Input {
    double CamE = 0, CamN = 0;
    double CellM = 1;
    int HalfCells = 0;
    std::vector<float> Points;
    std::vector<Ring> Rings;
    std::vector<Feature> Features;
  };

  struct Built {
    std::shared_ptr<const ClassStructure::Grid> Grid;
    double BuildMs = 0;
    int Overflow = 0;
  };

  [[nodiscard]] std::optional<Built> Build(const Input &input, const std::stop_token &stop = {});
  [[nodiscard]] size_t ScratchBytes() const;

private:
  struct Hit {
    double X;
    int Dir;
  };

  struct Workspace {
    std::vector<uint8_t> Base, BaseRank;
    std::vector<int32_t> SeedHead, SeedNext;
    std::vector<uint32_t> SeedCount;
    std::vector<float> Edges, Curve;
    std::vector<uint32_t> ByY, Act;
    std::vector<int32_t> CellHead, CellNext;
    std::vector<uint32_t> CellStamp, CellEdge, CellCount;
    std::vector<Hit> Hits;
    std::vector<uint32_t> Seeds;
  };

  struct RasterWindow {
    int I0, I1, J0, J1;
    uint32_t Generation;
  };

  struct CellSample {
    int I, J, Winding;
  };

  void BuildFeatureEdges(const Input &job, const Feature &feature);
  void IndexFeatureEdges(const Feature &feature,
                         const ClassStructure::Grid &grid,
                         const RasterWindow &window);
  void ScanlineHits(double northM, size_t &nextEdge);
  void SeedCell(const Feature &feature,
                ClassStructure::Grid &grid,
                const RasterWindow &window,
                CellSample sample,
                int &overflow);
  void ScanFeature(const Feature &feature,
                   ClassStructure::Grid &grid,
                   const RasterWindow &window,
                   int &overflow);
  void PackGrid(ClassStructure::Grid &grid);
  void RasterizeFeature(const Input &job,
                        const Feature &feature,
                        uint32_t &generation,
                        ClassStructure::Grid &grid,
                        int &overflow);

  bool LayDown(const Input &job, ClassStructure::Grid &out, int &overflow);

  Workspace Workspace_;
  std::stop_token Stop_;
};
}
#endif
