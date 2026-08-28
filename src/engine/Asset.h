#ifndef OUTSHINE_ENGINE_ASSET_H
#define OUTSHINE_ENGINE_ASSET_H

#include <string>
#include <vector>

#include <Scenario.h>

#include "Document.h"
#include "Pose.h"
#include "Subject.h"
#include "Variant.h"

namespace outshine::Core {

class Posed {
public:
  void Clears();
  [[nodiscard]] bool Reads(const std::string &path, const std::string &variant,
                           AssetAnimation animation, int clip, double fps, std::string &error);
  [[nodiscard]] bool Poses(int frame, double fps, std::string &error);
  void Carries(const Gltf::Subject &built) { Geometry_ = built; }

  [[nodiscard]] const Gltf::Document &File() const { return File_; }
  [[nodiscard]] const Gltf::Subject &Geometry() const { return Geometry_; }
  [[nodiscard]] Gltf::Subject &Geometry() { return Geometry_; }
  [[nodiscard]] const std::vector<double> &Previous() const { return PreviousPositionsM_; }
  [[nodiscard]] bool Moves() const { return Moves_; }
  [[nodiscard]] bool Stands() const { return Read_; }
  [[nodiscard]] int Frames() const { return Frames_; }
  [[nodiscard]] int At() const { return At_; }
  void Advances(int frames) { At_ = frames > 0 ? (At_ + 1) % frames : 0; }

private:
  Gltf::Document File_;
  Gltf::Subject Geometry_;
  Gltf::Pose Motion_;
  Gltf::VariantSelection Variant_;
  std::vector<Gltf::Transform> Locals_;
  std::vector<double> Weights_;
  std::vector<double> PreviousPositionsM_;
  bool Moves_ = false;
  bool Read_ = false;
  int Frames_ = 1;
  int At_ = 0;
};

}
#endif
