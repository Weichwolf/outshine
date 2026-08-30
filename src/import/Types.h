#ifndef OUTSHINE_IMPORT_TYPES_H
#define OUTSHINE_IMPORT_TYPES_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "Span.h"

#include "Keyframes.h"

#include "Material.h"
#include "PunctualLight.h"
#include "UvTransform.h"

namespace outshine::Gltf {

enum class ComponentType : uint16_t {
  Int8 = 5120,
  UInt8 = 5121,
  Int16 = 5122,
  UInt16 = 5123,
  UInt32 = 5125,
  Float32 = 5126,
};

enum class ElementType : uint8_t { Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4 };

enum class PrimitiveMode : uint8_t {
  Points = 0,
  Lines = 1,
  LineLoop = 2,
  LineStrip = 3,
  Triangles = 4,
  TriangleStrip = 5,
  TriangleFan = 6,
};

enum class CameraKind : uint8_t { Perspective, Orthographic };

size_t ComponentBytes(ComponentType component);
size_t ElementRows(ElementType element);
size_t ElementColumns(ElementType element);

inline size_t ElementComponents(ElementType element) {
  return ElementRows(element) * ElementColumns(element);
}

size_t TightElementBytes(ElementType element, ComponentType component);

struct BufferView {
  size_t Buffer = 0;
  size_t ByteOffset = 0;
  size_t ByteLength = 0;

  size_t ByteStride = 0;
};

struct SparseOverride {
  size_t Count = 0;
  int IndicesBufferView = -1;
  size_t IndicesByteOffset = 0;
  ComponentType IndicesComponent = ComponentType::UInt32;
  int ValuesBufferView = -1;
  size_t ValuesByteOffset = 0;
};

struct Accessor {
  int View = -1;
  size_t ByteOffset = 0;
  ComponentType Component = ComponentType::Float32;
  ElementType Element = ElementType::Scalar;
  size_t Count = 0;
  bool Normalized = false;
  bool HasSparse = false;
  SparseOverride Sparse;
  std::vector<double> Min, Max;
};

struct Attribute {
  std::string Semantic;
  int Accessor = -1;
};

struct MorphTarget {
  std::vector<Attribute> Attributes;

  int Find(const char *semantic) const;
};

struct Primitive {
  std::vector<Attribute> Attributes;

  std::vector<MorphTarget> Targets;
  int Indices = -1;
  int Material = -1;
  PrimitiveMode Mode = PrimitiveMode::Triangles;

  std::vector<int> VariantMaterials;

  int Find(const char *semantic) const;

  int MaterialUnder(int variant) const;
};

struct Mesh {
  std::string Name;
  std::vector<Primitive> Primitives;

  std::vector<double> Weights;
};

struct LightRef {
  std::string Name;
  outshine::PunctualLight Light;
};

struct Node {
  std::string Name;
  std::vector<int> Children;
  int Mesh = -1;
  int Camera = -1;

  int Light = -1;

  bool HasMatrix = false;
  double Matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  double Translation[3] = {0, 0, 0};
  double Rotation[4] = {0, 0, 0, 1};
  double Scale[3] = {1, 1, 1};

  int InstanceTranslation = -1;
  int InstanceRotation = -1;
  int InstanceScale = -1;

  bool Visible = true;

  int Skin = -1;
};

struct Skin {
  std::string Name;
  std::vector<int> Joints;
  int Skeleton = -1;

