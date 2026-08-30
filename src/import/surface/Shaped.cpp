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

namespace {

void FillFrom(const Subject &from, Render::ShapeStore &into) {
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
  for (const Material &surface : from.Surfaces()) { into.Surfaces.push_back(surface); }
}


void FillFrom(const outshine::Geometry &from, Render::ShapeStore &into) {
  const int parts = from.parts();
  const uint32_t firstSurface = (uint32_t)into.Surfaces.size();
  size_t wholeIndices = 0;
  for (int part = 0; part < parts; ++part) { wholeIndices += from.trianglesOf(part).size(); }
  into.Indices.reserve(wholeIndices);
  for (int surface = 0; surface < from.surfaces(); ++surface) {
    into.Surfaces.push_back(from.surfaceAt(MaterialInstance(surface)));
  }
  for (int lamp = 0; lamp < from.lamps(); ++lamp) {
    PunctualLight standing = from.lampAt(lamp);
    const double *const at = from.lampPlacementOf(lamp);
    for (int axis = 0; axis < 3; ++axis) { standing.Position[axis] = (float)at[12 + axis]; }
    into.Lamps.push_back(standing);
  }

  into.Parts.reserve(into.Parts.size() + (size_t)parts);
  size_t firstVertex = into.Parts.empty()
                           ? 0u
                           : into.Parts.back().FirstVertex + into.Parts.back().VertexCount;
  size_t firstIndex = into.Indices.size();
  for (int part = 0; part < parts; ++part) {
    Render::ShapePart made;
    made.Name = from.nameOf(part);
    const int wears = from.materialOf(part).index();
    made.Material = wears < 0 ? -1 : (int)firstSurface + wears;
    made.PositionsM = from.positionsOf(part);
    made.Normals = from.normalsOf(part);
    made.Tangents = from.tangentsOf(part);
    made.Uv = from.textureOf(part, 0);
    made.Uv1 = from.textureOf(part, 1);
    made.Colours = from.coloursOf(part);
    made.HasUv = !made.Uv.empty();
    made.HasUv1 = !made.Uv1.empty();
    made.HasNormal = !made.Normals.empty();
    made.HasColour = !made.Colours.empty();
    made.HasTangent = !made.Tangents.empty();
    made.VertexCount = made.PositionsM.size() / 3;
    made.FirstVertex = firstVertex;
    const std::span<const uint32_t> order = from.trianglesOf(part);
    made.FirstIndex = firstIndex;
    made.IndexCount = order.size();
    for (const uint32_t index : order) { into.Indices.push_back((uint32_t)firstVertex + index); }
    firstVertex += made.VertexCount;
    firstIndex += order.size();
    into.Parts.push_back(made);
  }
}

// THE STORE IS VIEWED ONCE, AFTER EVERYTHING IS IN IT. A part's channel spans point into the
// store's own arrays, so taking them before the last source has been appended would leave them
// pointing at freed memory the moment a vector grew.
Render::Shape Viewed(Render::ShapeStore &into) {
  Render::CookShape(into, into.Surfaces);
  Render::Shape out;
  out.Parts = into.Parts;
  out.Surfaces = into.Surfaces;
  out.Lamps = into.Lamps;
  out.Indices = into.Indices;
  out.Clusters = into.Clusters;
  out.ClusterIndices = into.ClusterIndices;
  for (const Render::ShapePart &one : into.Parts) {
    out.CarriesUv = out.CarriesUv || one.HasUv;
    out.CarriesUv1 = out.CarriesUv1 || one.HasUv1;
    out.CarriesNormal = out.CarriesNormal || one.HasNormal;
    out.CarriesTangent = out.CarriesTangent || one.HasTangent;
    out.CarriesColour = out.CarriesColour || one.HasColour;
  }
  return out;
}

}

Render::Shape Shaped(const Subject &from, Render::ShapeStore &into) {
  into.Clear();
  FillFrom(from, into);
  return Viewed(into);
}

Render::Shape Shaped(const outshine::Geometry &from, Render::ShapeStore &into) {
  into.Clear();
  FillFrom(from, into);
  return Viewed(into);
}

// A DRIVEN SUBJECT AND THE WORLD IT STANDS IN ARE ONE SHAPE. The subject's parts come FIRST and
// `Live::Carrying_` says how many they are, which is what bounds the shadow radius and what the
// placement rows address. Handing the world alone dropped the subject out of the picture entirely:
// the proxy stood over the world's parts while the surface table still named the file's, and
// `SubjectProxy::Wears` refused -- measured on the drive scenario, 3 parts against a slot for 9,
// which is every frame of it since the world path started handing a `Geometry`.
Render::Shape Shaped(const Subject &from, const outshine::Geometry &also,
                     Render::ShapeStore &into) {
  into.Clear();
  FillFrom(from, into);
  FillFrom(also, into);
  return Viewed(into);
}

}
