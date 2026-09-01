#ifndef OUTSHINE_IMPORT_SUBJECT_H
#define OUTSHINE_IMPORT_SUBJECT_H

#include "Viewing.h"
#include <Geometry.h>
#include <span>
#include <cstdint>
#include <string>
#include <vector>

#include "PunctualLight.h"
#include "Span.h"

#include "Viewport.h"
#include "Framing.h"
#include "Transform.h"
#include "Variant.h"

namespace outshine {
class Geometry;
}

namespace outshine::Gltf {

class Document;
struct Primitive;

using Viewpoint = Render::Viewpoint;

[[nodiscard]] bool ViewOf(const Viewpoint &from, Transform &out);

[[nodiscard]] bool ClipOf(const Viewpoint &from, double viewportAspect, Transform &out);

[[nodiscard]] bool FramingFor(const double minM[3],
                              const double maxM[3],
                              Viewpoint &out,
                              double fill = Render::kFramingFill);

[[nodiscard]] bool
DeclaredPlacement(const Document &document, int cameraIndex, Viewpoint &out, std::string &error);

enum class TangentSource { None, Supplied, Generated };

struct VertexPlacement {
  const Transform &Node;
  const Transform *Skinned = nullptr;

  [[nodiscard]] const Transform &At(size_t vertex) const {
    return (Skinned != nullptr) ? Skinned[vertex] : Node;
  }
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

class Subject {
public:
  [[nodiscard]] bool Build(const Document &document, const VariantSelection &variant = {});

  [[nodiscard]] bool Build(const Document &document,
                           Span<const Transform> pose,
                           Span<const double> weights,
                           const VariantSelection &variant = {});

  [[nodiscard]] bool Assemble(const outshine::Geometry &what);

  [[nodiscard]] outshine::Geometry Handed() const;

  [[nodiscard]] outshine::Geometry Handed(const Document &naming) const;

private:
  [[nodiscard]] outshine::Geometry Handed(const Document *naming) const;

public:
  [[nodiscard]] bool Append(const Subject &other);

  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] const std::vector<double> &PositionsM() const { return Positions_; }

  [[nodiscard]] const std::vector<double> &Uv() const { return Uv_; }

  [[nodiscard]] bool HasUv() const { return !Uv_.empty(); }

  [[nodiscard]] const std::vector<double> &Uv1() const { return Uv1_; }

  [[nodiscard]] bool HasUv1() const { return !Uv1_.empty(); }

  [[nodiscard]] const std::vector<double> &Normals() const { return Normals_; }

  [[nodiscard]] bool HasNormal() const { return !Normals_.empty(); }

  [[nodiscard]] const std::vector<double> &Tangents() const { return Tangents_; }

  [[nodiscard]] bool HasTangent() const { return !Tangents_.empty(); }

  [[nodiscard]] const std::vector<double> &Colours() const { return Colours_; }

  [[nodiscard]] bool HasColour() const { return !Colours_.empty(); }

  [[nodiscard]] const std::vector<PlacedLight> &Lights() const { return Lights_; }

  [[nodiscard]] const std::vector<Material> &Surfaces() const { return Surfaces_; }

  [[nodiscard]] const std::vector<Part> &Parts() const { return Parts_; }

  struct Undrawn {
    size_t Primitives = 0;
    size_t ByMode[7] = {0, 0, 0, 0, 0, 0, 0};
  };

  [[nodiscard]] const Undrawn &NotDrawn() const { return Undrawn_; }

  [[nodiscard]] const std::vector<uint32_t> &Indices() const { return Indices_; }

  [[nodiscard]] size_t VertexCount() const { return Positions_.size() / 3; }

  [[nodiscard]] size_t TriangleCount() const { return Indices_.size() / 3; }

  void BoundsOf(size_t parts, double least[3], double most[3]) const;

  [[nodiscard]] const double *MinM() const { return Min_; }

  [[nodiscard]] const double *MaxM() const { return Max_; }

  [[nodiscard]] double RadiusM() const;
  void CentreM(double out[3]) const;

  [[nodiscard]] [[nodiscard]] bool Frame(Viewpoint &out, double fill = Render::kFramingFill) const;

  [[nodiscard]] double ProjectedAreaPx(const Transform &clip, const Viewport &viewport) const;

private:
  struct Scratch {
    outshine::Geometry Made;
    std::vector<float> Narrowed;
    std::vector<double> Pos, Nor, Uv, Uv1, Col, Tan;
    std::vector<uint32_t> Idx;
    std::vector<double> Elements, NodeWeights, Morphed, MorphedNormals, Coordinates, Tints,
        Directions;
    std::vector<uint32_t> Run, Loop;
    std::vector<Transform> Joints, Instances, Skinned;
    Scratch() = default;
    ~Scratch() = default;

    Scratch(const Scratch &) {}

    Scratch &operator=(const Scratch &) { return *this; }

    Scratch(Scratch &&) noexcept = default;
    Scratch &operator=(Scratch &&) noexcept = default;
  };

  mutable Scratch Scratch_;
  [[nodiscard]] bool Refuse(std::string why);

  [[nodiscard]] bool Flatten(const Document &document,
                             const Transform *pose,
                             const double *weights,
                             const VariantSelection &variant);

  void Bound();

  [[nodiscard]] bool MorphDeltasFor(const Document &document,
                                    const Primitive &primitive,
                                    const char *semantic,
                                    const double *weights,
                                    size_t count,
                                    size_t components,
                                    size_t vertices,
                                    std::vector<double> &out);
  [[nodiscard]] static Transform
  JointMatrix(const Skin &skin, size_t joint, const Transform &world);
  [[nodiscard]] bool BlendSkinFor(const Document &document,
                                  const Skin &skin,
                                  std::span<const Transform> joints,
                                  const Primitive &primitive,
                                  size_t vertices,
                                  std::vector<Transform> &out);

  [[nodiscard]] bool FlatNormalsFor(Part &part);
  [[nodiscard]] bool GeneratedTangentsFor(Part &part);

  [[nodiscard]] bool SuppliedTangentsFor(const Document &document,
                                         const Primitive &primitive,
                                         const VertexPlacement &place,
                                         Span<const double> morphWeights,
                                         Part &part,
                                         size_t vertices,
                                         std::vector<double> &into);
  [[nodiscard]] bool BuildTangentsFor(const Document &document,
                                      const Primitive &primitive,
                                      const VertexPlacement &place,
                                      Span<const double> morphWeights,
                                      Part &part,
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
  std::vector<Material> Surfaces_;
  std::vector<uint8_t> TangentWanted_;
  double Min_[3] = {0, 0, 0}, Max_[3] = {0, 0, 0};
};

} // namespace outshine::Gltf
#endif
