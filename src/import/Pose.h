#ifndef OUTSHINE_IMPORT_POSE_H
#define OUTSHINE_IMPORT_POSE_H

#include <memory>
#include <string>
#include <vector>

#include "math/Vec4.h"
#include "math/Quat.h"
#include "math/Vec3.h"
#include "Track.h"
#include "Span.h"
#include "Transform.h"
#include "Types.h"

namespace outshine::Gltf {

class Document;

class Pose {
public:
  [[nodiscard]] static bool
  Build(const Document &document, int animation, Pose &out, std::string &error);

  [[nodiscard]] static bool
  Build(const Document &document, Span<const int> animations, Pose &out, std::string &error);

  [[nodiscard]] bool Valid() const { return !Nodes_.empty(); }

  [[nodiscard]] double StartS() const { return StartS_; }

  [[nodiscard]] double EndS() const { return EndS_; }

  [[nodiscard]] size_t ChannelCount() const { return Channels_.size(); }

  [[nodiscard]] size_t NodeCount() const { return Nodes_.size(); }

  void At(double seconds, std::vector<Transform> &locals, std::vector<double> &weights) const;

  struct FactorAt {
    int Material = -1;
    MaterialFactor Factor = MaterialFactor::BaseColour;
    Vec4 Values = {{0, 0, 0, 0}};
  };

  void FactorsAt(double seconds, std::vector<FactorAt> &factors) const;

  [[nodiscard]] size_t WeightsFirst(size_t node) const { return Nodes_[node].WeightFirst; }

  [[nodiscard]] size_t WeightsCount(size_t node) const { return Nodes_[node].WeightCount; }

  [[nodiscard]] size_t WeightCount() const { return RestWeights_.size(); }

private:
  struct Viewpoint {
    Vec3 Translation;
    Quat Rotation;
    Vec3 Scale = {{1, 1, 1}};
    bool HasMatrix = false;
    double Matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    size_t WeightFirst = 0;
    size_t WeightCount = 0;
  };

  struct Channel {
    int Node = -1;
    AnimationPath Path = AnimationPath::Translation;

    int Material = -1;
    MaterialFactor Factor = MaterialFactor::BaseColour;
    std::vector<double> Times, Values;
    Track Curve;
  };

  std::vector<std::unique_ptr<Channel>> Channels_;
  std::vector<Viewpoint> Nodes_;

  std::vector<double> RestWeights_;
  double StartS_ = 0, EndS_ = 0;
};

} // namespace outshine::Gltf
#endif
