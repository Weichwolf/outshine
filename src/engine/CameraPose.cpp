#include "EngineState.h"

#include <expected>
#include <string>

namespace outshine {

Result Engine::setCameraPose(const Camera &pose) {
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (!S_->Session.Views) {
    return std::unexpected(std::string("a camera pose requires an active scenario view"));
  }
  const auto &previous = S_->Session.Views->Active();
  const bool changed = previous.Placement != Scenario::CameraPlacement::Local ||
                       previous.OffsetM != Vec3{} || previous.Geographic.SamplesHeight ||
                       !previous.Follows.empty() || previous.Sees.PositionM != pose.PositionM ||
                       previous.Sees.LooksAt != pose.LooksAt ||
                       previous.Sees.LookAtM != pose.LookAtM || previous.Sees.UpM != pose.UpM ||
                       previous.Sees.Orientation.X != pose.Orientation.X ||
                       previous.Sees.Orientation.Y != pose.Orientation.Y ||
                       previous.Sees.Orientation.Z != pose.Orientation.Z ||
                       previous.Sees.Orientation.W != pose.Orientation.W;
  const auto accepted = S_->Session.Views->SetCameraPose(pose);
  if (!accepted) {
    return std::unexpected(std::string("camera pose has no valid local transform"));
  }
  S_->RouteCamera.reset();
  S_->CameraPosePending = S_->CameraPosePending || changed;
  return {};
}

}
