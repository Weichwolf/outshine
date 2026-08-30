#ifndef OUTSHINE_TEST_RENDER_HANDED_H
#define OUTSHINE_TEST_RENDER_HANDED_H

#include <cstdint>
#include <string>
#include <vector>

#include <Geometry.h>
#include <Material.h>
#include <PunctualLight.h>

namespace outshine::Test {

// THE SCORER'S OWN FLAT READING OF WHAT A FILE CONTAINS. The door hands a `Geometry` back with one
// array per part and part-local triangle indices, which is what an AUTHOR fills in; a scorer walks
// the whole subject at once -- a silhouette is over every triangle it has, not over one part's --
// so it wants one vertex run and one index run with global ids. That is a LAYOUT, not a second
// model: nothing here is read from anywhere but the door, and a part keeps the door's own ordinal.
struct Handed {
  struct Part {
    std::string NodeName;
    int Material = -1;
    size_t FirstVertex = 0, VertexCount = 0;
    size_t FirstIndex = 0, IndexCount = 0;
    bool HasNormal = false, HasUv = false, HasUv1 = false, HasTangent = false, HasColour = false;
  };
  struct Lamp {
    std::string NodeName;
    PunctualLight Light;
  };

  void Reads(const Geometry &handed) {
    Parts_.clear();
    PositionsM_.clear();
    Normals_.clear();
    Indices_.clear();
    Surfaces_.clear();
    Lamps_.clear();
    HasUv1_ = false;
    for (int at = 0; at < handed.surfaces(); ++at) {
      Surfaces_.push_back(handed.surfaceAt(MaterialInstance(at)));
      SurfaceNames_.push_back(std::string(handed.surfaceNameOf(at)));
    }
    for (int at = 0; at < handed.lamps(); ++at) {
      Lamps_.push_back(Lamp{std::string(handed.lampNameOf(at)), handed.lampAt(at)});
    }
    for (int at = 0; at < handed.parts(); ++at) {
      const std::span<const float> positions = handed.positionsOf(at);
      const std::span<const float> normals = handed.normalsOf(at);
      const std::span<const uint32_t> triangles = handed.trianglesOf(at);
      Part one;
      one.NodeName = std::string(handed.nameOf(at));
      one.Material = handed.materialOf(at).index();
      one.FirstVertex = PositionsM_.size() / 3;
      one.VertexCount = positions.size() / 3;
      one.FirstIndex = Indices_.size();
      one.IndexCount = triangles.size();
      one.HasNormal = !normals.empty();
      one.HasUv = !handed.textureOf(at, 0).empty();
      one.HasUv1 = !handed.textureOf(at, 1).empty();
      one.HasTangent = !handed.tangentsOf(at).empty();
      one.HasColour = !handed.coloursOf(at).empty();
      HasUv1_ = HasUv1_ || one.HasUv1;

      // NORMALS ARE PADDED TO THE VERTEX RUN, never concatenated only where they exist: a part
      // without them would otherwise shift every later part's normals by its own vertex count,
      // and the reader indexes both runs by the SAME global vertex id.
      for (const float metre : positions) { PositionsM_.push_back((double)metre); }
      for (size_t axis = 0; axis < one.VertexCount * 3; ++axis) {
        Normals_.push_back(axis < normals.size() ? (double)normals[axis] : 0.0);
      }
      for (const uint32_t index : triangles) {
        Indices_.push_back((uint32_t)(index + one.FirstVertex));
      }
      Parts_.push_back(std::move(one));
    }
  }

  [[nodiscard]] const std::vector<Part> &Parts() const { return Parts_; }
  [[nodiscard]] const std::vector<Lamp> &Lights() const { return Lamps_; }
  [[nodiscard]] const std::vector<double> &PositionsM() const { return PositionsM_; }
  [[nodiscard]] const std::vector<double> &Normals() const { return Normals_; }
  [[nodiscard]] const std::vector<uint32_t> &Indices() const { return Indices_; }
  [[nodiscard]] const std::vector<Material> &Surfaces() const { return Surfaces_; }
  [[nodiscard]] const std::vector<std::string> &SurfaceNames() const { return SurfaceNames_; }
  [[nodiscard]] bool HasUv1() const { return HasUv1_; }
  [[nodiscard]] size_t VertexCount() const { return PositionsM_.size() / 3; }

  void CentreM(double out[3]) const {
    double least[3] = {0, 0, 0}, most[3] = {0, 0, 0};
    for (size_t vertex = 0; vertex * 3 + 2 < PositionsM_.size(); ++vertex) {
      for (int axis = 0; axis < 3; ++axis) {
        const double held = PositionsM_[vertex * 3 + (size_t)axis];
        least[axis] = vertex == 0 || held < least[axis] ? held : least[axis];
        most[axis] = vertex == 0 || held > most[axis] ? held : most[axis];
      }
    }
    for (int axis = 0; axis < 3; ++axis) { out[axis] = 0.5 * (least[axis] + most[axis]); }
  }

private:
  std::vector<Part> Parts_;
  std::vector<Lamp> Lamps_;
  std::vector<double> PositionsM_;
  std::vector<double> Normals_;
  std::vector<uint32_t> Indices_;
  std::vector<Material> Surfaces_;
  std::vector<std::string> SurfaceNames_;
  bool HasUv1_ = false;
};

}

#endif
