#ifndef OUTSHINE_CONTENT_SCENE_SCENEPLAYBACK_H
#define OUTSHINE_CONTENT_SCENE_SCENEPLAYBACK_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <scene/Geometry.h>
#include <scene/Camera.h>

namespace outshine {
class AnimatedAsset;
}

namespace outshine {

enum class PlaybackPolicy : uint8_t { Once, Loop, RestPose, ExternallyDriven };

[[nodiscard]] constexpr bool ImportsAnimation(PlaybackPolicy policy) noexcept {
  return policy == PlaybackPolicy::Once || policy == PlaybackPolicy::Loop;
}

class ScenePlayback {
public:
  ScenePlayback();
  ~ScenePlayback();
  ScenePlayback(ScenePlayback &&) noexcept;
  ScenePlayback &operator=(ScenePlayback &&) noexcept;
  ScenePlayback(const ScenePlayback &) = delete;
  ScenePlayback &operator=(const ScenePlayback &) = delete;

  void Clear();

  struct AssetRequest {
    std::string Path;
    std::string Variant;
  };

  struct PlaybackSettings {
    int Clip = 0;
    double Fps = 0.0;
  };

  [[nodiscard]] bool
  Load(const AssetRequest &asset, bool animate, PlaybackSettings settings, std::string &error);
  [[nodiscard]] bool Sample(double seconds, std::string &error);

  void SetGeometry(outshine::Geometry &&geometry);

  [[nodiscard]] bool Append(ScenePlayback &&more, std::string &error);

  [[nodiscard]] uint64_t Revision() const { return Revision_; }

  [[nodiscard]] bool HasGeometry() const { return HasGeometry_; }

  [[nodiscard]] const outshine::Geometry &Snapshot() const { return Geometry_; }

  [[nodiscard]] const std::optional<outshine::Camera> &AuthoredCamera() const { return Camera_; }

  [[nodiscard]] bool IsAnimated() const { return Animated_; }

  [[nodiscard]] double PlacementDigest() const { return PlacementDigest_; }

  [[nodiscard]] double VertexDigest() const { return VertexDigest_; }

  [[nodiscard]] bool IsLoaded() const { return Loaded_; }

  [[nodiscard]] int FrameCount() const { return FrameCount_; }

  [[nodiscard]] double TimeS() const { return TimeS_; }

  [[nodiscard]] double DurationS() const { return DurationS_; }

  void Advance(double stepS, bool loops) {
    const double end = DurationS_;
    const double next = TimeS_ + stepS;
    if (!(end > 0.0)) {
      TimeS_ = 0.0;
      return;
    }
    TimeS_ = loops ? next - end * std::floor(next / end) : std::min(next, end);
  }

private:
  struct Asset {
    std::unique_ptr<AnimatedAsset> Animator;
    outshine::Geometry Snapshot;
    std::optional<outshine::Camera> Camera;
  };

  std::vector<Asset> Assets_;
  std::optional<outshine::Camera> Camera_;
  outshine::Geometry Geometry_;
  bool HasGeometry_ = false;
  uint64_t Revision_ = 0;
  [[nodiscard]] bool PoseInto(double seconds, std::string &error);
  [[nodiscard]] bool Rebuild(std::span<const outshine::Geometry> snapshots, std::string &error);
  void RefreshCamera();
  void RefreshDigests();
  bool Animated_ = false;
  double DurationS_ = 0.0;
  double PlacementDigest_ = 0.0;
  double VertexDigest_ = 0.0;
  bool Loaded_ = false;
  int FrameCount_ = 1;
  double TimeS_ = 0.0;
};

}
#endif
