#ifndef OUTSHINE_RENDER_SHAPE_H
#define OUTSHINE_RENDER_SHAPE_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <Material.h>
#include <PunctualLight.h>
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
// THE STREAMS ARE THE PRODUCER'S OWN AND THEY ARE PER PART. A generator writes float through the
// door (`setPositions(part, span<const float>)`) and the device buffer is PLANAR -- one contiguous
// run per channel over the whole subject -- so the packer walks parts inside each run either way.
// Concatenating the channels first bought nothing and cost a widening to double and a narrowing
// back: 2437 ms assembling and 2708 ms packing 28 M vertices on Shibuya, with double the memory
// standing between. A whole-subject FLOAT view was measured too and was WORSE (Shibuya 30.1 -> 36.9
// s), because it adds a pass rather than removing one -- the concatenation is the cost, not the
// conversion. So the parts carry their own spans and nothing is concatenated.
//
// INDICES ARE THE EXCEPTION and stay whole-subject. They are uint32_t on both sides, so joining
// them is a copy with an offset rather than a conversion, and the compiled draw runs address them
// by one global first-index.
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

  std::span<const float> PositionsM;
  std::span<const float> Normals;
  std::span<const float> Tangents;
  std::span<const float> Uv;
  std::span<const float> Uv1;
  std::span<const float> Colours;
};

struct Shape {
  std::span<const ShapePart> Parts;
  std::span<const uint32_t> Indices;

  // THE SURFACE ROWS THE PARTS INDEX INTO. `Material` is the DOOR's type -- a generator declares
  // one through `addSurface` and a file's importer resolves one into the same row -- so carrying
  // them here costs the render side nothing and lets the surface resolution stop naming the
  // importer's carrier for a part count it could have had from anywhere.
  std::span<const Material> Surfaces;

  // THE LAMPS A PRODUCER PLACED, in the door's own type. A generator declares one through
  // `addLamp` and a document's importer resolves one into the same row, so the renderer takes
  // them from here and never asks which of the two put them there.
  std::span<const PunctualLight> Lamps;

  bool CarriesUv = false;
  bool CarriesUv1 = false;
  bool CarriesNormal = false;
  bool CarriesTangent = false;
  bool CarriesColour = false;

  // THE PARTS TILE THE VERTEX NUMBERING, so the end of the last one is the count and no second
  // field can disagree with the parts about it.
  [[nodiscard]] size_t VertexCount() const {
    return Parts.empty() ? 0u : Parts.back().FirstVertex + Parts.back().VertexCount;
  }
  [[nodiscard]] size_t TriangleCount() const { return Indices.size() / 3; }
  [[nodiscard]] bool Empty() const { return Parts.empty() || Indices.empty(); }

  // THE EXTENT OF THE FIRST `parts` PARTS, and of the WHOLE when that is 0 or reaches every part.
  // This is the importer's carrier's own contract and it is kept to the letter, because three call
  // sites read it: a fitted camera, a near-plane refusal and a shadow radius. A version that
  // SKIPPED the first `parts` instead compiled, ran, and answered an empty extent -- the refusal
  // "the subject has no extent over its own grid" is what a wrong reading of one parameter looks
  // like from four cases away.
  void BoundsOf(size_t parts, double leastM[3], double mostM[3]) const;
};

// WHERE A SHAPE'S OWN BYTES LIVE WHEN THE PRODUCER HAS NONE TO POINT AT. A generator's `Geometry`
// already holds float per part, so a world Shape views it and this store carries only the joined
// indices. A document read from a file holds double, so its shape narrows ONCE into here and the
// parts point at that -- the same single conversion the packer used to do, moved to where it can
// be done once per rebuild rather than once per frame.
struct ShapeStore {
  std::vector<ShapePart> Parts;
  std::vector<float> PositionsM;
  std::vector<float> Normals;
  std::vector<float> Tangents;
  std::vector<float> Uv;
  std::vector<float> Uv1;
  std::vector<float> Colours;
  std::vector<uint32_t> Indices;
  std::vector<Material> Surfaces;
  std::vector<PunctualLight> Lamps;

  void Clear() {
    Parts.clear();
    PositionsM.clear();
    Normals.clear();
    Tangents.clear();
    Uv.clear();
    Uv1.clear();
    Colours.clear();
    Indices.clear();
    Surfaces.clear();
    Lamps.clear();
  }
};

}
#endif
