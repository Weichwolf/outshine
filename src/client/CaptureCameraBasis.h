#ifndef OUTSHINE_CLIENT_CAPTURECAMERABASIS_H
#define OUTSHINE_CLIENT_CAPTURECAMERABASIS_H

#include <scene/Camera.h>
#include <expected>

namespace outshine::Client {

struct CaptureCameraBasis {
  Vec3 Forward;
  Vec3 Up;

  [[nodiscard]] static std::expected<CaptureCameraBasis, CameraMatrixError>
  Of(const Camera &camera) noexcept {
    Mat4 view;
    if (const auto formed = camera.viewMatrix(view); !formed) {
      return std::unexpected(formed.error());
    }
    return CaptureCameraBasis{.Forward = {{-view[2], -view[6], -view[10]}},
                              .Up = {{view[1], view[5], view[9]}}};
  }
};

}
#endif
