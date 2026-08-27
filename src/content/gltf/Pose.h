#ifndef OUTSHINE_CONTENT_GLTF_POSE_H
#define OUTSHINE_CONTENT_GLTF_POSE_H

#include <memory>
#include <string>
#include <vector>

#include "Track.h"
#include "Span.h"
#include "Transform.h"
#include "Types.h"

namespace outshine::Gltf {

class Document;

class Pose {
public:

  [[nodiscard]] static bool Build(const Document &document, int animation, Pose &out,
                                  std::string &error);

  [[nodiscard]] static bool Build(const Document &document, Span<const int> animations, Pose &out,
                                  std::string &error);

  [[nodiscard]] bool Valid() const { return !Nodes_.empty(); }

  double StartS() const { return StartS_; }
  double EndS() const { return EndS_; }
  size_t ChannelCount() const { return Channels_.size(); }
  size_t NodeCount() const { return Nodes_.size(); }

  void At(double seconds, std::vector<Transform> &locals, std::vector<double> &weights) const;

  struct FactorAt {
    int Material = -1;
    MaterialFactor Factor = MaterialFactor::BaseColour;
    double Values[4] = {0, 0, 0, 0};
  };

  void FactorsAt(double seconds, std::vector<FactorAt> &factors) const;

  [[nodiscard]] size_t WeightsFirst(size_t node) const { return Nodes_[node].WeightFirst; }
  [[nodiscard]] size_t WeightsCount(size_t node) const { return Nodes_[node].WeightCount; }
  [[nodiscard]] size_t WeightCount() const { return RestWeights_.size(); }

private:

  struct Viewpoint {
    double Translation[3] = {0, 0, 0};
    double Rotation[4] = {0, 0, 0, 1};
    double Scale[3] = {1, 1, 1};
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

}
#endif
