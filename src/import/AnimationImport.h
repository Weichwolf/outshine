#ifndef OUTSHINE_IMPORT_ANIMATIONIMPORT_H
#define OUTSHINE_IMPORT_ANIMATIONIMPORT_H

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "AnimationClip.h"
#include "Types.h"

namespace outshine::Gltf {

class Document;

class AnimationImport {
public:
  [[nodiscard]] static bool
  Build(const Document &document, int animation, AnimationClip &out, std::string &error);
  [[nodiscard]] static bool Build(const Document &document,
                                  std::span<const int> animations,
                                  AnimationClip &out,
                                  std::string &error);

private:
  struct BuildState;
  void InitialiseNodes(const Document &document);
  [[nodiscard]] bool ValidateChannel(const Document &document,
                                     const Animation &what,
                                     const AnimationChannel &channel,
                                     std::string &error) const;
  [[nodiscard]] bool AppendChannel(const Document &document,
                                   const Animation &what,
                                   const AnimationChannel &channel,
                                   std::string &error);
  [[nodiscard]] bool
  AppendAnimation(const Document &document, int animation, BuildState &state, std::string &error);
  void Publish(AnimationClip &out);

  std::vector<std::unique_ptr<AnimationTrack>> Tracks_;
  std::vector<AnimationRestPose> Nodes_;
  std::vector<double> RestWeights_;
  double StartS_ = 0;
  double EndS_ = 0;
};

}
#endif
