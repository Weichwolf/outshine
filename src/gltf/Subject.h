#ifndef GLTF_SUBJECT_H
#define GLTF_SUBJECT_H

#include <cstdint>
#include <string>
#include <vector>

#include "PunctualLight.h"
#include "Span.h"

#include "Camera.h"
#include "Framing.h"
#include "Transform.h"
#include "Variant.h"

namespace outshine::Gltf {

class Document;
struct Primitive;

struct Placement {
  double EyeM[3] = {0, 0, 0};
  double Forward[3] = {0, 0, -1};
  double Right[3] = {1, 0, 0};
  double Up[3] = {0, 1, 0};

  CameraKind Kind = CameraKind::Perspective;
  double YfovRad = 0;
  double XMagM = 0;
  double YMagM = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  [[nodiscard]] static bool LookAt(const double eyeM[3], const double aimM[3], double rollRad,
                                   Placement &out);

  [[nodiscard]] bool View(Transform &out) const;

  [[nodiscard]] bool Clip(double viewportAspect, Transform &out) const;
};

[[nodiscard]] bool FramingFor(const double minM[3], const double maxM[3], Placement &out,
                              double fill = kFramingFill);

[[nodiscard]] bool DeclaredPlacement(const Document &document, int cameraIndex, Placement &out,
                                     std::string &error);

enum class TangentSource { None, Supplied, Generated };

struct VertexPlacement {
  const Transform &Node;
  const Transform *Skinned = nullptr;

  const Transform &At(size_t vertex) const { return Skinned ? Skinned[vertex] : Node; }
};

struct Part {
  std::string NodeName;

  int Material = -1;
  bool HasUv = false;
  bool HasUv1 = false;
  bool HasNormal = false;
  bool HasColour = false;
  TangentSource Tangent = TangentSource::None;
  size_t FirstVertex = 0;
  size_t VertexCount = 0;
  size_t FirstIndex = 0;
  size_t IndexCount = 0;

  [[nodiscard]] bool HasTangent() const { return Tangent != TangentSource::None; }
};

struct PlacedLight {
  std::string NodeName;
  std::string LightName;
  outshine::PunctualLight Light;
};

struct Piece {
  std::string NodeName;
  int Material = -1;
  Span<const float> PositionsM;
  Span<const float> Normals;
  Span<const float> Uv;
  Span<const float> Uv1;
  Span<const float> Tangents;

  Span<const float> Colours;
  Span<const uint32_t> Indices;
};

struct Assembly {
  Span<const Piece> Pieces;
};

class Subject {
public:

  [[nodiscard]] bool Build(const Document &document, const VariantSelection &variant = {});

  [[nodiscard]] bool Build(const Document &document, Span<const Transform> pose,
                           Span<const double> weights, const VariantSelection &variant = {});

  [[nodiscard]] bool Assemble(const Assembly &what);

  const std::string &Error() const { return Error_; }

  const std::vector<double> &PositionsM() const { return Positions_; }

  const std::vector<double> &Uv() const { return Uv_; }
  bool HasUv() const { return !Uv_.empty(); }

  const std::vector<double> &Uv1() const { return Uv1_; }
  bool HasUv1() const { return !Uv1_.empty(); }

  const std::vector<double> &Normals() const { return Normals_; }
  bool HasNormal() const { return !Normals_.empty(); }

  const std::vector<double> &Tangents() const { return Tangents_; }
  bool HasTangent() const { return !Tangents_.empty(); }

  const std::vector<double> &Colours() const { return Colours_; }
  bool HasColour() const { return !Colours_.empty(); }

  const std::vector<PlacedLight> &Lights() const { return Lights_; }

  const std::vector<Part> &Parts() const { return Parts_; }

  struct Undrawn {
    size_t Primitives = 0;
    size_t ByMode[7] = {0, 0, 0, 0, 0, 0, 0};
  };
  const Undrawn &NotDrawn() const { return Undrawn_; }

  const std::vector<uint32_t> &Indices() const { return Indices_; }
  size_t VertexCount() const { return Positions_.size() / 3; }
  size_t TriangleCount() const { return Indices_.size() / 3; }

  const double *MinM() const { return Min_; }
  const double *MaxM() const { return Max_; }

  double RadiusM() const;
  void CentreM(double out[3]) const;

  [[nodiscard]]
  bool Frame(Placement &out, double fill = kFramingFill) const;

  double ProjectedAreaPx(const Transform &clip, const Viewport &viewport) const;

private:
  [[nodiscard]] bool Refuse(const std::string &why);

  [[nodiscard]] bool Flatten(const Document &document, const Transform *pose,
                             const double *weights, const VariantSelection &variant);

  void Bound();

  [[nodiscard]] bool MorphDeltasFor(const Document &document, const Primitive &primitive,
                                    const char *semantic, const double *weights, size_t count,
                                    size_t components, size_t vertices, std::vector<double> &out);
  [[nodiscard]] static Transform JointMatrix(const Skin &skin, size_t joint, const Transform &world);
  [[nodiscard]] bool BlendSkinFor(const Document &document, const Skin &skin,
                                  const std::vector<Transform> &joints, const Primitive &primitive,
                                  size_t vertices, std::vector<Transform> &out);

  [[nodiscard]] bool FlatNormalsFor(Part &part);

  [[nodiscard]] bool BuildTangentsFor(const Document &document, const Primitive &primitive,
                                      const VertexPlacement &place,
                                       Span<const double> morphWeights, Part &part,
                                       size_t vertices);

  std::string Error_;
  std::vector<double> Positions_;
  std::vector<double> Uv_;
  std::vector<double> Uv1_;
  std::vector<double> Normals_;
  std::vector<double> Tangents_;
  std::vector<double> Colours_;
  std::vector<uint32_t> Indices_;
  std::vector<Part> Parts_;
  Undrawn Undrawn_;
  std::vector<PlacedLight> Lights_;
  double Min_[3] = {0, 0, 0}, Max_[3] = {0, 0, 0};
};

}
#endif
