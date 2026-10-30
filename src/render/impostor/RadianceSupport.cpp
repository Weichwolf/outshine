#include "RadianceSupport.h"
#include "ImpostorViewFrame.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Render {
namespace {
constexpr double kLeastRayPlaneDenominator = 1e-12;

struct Projection {
  const RadiancePlane &Source;
  const CameraBasis &Camera;
  const Lens &Optics;
  Mat4f Matrix;
  Vec3 Normal, Centre, CaptureDirection;
  ImpostorViewFrame Capture;

  [[nodiscard]] Vec3 Point(double u, double v) const {
    const auto &depth = Source.DepthCoefficients;
    return Source.Centre + Capture.Right * (u * Source.HalfExtentM) +
           Capture.Up * (v * Source.HalfExtentM) +
           CaptureDirection * ((depth[0] * u + depth[1] * v + depth[2]) * Source.HalfExtentM);
  }

  [[nodiscard]] std::optional<Vec2> Screen(const Vec3 &point) const {
    const Vec3 local = point - Camera.EyeM;
    const double forward = Dot(local, Camera.Forward);
    if (!(forward > Optics.NearM)) { return std::nullopt; }
    const double w = -Matrix[11] * forward + Matrix[15];
    if (!(w > 0)) { return std::nullopt; }
    const double x = (Matrix[0] * Dot(local, Camera.Right) - Matrix[8] * forward + Matrix[12]) / w;
    const double y = (Matrix[5] * Dot(local, Camera.Up) - Matrix[9] * forward + Matrix[13]) / w;
    const Vec2 pixel{{(1 + x) * Optics.WidePx * .5, (1 - y) * Optics.HighPx * .5}};
    if (!std::isfinite(pixel[0]) || !std::isfinite(pixel[1])) { return std::nullopt; }
    return pixel;
  }

  [[nodiscard]] std::optional<Vec3> Plane(const Vec2 &pixel) const {
    const bool orthographic = Optics.OrthoM > 0;
    const double w = orthographic ? Matrix[15] : Matrix[15] - Matrix[11];
    const double x =
        ((2 * pixel[0] / Optics.WidePx - 1) * w - Matrix[12] + (orthographic ? 0 : Matrix[8])) /
        Matrix[0];
    const double y =
        ((1 - 2 * pixel[1] / Optics.HighPx) * w - Matrix[13] + (orthographic ? 0 : Matrix[9])) /
        Matrix[5];
    const Vec3 offset = Camera.Right * x + Camera.Up * y;
    const Vec3 origin = orthographic ? Camera.EyeM + offset : Camera.EyeM;
    const Vec3 direction = orthographic ? Camera.Forward : Camera.Forward + offset;
    const double denominator = Dot(Normal, direction);
    if (std::abs(denominator) < kLeastRayPlaneDenominator) { return std::nullopt; }
    const double distance = Dot(Normal, Centre - origin) / denominator;
    const Vec3 point = origin + direction * distance;
    if (!(distance > 0) || !std::isfinite(Length(point))) { return std::nullopt; }
    return point;
  }
};
}

std::expected<Geometry, std::string> BuildProjectedRadianceSupport(const RadiancePlane &source,
                                                                   const CameraBasis &camera,
                                                                   const Lens &lens) {
  auto normal = source.TowardEye;
  const auto frame = ImpostorViewFrame::From(normal);
  const bool validLens = lens.OrthoM > 0 || (lens.FovDeg > 0 && lens.FovDeg < kDegPerHalfTurn);
  if (!frame || !Normalise(normal) || !(source.HalfExtentM > 0) ||
      !std::isfinite(source.HalfExtentM) || !(lens.WidePx > 0) || !(lens.HighPx > 0) ||
      !std::isfinite(lens.WidePx) || !std::isfinite(lens.HighPx) || !validLens ||
      !std::ranges::all_of(source.DepthCoefficients, [](double x) { return std::isfinite(x); })) {
    return std::unexpected("radiance plane or viewer is invalid");
  }
  const auto &depth = source.DepthCoefficients;
  const double scale = std::hypot(1.0, depth[0], depth[1]);
  Vec3 physicalNormal =
      normal * (1.0 / scale) - frame->Right * (depth[0] / scale) - frame->Up * (depth[1] / scale);
  const Vec3 centre = source.Centre + normal * (depth[2] * source.HalfExtentM);
  if (!Normalise(physicalNormal) || !(Dot(physicalNormal, camera.EyeM - centre) > 0)) {
    return std::unexpected("radiance physical support faces away from its viewer");
  }
  const Projection projection{.Source = source,
                              .Camera = camera,
                              .Optics = lens,
                              .Matrix = lens.Projection(),
                              .Normal = physicalNormal,
                              .Centre = centre,
                              .CaptureDirection = normal,
                              .Capture = *frame};
  if (!(projection.Matrix[0] > 0) || !(projection.Matrix[5] > 0)) {
    return std::unexpected("radiance projection is invalid");
  }
  double left = std::numeric_limits<double>::infinity();
  double top = left;
  double right = -left;
  double bottom = right;
  for (const double x : {-1.0, 1.0}) {
    for (const double y : {-1.0, 1.0}) {
      const auto pixel = projection.Screen(projection.Point(x, y));
      if (!pixel) { return std::unexpected("radiance plane crosses the near plane"); }
      left = std::min(left, (*pixel)[0]);
      right = std::max(right, (*pixel)[0]);
      top = std::min(top, (*pixel)[1]);
      bottom = std::max(bottom, (*pixel)[1]);
    }
  }
  left = std::max(-1.0, std::floor(left - 1));
  right = std::min(lens.WidePx + 1, std::ceil(right + 1));
  top = std::max(-1.0, std::floor(top - 1));
  bottom = std::min(lens.HighPx + 1, std::ceil(bottom + 1));
  if (!(left < right) || !(top < bottom)) {
    return std::unexpected("radiance support is outside the viewport");
  }
  std::vector<float> positions;
  std::vector<float> uv;
  std::vector<float> normals;
  positions.reserve(12);
  normals.reserve(12);
  uv.reserve(8);
  for (const Vec2 pixel :
       {Vec2{{left, bottom}}, Vec2{{right, bottom}}, Vec2{{right, top}}, Vec2{{left, top}}}) {
    const auto point = projection.Plane(pixel);
    if (!point) { return std::unexpected("radiance pixel support is unbounded"); }
    const Vec3 offset = *point - source.Centre;
    for (size_t axis = 0; axis < 3; ++axis) {
      positions.push_back(static_cast<float>((*point)[axis]));
      normals.push_back(static_cast<float>(physicalNormal[axis]));
    }
    uv.push_back(static_cast<float>(.5 + Dot(offset, frame->Right) / (2 * source.HalfExtentM)));
    uv.push_back(static_cast<float>(.5 - Dot(offset, frame->Up) / (2 * source.HalfExtentM)));
  }
  Geometry geometry;
  Material material;
  material.Unlit = true;
  material.Alpha = AlphaMode::Blended;
  material.BaseColour = {{1, 1, 1, 1}};
  const auto surface = geometry.addSurface("radiance", material);
  if (!surface) { return std::unexpected("radiance support material is invalid"); }
  const auto part = geometry.addPart("radiance", *surface);
  if (!part || !geometry.setPositions(*part, positions) || !geometry.setNormals(*part, normals) ||
      !geometry.setTexture(*part, uv) ||
      !geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3})) {
    return std::unexpected("radiance pixel support has invalid geometry");
  }
  return geometry;
}
}
