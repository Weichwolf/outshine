#include "LightVisibilityStage.h"

#include <cmath>

#include "SubjectDraw.h"

namespace outshine::Render {

bool LightVisibilityStage::Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error) {
  Subjects_ = &subjects;
  return subjects.ConfigureDepthOnly(gpu, error);
}

void LightVisibilityStage::Declare(const float toSun[3], const float up[3], double radiusM) {
  for (int axis = 0; axis < 3; ++axis) {
    ToSun_[axis] = (double)toSun[axis];
    Up_[axis] = (double)up[axis];
  }
  RadiusM_ = radiusM;
  // a degenerate basis refuses HERE: a zero sun or an up parallel to it divides the
  // normalisation by zero and every shadow term goes silently NaN -- a failure is loud
  double sunLength = 0.0, crossLength = 0.0;
  double cross[3] = {Up_[1] * ToSun_[2] - Up_[2] * ToSun_[1],
                     Up_[2] * ToSun_[0] - Up_[0] * ToSun_[2],
                     Up_[0] * ToSun_[1] - Up_[1] * ToSun_[0]};
  for (int axis = 0; axis < 3; ++axis) {
    sunLength += ToSun_[axis] * ToSun_[axis];
    crossLength += cross[axis] * cross[axis];
  }
  Declared_ = radiusM > 0.0 && sunLength > 0.0 && crossLength > 0.0;
}

void LightVisibilityStage::Frame(const double centreM[3]) {
  for (int axis = 0; axis < 3; ++axis) { CentreM_[axis] = centreM[axis]; }
}

void LightVisibilityStage::Build(const double eye[3]) {
  double forward[3] = {-ToSun_[0], -ToSun_[1], -ToSun_[2]};
  double length = 0.0;
  for (int axis = 0; axis < 3; ++axis) { length += forward[axis] * forward[axis]; }
  length = std::sqrt(length);
  for (int axis = 0; axis < 3; ++axis) { forward[axis] /= length; }

  double right[3] = {Up_[1] * forward[2] - Up_[2] * forward[1],
                     Up_[2] * forward[0] - Up_[0] * forward[2],
                     Up_[0] * forward[1] - Up_[1] * forward[0]};
  double rLength = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
  for (int axis = 0; axis < 3; ++axis) { right[axis] /= rLength; }
  const double upward[3] = {forward[1] * right[2] - forward[2] * right[1],
                            forward[2] * right[0] - forward[0] * right[2],
                            forward[0] * right[1] - forward[1] * right[0]};

  const double texelM = 2.0 * RadiusM_ / (double)kShadowAtlasPx;
  double eyeLight[3] = {0.0, 0.0, 0.0};
  for (int axis = 0; axis < 3; ++axis) {
    eyeLight[0] += right[axis] * CentreM_[axis];
    eyeLight[1] += upward[axis] * CentreM_[axis];
    eyeLight[2] += forward[axis] * CentreM_[axis];
  }

  eyeLight[0] = std::floor(eyeLight[0] / texelM) * texelM;
  eyeLight[1] = std::floor(eyeLight[1] / texelM) * texelM;

  const double depthM = 2.0 * RadiusM_;
  const double nearAlong = eyeLight[2] - depthM;
  const double farAlong = eyeLight[2] + depthM;
  for (int i = 0; i < 16; ++i) { LightFromWorld_[i] = 0.0; }
  for (int axis = 0; axis < 3; ++axis) {
    LightFromWorld_[axis * 4 + 0] = right[axis] / RadiusM_;
    LightFromWorld_[axis * 4 + 1] = upward[axis] / RadiusM_;

    LightFromWorld_[axis * 4 + 2] = -forward[axis] / (farAlong - nearAlong);
  }
  LightFromWorld_[12] = -eyeLight[0] / RadiusM_;
  LightFromWorld_[13] = -eyeLight[1] / RadiusM_;
  LightFromWorld_[14] = farAlong / (farAlong - nearAlong);
  LightFromWorld_[15] = 1.0;

  for (int row = 0; row < 3; ++row) {
    double carried = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      carried += LightFromWorld_[axis * 4 + row] * eye[axis];
    }
    LightFromWorld_[12 + row] += carried;
  }
}

void LightVisibilityStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (!Declared_ || Subjects_ == nullptr) { return; }
  Build(ctx.Eye);
  Subjects_->EncodeDepthOnly(LightFromWorld_, ctx.Eye, kShadowAtlasPx, into);
}

}
