/* THE glTF 2.0 VOCABULARY, one enumeration per place the format uses an integer, and the records the
 * reader fills. The numbers are the format's own (they are what a file carries), so they are stated
 * once here and never appear again as a literal.
 *
 * AN ATTRIBUTE IS A NAME, NOT A SLOT. A primitive keeps whatever semantics its file carries --
 * JOINTS_0, TEXCOORD_1, COLOR_0 included -- because deciding which of them a vertex layout holds is
 * a question about the renderer and not about the file. The reader answers "what is in it"; nothing
 * here answers "what shape does it become". */
#ifndef GLTF_TYPES_H
#define GLTF_TYPES_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "Keyframes.h"

#include "Material.h"
#include "PunctualLight.h"

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

/* Rows and columns of an element, and the byte width of one component. The format's own alignment
 * rule -- every column of a matrix starts on a 4-byte boundary -- is applied where the stride is
 * computed, because it is a property of the pair and not of either half. */
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
  /* 0 means "the format did not say", which is tight packing -- not "stride zero". */
  size_t ByteStride = 0;
};

/* The compaction: a base run of elements, some of which are overridden by index. A sparse accessor
 * with no bufferView reads as zeros before the overrides are applied, which is what makes a mostly
 * empty morph target cost only what it changes. */
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

struct Primitive {
  std::vector<Attribute> Attributes;
  int Indices = -1;
  int Material = -1;
  PrimitiveMode Mode = PrimitiveMode::Triangles;

  /* -1 for "this file does not carry it". A caller that needs it refuses by name; nothing here
   * invents one (doc/requirements.md I.26). */
  int Find(const char *semantic) const;
};

struct Mesh {
  std::string Name;
  std::vector<Primitive> Primitives;
};

/* ONE ENTRY OF `KHR_lights_punctual`'s document-level table. The shading half is
 * `outshine::PunctualLight`, which is the engine's one vocabulary for a light the way
 * `outshine::Material` is for a surface; what is added here is only what is about the FILE. The
 * light carries no place of its own -- a node does, and `Position`/`Direction` stay at their
 * defaults until a consumer resolves the node that references it. */
struct LightRef {
  std::string Name;
  outshine::PunctualLight Light;
};

struct Node {
  std::string Name;
  std::vector<int> Children;
  int Mesh = -1;
  int Camera = -1;
  /* Into `Document::Lights()`, or -1. The extension puts the reference on the node and the beam on
   * the node's -Z, so a light's direction is the hierarchy's answer and never the table's. */
  int Light = -1;
  /* A node carries a matrix or a TRS triple, never both -- the format says so and the reader
   * refuses the file that does. */
  bool HasMatrix = false;
  double Matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  double Translation[3] = {0, 0, 0};
  double Rotation[4] = {0, 0, 0, 1}; /* xyzw, the format's order */
  double Scale[3] = {1, 1, 1};
};

struct Scene {
  std::string Name;
  std::vector<int> Roots;
};

/* WHAT A SAMPLER DOES AT THE EDGE OF THE IMAGE. glTF's own numbers, so the file's integers become
 * this enumeration once and never travel as integers (`Enum.2`). `TextureSettingsTest` renders one
 * cell per mode and a viewer that collapses two of them shows the wrong picture in exactly two of
 * its cells. */
enum class Wrap : uint16_t { ClampToEdge = 33071, MirroredRepeat = 33648, Repeat = 10497 };

/* Nearest or linear at magnification. `TextureLinearInterpolationTest` is about WHERE the sRGB
 * decode happens relative to this filter, not about which filter it is. */
enum class Filter : uint8_t { Nearest, Linear };

struct Sampler {
  Wrap WrapS = Wrap::Repeat;   /* the format's default when the field is absent */
  Wrap WrapT = Wrap::Repeat;
  Filter Mag = Filter::Linear;
  Filter Min = Filter::Linear;
};

/* AN IMAGE IS BYTES AND A DECLARED TYPE, never a decoded raster: this layer has no decoder and
 * naming one here would put an SDL dependency inside the format reader. `Uri` is already
 * PERCENT-DECODED -- the format requires reserved characters to be encoded on the way out
 * (`Specification.adoc:550`), so decoding them is this reader's obligation on the way in. */
