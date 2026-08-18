/* THE TANGENT BASIS A NORMAL MAP IS READ IN, IN C++ AND IN MSL. glTF states a per-texel normal in
 * the space the file's own `NORMAL` and `TANGENT` attributes span, so the basis is what makes the
 * sampled triple a direction in the scene at all; get it wrong and the shading is wrong by a
 * rotation nobody can see in a single lit still.
 *
 * TWO HALVES OF ONE STATEMENT, for the reason `ShadowRay.h` and `MetalRoughBrdf.h` state beside
 * theirs: the basis has to run on the device and has to be checkable without one. THE TWO ARE
 * SPELLED DIFFERENTLY ON PURPOSE. The C++ half negates the three axes one by one, which is the
 * format's own sentence; the MSL half carries a single sign out to the composed normal, which is the
 * shape a mistake cannot be written in. A transliteration would agree with itself by construction
 * and prove nothing -- a shader test runs both over one
 * sample set and holds them to the same direction.
 *
 * THE FRAME IS RE-ORTHOGONALISED HERE AND NOT TRUSTED AS IT ARRIVES. A supplied `TANGENT` is
 * interpolated across a triangle and quantised to f32 by whoever wrote the file, so it is neither
 * unit nor perpendicular at a fragment -- MEASURED on `NormalTangentMirrorTest`, |T.N| reaches
 * 1.13e-5 at its own vertices before any interpolation. Gram-Schmidt against the shading normal is
 * the correction the format's own sample implementation applies, and it costs one dot product.
 *
 * THE BITANGENT IS DERIVED FROM `w` AND NEVER CARRIED. glTF states the handedness as one number
 * exactly so that a mirrored body cannot end up with a bitangent that disagrees with its tangent;
 * a run that carried both would be two answers to one question.
 *
 * ONLY x AND y ARE SCALED. `normalTexture.scale` is defined as multiplying the sampled x and y
 * (Specification.adoc:1416), so the surface flattens towards its geometric normal as the scale goes
 * to zero -- scaling z as well would leave the normal unchanged and the parameter would do nothing.
 *
 * A BACK FACE NEGATES ALL THREE AXES AND NOT TWO (board:1127). The format's own renderer is
 * unambiguous -- `t *= -1; b *= -1; ng *= -1` in `material_info.glsl:160` of glTF-Sample-Renderer --
 * and the reason is that a back face is the same surface seen through a mirror: the map's y axis
 * runs the other way across the screen there, so a basis that flipped the normal and the tangent and
 * kept the bitangent would read the map's green channel backwards. Deriving the bitangent AFTER the
 * flip is exactly how two axes get negated while the third quietly does not, because the cross
 * product is bilinear and `cross(-n, -t) == cross(n, t)`. */
#ifndef NORMALFROMMAP_H
#define NORMALFROMMAP_H

#include <array>
#include <cmath>
#include <string>

namespace outshine::Render {

/* Three doubles, because the subject here is a DIRECTION and the reference half is evaluated in f64
 * against a device that answers in f32. */
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

/* WHICH SIDE OF THE SURFACE THE FRAGMENT IS ON, as a type rather than as a bool, so a call site
 * cannot pass the wrong one silently (`Enum.2`). The device half takes MSL's `[[front_facing]]`
 * bool because that is what a fragment is handed. */
enum class Facing { Front, Back };

/* WHAT THE FILE SUPPLIES AT A VERTEX: glTF's `NORMAL` and `TANGENT`, the second of which is four
 * numbers -- a direction and the handedness of the bitangent (`I.23`). Neither is unit here and the
 * tangent is not perpendicular; that is the caller's honest state and this header's job. */
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

/* `tap` is the texture's triple already decoded to [-1, 1]; the scale is the material's own. */
[[nodiscard]] inline Direction NormalFromMap(const SuppliedFrame &supplied, const Direction &tap,
                                             double scale, Facing facing) {
  const SurfaceBasis basis = SurfaceBasisAt(supplied, facing);
  const Direction along = Scaled(basis.Tangent, tap.X * scale);
  const Direction across = Scaled(basis.Bitangent, tap.Y * scale);
  const Direction out = Scaled(basis.Normal, tap.Z);
  return Normalised(Sum(Sum(along, across), out));
}

[[nodiscard]] inline std::string NormalFromMapMsl(void) {
  return R"(
/* THE SIGN IS CARRIED OUT TO THE COMPOSED NORMAL AND APPEARS ONCE (board:1127). The basis is built
 * in the front-facing orientation and the three axes are never negated one at a time here, because
 * the composition is linear in all three and a single sign on the result IS the negated frame -- so
 * "two axes turned and the third did not" has no spelling in this function. */
static inline float3 normalFromMap(float3 vertexNormal, float4 tangent, float3 tap, float scale,
                                   bool front) {
  float3 n = normalize(vertexNormal);
  float3 t = normalize(tangent.xyz - n * dot(n, tangent.xyz));
  float3 b = cross(n, t) * tangent.w;
  float3 scaled = float3(tap.xy * scale, tap.z);
  float3 mapped = normalize(t * scaled.x + b * scaled.y + n * scaled.z);
  return mapped * select(-1.0, 1.0, front);
}
)";
}

} // namespace outshine::Render
#endif