  std::vector<double> InverseBind;
};

struct Scene {
  std::string Name;
  std::vector<int> Roots;
};

enum class Wrap : uint16_t { ClampToEdge = 33071, MirroredRepeat = 33648, Repeat = 10497 };

enum class Filter : uint8_t { Nearest, Linear };

enum class MipFilter : uint8_t { None, Nearest, Linear };

struct Sampler {
  Wrap WrapS = Wrap::Repeat;
  Wrap WrapT = Wrap::Repeat;
  Filter Mag = Filter::Linear;
  Filter Min = Filter::Linear;
  MipFilter Mip = MipFilter::Linear;
};

enum class MetadataShape : uint8_t { Text, Structure };

enum class MetadataCarrier : uint8_t { Asset, Scene, Node, Mesh, Material, Image, Animation };

struct MetadataUse {
  MetadataCarrier Carrier = MetadataCarrier::Asset;
  uint32_t Which = 0;
  uint32_t Packet = 0;
};

static_assert(sizeof(MetadataUse) == 12);
static_assert(std::is_trivially_copyable_v<MetadataUse>);

struct MetadataProperty {
  std::string Key;
  std::string Value;
  MetadataShape Shape = MetadataShape::Text;
};

struct MetadataPacket {
  std::vector<MetadataProperty> Held;

  [[nodiscard]] std::optional<std::string_view> Of(std::string_view key) const {
    for (const MetadataProperty &one : Held) {
      if (one.Key != key) { continue; }
      if (one.Shape != MetadataShape::Text) { return std::nullopt; }
      return std::string_view(one.Value);
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<std::string_view> SourceOf(std::string_view key) const {
    for (const MetadataProperty &one : Held) {
      if (one.Key != key) { continue; }
      return std::string_view(one.Value);
    }
    return std::nullopt;
  }

  [[nodiscard]] bool Carries(std::string_view key) const {
    for (const MetadataProperty &one : Held) {
      if (one.Key == key) { return true; }
    }
    return false;
  }
};

struct Image {
  std::string Name;
  std::string Uri;
  std::string MimeType;
  int View = -1;
};

struct Texture {
  std::string Name;
  int Source = -1;
  int Sampler = -1;
};

struct TextureRef {
  int Texture = -1;
  int TexCoord = 0;
  outshine::UvTransform Transform;

  [[nodiscard]] bool Declared() const { return Texture >= 0; }
};

struct MaterialRef {
  std::string Name;
  outshine::Material Surface;
  TextureRef BaseColour;
  TextureRef MetallicRoughness;
  TextureRef Normal;
  TextureRef Occlusion;
  TextureRef Emissive;

  TextureRef SpecularStrength;
  TextureRef SpecularTint;
  double NormalScale = 1.0;
  double OcclusionStrength = 1.0;
};

using Interpolation = outshine::Keyframes::Interpolation;

enum class AnimationPath : uint8_t { Translation, Rotation, Scale, Weights, MaterialFactor };

enum class MaterialFactor : uint8_t { BaseColour, Metalness, Roughness, Emissive };

struct AnimatablePointer {
  const char *Tail;
  MaterialFactor Factor;
};

Span<const AnimatablePointer> AnimatablePointers();

enum class UndrivenReason : uint8_t {

  PointerUnparsed,

  PointerUnheld,
};

struct UndrivenChannel {
  std::string Pointer;
  UndrivenReason Why = UndrivenReason::PointerUnparsed;
};

size_t FactorComponents(MaterialFactor factor);

size_t PathComponents(AnimationPath path);

struct AnimationSampler {
  int Input = -1;
  int Output = -1;
  Interpolation How = Interpolation::Linear;
};

struct AnimationChannel {
  int Sampler = -1;
  int Node = -1;
  AnimationPath Path = AnimationPath::Translation;

  int Material = -1;
  MaterialFactor Factor = MaterialFactor::BaseColour;
};

struct Animation {
  std::string Name;
  std::vector<AnimationSampler> Samplers;
  std::vector<AnimationChannel> Channels;

  std::vector<UndrivenChannel> Undriven;
};

outshine::Material DefaultMaterial();

[[nodiscard]] bool
VertexColourComponents(const Accessor &accessor, size_t &components, std::string &why);

std::string MissingSemantics(const Primitive &primitive,
                             std::initializer_list<const char *> required);

enum class CarriedUvSets { FirstOnly, Both };

[[nodiscard]] bool UvSetOf(const TextureRef &reference,
                           CarriedUvSets carried,
                           const char *socket,
                           UvSet &out,
                           std::string &why);

}
#endif
