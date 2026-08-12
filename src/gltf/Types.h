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

struct Node {
  std::string Name;
  std::vector<int> Children;
  int Mesh = -1;
  int Camera = -1;
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

/* WHAT A SUBJECT DOES NOT CARRY, NAMED. The empty string means it carries all of them; anything else
 * is the refusal a case prints and stops on. There is no arm that derives a missing semantic --
 * doc/requirements.md I.26 forbids it, and the Khronos `Triangle` having no NORMAL is a property of
 * the subject, recorded, not repaired. */
std::string MissingSemantics(const Primitive &primitive,
                             std::initializer_list<const char *> required);

} // namespace outshine::Gltf
#endif
