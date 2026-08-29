#include "Shaped.h"

namespace outshine::Gltf {
namespace {

// A CHANNEL A PRODUCER DID NOT WRITE IS AN EMPTY SPAN, NEVER A SHORT ONE. Reaching past the end
// of a stream that was never filled has to answer nothing rather than a fragment, because a part
// carrying half a channel would pack half a vertex and the device would read the neighbour's.
template <typename T>
[[nodiscard]] std::span<const T> Reach(const std::vector<T> &whole, size_t from, size_t count) {
  if (count == 0 || from + count > whole.size()) { return {}; }
  return std::span<const T>(whole.data() + from, count);
}

}

Render::Shape Shaped(const Subject &from, Render::ShapeStore &into) {
  into.Clear();
  const auto narrow = [](const std::vector<double> &wide, std::vector<float> &held) {
    held.reserve(wide.size());
    for (const double value : wide) { held.push_back((float)value); }
  };
  narrow(from.PositionsM(), into.PositionsM);
  narrow(from.Normals(), into.Normals);
  narrow(from.Tangents(), into.Tangents);
  narrow(from.Uv(), into.Uv);
  narrow(from.Uv1(), into.Uv1);
  narrow(from.Colours(), into.Colours);
  into.Indices.assign(from.Indices().begin(), from.Indices().end());
  for (const PlacedLight &placed : from.Lights()) { into.Lamps.push_back(placed.Light); }

  into.Parts.reserve(from.Parts().size());
  for (const Part &one : from.Parts()) {
    Render::ShapePart made;
    made.Name = one.NodeName;
    made.Material = one.Material;
    made.HasUv = one.HasUv;
    made.HasUv1 = one.HasUv1;
    made.HasNormal = one.HasNormal;
    made.HasColour = one.HasColour;
    made.HasTangent = one.HasTangent();
    made.FirstVertex = one.FirstVertex;
    made.VertexCount = one.VertexCount;
    made.FirstIndex = one.FirstIndex;
    made.IndexCount = one.IndexCount;
    made.PositionsM = Reach(into.PositionsM, one.FirstVertex * 3, one.VertexCount * 3);
    made.Normals = Reach(into.Normals, one.FirstVertex * 3, one.VertexCount * 3);
    made.Tangents = Reach(into.Tangents, one.FirstVertex * 4, one.VertexCount * 4);
    made.Uv = Reach(into.Uv, one.FirstVertex * 2, one.VertexCount * 2);
    made.Uv1 = Reach(into.Uv1, one.FirstVertex * 2, one.VertexCount * 2);
    made.Colours = Reach(into.Colours, one.FirstVertex * 4, one.VertexCount * 4);
    into.Parts.push_back(made);
  }
  Render::Shape out;
  out.Parts = into.Parts;
  out.Surfaces = from.Surfaces();
  out.Lamps = into.Lamps;
  out.Indices = into.Indices;
  out.CarriesUv = from.HasUv();
  out.CarriesUv1 = from.HasUv1();
  out.CarriesNormal = from.HasNormal();
  out.CarriesTangent = from.HasTangent();
  out.CarriesColour = from.HasColour();
  return out;
}

}