struct Image {
  std::string Name;
  std::string Uri;        /* empty when the image lives in a bufferView */
  std::string MimeType;
  int View = -1;
};

struct Texture {
  std::string Name;
  int Source = -1;    /* into Images */
  int Sampler = -1;   /* -1 is the format's "use repeat and auto filtering" */
};

/* WHICH TEXTURE FEEDS WHICH SLOT, and which UV SET it reads. `TexCoord` is the number after
 * TEXCOORD_, so `MultiUVTest`'s logo declaring `texCoord: 1` is carried rather than collapsed. */
struct TextureRef {
  int Texture = -1;
  int TexCoord = 0;
  [[nodiscard]] bool Declared() const { return Texture >= 0; }
};

/* THE FILE'S MATERIAL. The shading half is `outshine::Material`, which is the engine's one
 * vocabulary for a surface and is glTF's own metal-rough parameterisation; what is added here is
 * only what is about the FILE -- which texture, which UV set, and whether the file says the surface
 * has two sides. */
struct MaterialRef {
  std::string Name;
  outshine::Material Surface;
  TextureRef BaseColour;
  TextureRef MetallicRoughness;
  TextureRef Normal;
  TextureRef Occlusion;
  TextureRef Emissive;
  double NormalScale = 1.0;
  double OcclusionStrength = 1.0;
};

/* HOW A SAMPLER GETS FROM ONE KEYFRAME TO THE NEXT IS `outshine::Keyframes::Interpolation` AND NOT A
 * SECOND ENUMERATION HERE. The evaluator in core already carries glTF's three words and glTF's own
 * triple layout for the spline, so a copy in this layer would be a second spelling of one meaning
 * that nothing keeps in step. What this layer owns is the REFUSAL: a fourth word is named and
 * rejected by the reader rather than falling back to `Linear`, which is what `InterpolationTest`
 * exists to catch a viewer doing.
 *
 * `CubicSpline` CHANGES THE SHAPE OF THE OUTPUT and not only the arithmetic: its output accessor
 * carries THREE elements per keyframe -- in-tangent, value, out-tangent -- so a reader that treated
 * the word as a hint would read a third of the animation and see two thirds of it as keyframes. */
using Interpolation = outshine::Keyframes::Interpolation;

/* WHAT A CHANNEL DRIVES. `Weights` is carried because the format has it and a file that uses it must
 * not read as something else; nothing in this tree consumes it yet, and that is a fact about the
 * consumer rather than about the reader. */
enum class AnimationPath : uint8_t { Translation, Rotation, Scale, Weights };

/* THE COMPONENTS ONE KEYFRAME OF A PATH CARRIES: 3 for a translation or a scale, 4 for a rotation
 * quaternion. `Weights` is as many as the mesh has morph targets and is therefore not answerable
 * from the path alone -- 0 says so, rather than a number that would be wrong. */
size_t PathComponents(AnimationPath path);

/* ONE SAMPLER: a time grid, the values on it, and how to get between them. `Input` and `Output` are
 * accessor indices, so the decode is `Document::ReadElements` like every other run in the file and
 * this record holds no bytes of its own. */
struct AnimationSampler {
  int Input = -1;
  int Output = -1;
  Interpolation How = Interpolation::Linear;
};

/* ONE CHANNEL: which sampler drives which property of which node. `Node` may be -1, which the format
 * allows and defines as a channel to be IGNORED rather than an error. */
struct AnimationChannel {
  int Sampler = -1;
  int Node = -1;
  AnimationPath Path = AnimationPath::Translation;
};

struct Animation {
  std::string Name;
  std::vector<AnimationSampler> Samplers;
  std::vector<AnimationChannel> Channels;
};

/* WHAT A SUBJECT DOES NOT CARRY, NAMED. The empty string means it carries all of them; anything else
 * is the refusal a case prints and stops on. There is no arm that derives a missing semantic --
 * doc/requirements.md I.26 forbids it, and the Khronos `Triangle` having no NORMAL is a property of
 * the subject, recorded, not repaired. */
std::string MissingSemantics(const Primitive &primitive,
                             std::initializer_list<const char *> required);

} // namespace outshine::Gltf
#endif
