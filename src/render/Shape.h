#ifndef OUTSHINE_RENDER_SHAPE_H
#define OUTSHINE_RENDER_SHAPE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace outshine::Render {

// WHAT THE RENDERER NEEDS OF A GEOMETRY, AND NOTHING ELSE. The render tier held the importer's own carrier --
// the glTF importer's own carrier -- and reached into it fourteen times for streams, parts, counts
// and bounds. An interchange format's carrier is not a render input: the tier that must not care
// what it is drawing had a type that knows what FILE it came from.
//
// SPANS RATHER THAN A COPY. This is a VIEW the engine fills over whatever it holds, so inverting the
// dependency costs no bytes -- the importer knows the engine, the engine fills a Shape, and the
// renderer never learns where any of it came from. That is the arrow Unreal has (the glTF importer
// is a module DEPENDING on the engine) and RAGE has (tools depend on the runtime, never the
// reverse).
//
// WHAT IT DOES NOT DECIDE: it says nothing about the LAYOUT of those streams. They are still
// separate runs of double here because the producer still writes them that way; the goal's other
// half -- that a vertex reaches the GPU without being reshaped -- is a change to what the producer
// writes, not to who looks at it.
struct ShapePart {
  // WHAT THE PART IS CALLED, kept because two refusals name it and a refusal that cannot say WHICH
  // part is half a refusal. A name is not a glTF idea -- a generator names its parts too
  // (`addPart("roofs", ...)`) -- so this is the engine's own word, viewed rather than copied.
  std::string_view Name;
  int Material = -1;
  bool HasUv = false;
  bool HasUv1 = false;
  bool HasNormal = false;
  bool HasColour = false;
  bool HasTangent = false;
  size_t FirstVertex = 0;
  size_t VertexCount = 0;
  size_t FirstIndex = 0;
  size_t IndexCount = 0;
};

struct Shape {
  std::span<const ShapePart> Parts;
  std::span<const double> PositionsM;
  std::span<const double> Normals;
  std::span<const double> Tangents;
  std::span<const double> Uv;
  std::span<const double> Uv1;
  std::span<const double> Colours;
  std::span<const uint32_t> Indices;

  bool CarriesUv = false;
  bool CarriesUv1 = false;
  bool CarriesNormal = false;
  bool CarriesTangent = false;
  bool CarriesColour = false;

  [[nodiscard]] size_t VertexCount() const { return PositionsM.size() / 3; }
  [[nodiscard]] size_t TriangleCount() const { return Indices.size() / 3; }
  [[nodiscard]] bool Empty() const { return Parts.empty() || Indices.empty(); }

  // THE EXTENT OF THE PARTS FROM `first` ONWARD, which is the one piece of arithmetic the render
  // tier used to reach into the importer's carrier for.
  void BoundsOf(size_t first, double leastM[3], double mostM[3]) const;
};

}
#endif
