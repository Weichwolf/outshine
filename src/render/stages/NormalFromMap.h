#ifndef OUTSHINE_RENDER_STAGES_NORMALFROMMAP_H
#define OUTSHINE_RENDER_STAGES_NORMALFROMMAP_H

#include <array>
#include <cmath>
#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

struct Direction {
  double X = 0, Y = 0, Z = 0;
};

[[nodiscard]] inline double Dot(const Direction &left, const Direction &right) {
  return left.X * right.X + left.Y * right.Y + left.Z * right.Z;
}

[[nodiscard]] inline Direction Cross(const Direction &left, const Direction &right) {
  return {left.Y * right.Z - left.Z * right.Y, left.Z * right.X - left.X * right.Z,
          left.X * right.Y - left.Y * right.X};
}

[[nodiscard]] inline Direction Scaled(const Direction &of, double by) {
  return {of.X * by, of.Y * by, of.Z * by};
}

[[nodiscard]] inline Direction Sum(const Direction &left, const Direction &right) {
  return {left.X + right.X, left.Y + right.Y, left.Z + right.Z};
}

[[nodiscard]] inline Direction Normalised(const Direction &of) {
  const double length = std::sqrt(Dot(of, of));
  return Scaled(of, 1.0 / length);
}

enum class Facing { Front, Back };

struct SuppliedFrame {
  Direction Normal;
  Direction Tangent;
  double Handedness = 1.0;
};

struct SurfaceBasis {
  Direction Tangent;
  Direction Bitangent;
  Direction Normal;
};

[[nodiscard]] inline SurfaceBasis SurfaceBasisAt(const SuppliedFrame &supplied, Facing facing) {
  const Direction normal = Normalised(supplied.Normal);
  const Direction perpendicular = Normalised(
      Sum(supplied.Tangent, Scaled(normal, -Dot(normal, supplied.Tangent))));
  const Direction bitangent = Scaled(Cross(normal, perpendicular), supplied.Handedness);
  if (facing == Facing::Front) { return {perpendicular, bitangent, normal}; }
  return {Scaled(perpendicular, -1.0), Scaled(bitangent, -1.0), Scaled(normal, -1.0)};
}

[[nodiscard]] inline Direction NormalFromMap(const SuppliedFrame &supplied, const Direction &tap,
                                             double scale, Facing facing) {
  const SurfaceBasis basis = SurfaceBasisAt(supplied, facing);
  const Direction along = Scaled(basis.Tangent, tap.X * scale);
  const Direction across = Scaled(basis.Bitangent, tap.Y * scale);
  const Direction out = Scaled(basis.Normal, tap.Z);
  return Normalised(Sum(Sum(along, across), out));
}

[[nodiscard]] inline std::string NormalFromMapMsl(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/normalFromMap.msl", body, error)) { return std::string(); }
  return body;
}

[[nodiscard]] inline std::string NormalFromMapMsl() {
  std::string ignored;
  return NormalFromMapMsl(ignored);
}

}
#endif
