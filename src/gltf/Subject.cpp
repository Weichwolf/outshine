#include "Subject.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>

#include "Document.h"
#include "Framing.h"
#include "Tangents.h"

namespace outshine::Gltf {

namespace {

constexpr double kPi = 3.14159265358979323846;

void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

double Length(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

[[nodiscard]] bool Normalise(double v[3]) {
  const double length = Length(v);
  if (!(length > 0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

/* AN ATTRIBUTE RUN A PRODUCER STATES, against the vertex count its positions already fixed. The
 * finiteness check is here and not at the reader's end because a NaN in a file is a byte somebody
 * wrote and a NaN in a produced run is arithmetic that went wrong -- caught at the handover, it
 * names the piece; caught in the picture, it is a hole. */
[[nodiscard]] bool RunIsStatable(Span<const float> run, size_t vertices, size_t components,
                                 const char *semantic, const std::string &where, std::string &why) {
  if (run.Empty()) { return true; }
  if (run.Size() != vertices * components) {
    why = where + " states " + std::to_string(run.Size() / components) + " " + semantic + " over " +
          std::to_string(vertices) + " vertices";
    return false;
  }
  for (size_t at = 0; at < run.Size(); ++at) {
    if (!std::isfinite(run[at])) {
      why = where + " states a " + semantic + " component that is not finite, at " +
            std::to_string(at);
      return false;
    }
  }
  return true;
}

const char *ModeName(PrimitiveMode mode) {
  switch (mode) {
  case PrimitiveMode::Points: return "POINTS";
  case PrimitiveMode::Lines: return "LINES";
  case PrimitiveMode::LineLoop: return "LINE_LOOP";
  case PrimitiveMode::LineStrip: return "LINE_STRIP";
  case PrimitiveMode::Triangles: return "TRIANGLES";
  case PrimitiveMode::TriangleStrip: return "TRIANGLE_STRIP";
  case PrimitiveMode::TriangleFan: return "TRIANGLE_FAN";
  }
  return "an undeclared mode";
}

bool DrawsASurface(PrimitiveMode mode) {
  return mode == PrimitiveMode::Triangles || mode == PrimitiveMode::TriangleStrip ||
         mode == PrimitiveMode::TriangleFan;
}

/* Whether `run` can be walked as this mode at all: three indices to a triangle for a list, three to
 * start a run for a strip or a fan. */
bool RunIsWhole(PrimitiveMode mode, size_t indices) {
  return (mode == PrimitiveMode::Triangles) ? (indices > 0 && indices % 3 == 0) : (indices >= 3);
}

/* WHAT A NODE'S TRANSFORM DOES TO THE ORDER OF ITS TRIANGLES. A mirroring transform turns a
 * front face into a back one, so the format restates the winding rather than the geometry
 * (Specification.adoc:1734). It is an argument of the triangulation and not a step after it, so a
 * path that produces triangles without deciding this has no spelling (`Enum.2`). */
enum class Handedness { Preserved, Reversed };

/* THE ODD TRIANGLE OF A STRIP IS (i+1, i, i+2), NOT A SLIDING WINDOW: without the swap every second
 * triangle of the run would face the other way, which is the format's rule and not a convention this
 * file chooses. A fan is (0, i, i+1) throughout and needs no such swap. Assumes `RunIsWhole`. */
void Triangulate(PrimitiveMode mode, Handedness handedness, const std::vector<uint32_t> &run,
                 std::vector<uint32_t> &out) {
  out.clear();
  if (mode == PrimitiveMode::Triangles) {
    out = run;
  } else if (mode == PrimitiveMode::TriangleStrip) {
    for (size_t at = 0; at + 2 < run.size(); ++at) {
      const size_t flipped = (at % 2 == 0) ? 0u : 1u;
      out.push_back(run[at + flipped]);
      out.push_back(run[at + 1u - flipped]);
      out.push_back(run[at + 2u]);
    }
  } else {
    for (size_t at = 1; at + 1 < run.size(); ++at) {
      out.push_back(run[0]);
      out.push_back(run[at]);
      out.push_back(run[at + 1]);
    }
  }
  if (handedness == Handedness::Reversed) {
    for (size_t triangle = 0; triangle * 3 + 2 < out.size(); ++triangle) {
      std::swap(out[triangle * 3 + 1], out[triangle * 3 + 2]);
    }
  }
}

/* THE FOUR NUMBERS OF ONE BASIS AS A KEY, so that "these two corners hold the same basis" is an
 * exact question. Any bound below which two bases counted as one would be a tolerance nobody
 * derived, and the price of having none is a duplicated vertex where two bases differ in their last
 * bit -- which costs memory and never a picture. */
struct BasisKey {
  uint64_t Bits[4] = {};
  bool operator<(const BasisKey &other) const {
    return std::memcmp(Bits, other.Bits, sizeof Bits) < 0;
  }
};

BasisKey KeyOf(const double basis[4]) {
  BasisKey key;
  for (size_t at = 0; at < 4; ++at) {
    const double folded = basis[at] == 0.0 ? 0.0 : basis[at];
    std::memcpy(&key.Bits[at], &folded, sizeof key.Bits[at]);
  }
  return key;
}

} // namespace

bool Subject::BuildTangentsFor(const Document &document, const Primitive &primitive,
                               const Transform &world, Part &part, size_t vertices) {
  Tangents_.resize((Positions_.size() / 3) * 4, 0.0);

  const int supplied = primitive.Find("TANGENT");
  if (supplied >= 0) {
    std::vector<double> elements;
    if (!document.ReadElements(supplied, elements)) {
      return Refuse(document.Path() + ": TANGENT does not decode: " + document.Error());
    }
    if (elements.size() != vertices * 4) {
      return Refuse(document.Path() + ": TANGENT decodes to " + std::to_string(elements.size() / 4) +
                    " vectors over " + std::to_string(vertices) + " vertices");
    }
    /* A TANGENT TRANSFORMS LIKE A DIRECTION IN THE SURFACE and not like the normal: it lies IN the
     * tangent plane, so the node's linear part carries it and the inverse transpose would tilt it
     * out of the plane on any non-uniform scale. */
    const double mirrored = world.LinearDeterminant() < 0 ? -1.0 : 1.0;
    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      const double local[3] = {elements[vertex * 4], elements[vertex * 4 + 1],
                               elements[vertex * 4 + 2]};
      double global[3];
      world.Direction(local, global);
      (void)Normalise(global);
      for (int axis = 0; axis < 3; ++axis) {
        Tangents_[(part.FirstVertex + vertex) * 4 + (size_t)axis] = global[axis];
      }
      Tangents_[(part.FirstVertex + vertex) * 4 + 3] = elements[vertex * 4 + 3] * mirrored;
    }
    part.Tangent = TangentSource::Supplied;
    return true;
  }

  /* THE THREE CONDITIONS THE FORMAT PUTS ON GENERATING ONE, and all three are the file's rather than
   * this reader's: the material must actually sample a normal map, and the primitive must carry the
   * normal and the uv set the algorithm is defined over. A basis generated where nothing reads it is
   * an attribute the subject did not declare. */
  const bool needed = part.Material >= 0 && (size_t)part.Material < document.Materials().size() &&
                      document.Materials()[(size_t)part.Material].Normal.Texture >= 0;
  if (!needed || !part.HasNormal || !part.HasUv || part.IndexCount == 0) { return true; }

  TangentSubject over;
  over.PositionsM = Positions_.data();
  over.Normals = Normals_.data();
  over.Uv = Uv_.data();
  over.VertexCount = VertexCount();
  over.Indices = Indices_.data() + part.FirstIndex;
  over.IndexCount = part.IndexCount;
  std::vector<double> corners;
  std::string error;
  if (!GenerateTangents(over, corners, error)) {
    return Refuse(document.Path() + ": the tangent basis the material needs cannot be generated: " +
                  error);
  }

  std::map<BasisKey, uint32_t> split;
  std::vector<char> written(VertexCount(), 0);
  for (size_t corner = 0; corner < part.IndexCount; ++corner) {
    const uint32_t vertex = Indices_[part.FirstIndex + corner];
    const double *basis = &corners[corner * 4];
    if (!written[vertex]) {
      for (size_t at = 0; at < 4; ++at) { Tangents_[vertex * 4 + at] = basis[at]; }
      written[vertex] = 1;
      continue;
    }
    if (std::memcmp(&Tangents_[vertex * 4], basis, 4 * sizeof(double)) == 0) { continue; }
    BasisKey key = KeyOf(basis);
    /* The vertex is part of the key, so two vertices that happen to want one basis do not collapse
     * into each other -- they are different points of the surface and only their tangent agrees. */
    key.Bits[0] ^= (uint64_t)vertex * 0x9e3779b97f4a7c15ull;
    const auto found = split.find(key);
    if (found != split.end()) {
      Indices_[part.FirstIndex + corner] = found->second;
      continue;
    }
    const uint32_t made = (uint32_t)VertexCount();
    for (size_t axis = 0; axis < 3; ++axis) {
      Positions_.push_back(Positions_[vertex * 3 + axis]);
    }
    Uv_.resize((size_t)made * 2 + 2, 0.0);
    Uv_[(size_t)made * 2] = Uv_[vertex * 2];
    Uv_[(size_t)made * 2 + 1] = Uv_[vertex * 2 + 1];
    Normals_.resize((size_t)made * 3 + 3, 0.0);
    for (size_t axis = 0; axis < 3; ++axis) {
      Normals_[(size_t)made * 3 + axis] = Normals_[vertex * 3 + axis];
    }
    Tangents_.resize((size_t)made * 4 + 4, 0.0);
    for (size_t at = 0; at < 4; ++at) { Tangents_[(size_t)made * 4 + at] = basis[at]; }
    split.emplace(key, made);
    Indices_[part.FirstIndex + corner] = made;
  }
  part.Tangent = TangentSource::Generated;
  return true;
}

bool Placement::LookAt(const double eyeM[3], const double aimM[3], double rollRad, Placement &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Normalise(forward)) { return false; }
  const double worldUp[3] = {0, 1, 0};
  double right[3];
  Cross(forward, worldUp, right);
  if (!Normalise(right)) { return false; }
  double up[3];
  Cross(right, forward, up);

  const double turn = std::cos(rollRad);
  const double lean = std::sin(rollRad);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis] * turn + up[axis] * lean;
    out.Up[axis] = up[axis] * turn - right[axis] * lean;
  }
  return true;
}

bool Placement::View(Transform &out) const {
  Transform world;
  for (int axis = 0; axis < 3; ++axis) {
    world.M[axis] = Right[axis];
    world.M[4 + axis] = Up[axis];
    world.M[8 + axis] = -Forward[axis];
    world.M[12 + axis] = EyeM[axis];
  }
  world.M[3] = world.M[7] = world.M[11] = 0;
  world.M[15] = 1;
  return world.Inverse(out);
}

bool Placement::Clip(double viewportAspect, Transform &out) const {
  Camera lens;
  lens.Kind = Kind;
  lens.YfovRad = YfovRad;
  lens.XMagM = XMagM;
  lens.YMagM = YMagM;
  lens.ZNearM = ZNearM;
  lens.ZFarM = ZFarM;
  Transform projection, view;
  if (!lens.Projection(viewportAspect, projection)) { return false; }
  if (!View(view)) { return false; }
  out = projection * view;
  return true;
}

/* A REFUSED SUBJECT CARRIES NO RUN AT ALL, and every run goes rather than the two that used to:
 * `HasUv`, `HasNormal` and `HasTangent` answer from their own run's emptiness, so leaving one of
 * them populated over zero vertices spells a subject that has a tangent for a vertex it does not
 * have. */
bool Subject::Refuse(const std::string &why) {
  Error_ = why;
  Positions_.clear();
  Uv_.clear();
  Normals_.clear();
  Tangents_.clear();
  Indices_.clear();
  Parts_.clear();
  return false;
}

bool Subject::Build(const Document &document) { return Flatten(document, nullptr); }

bool Subject::Build(const Document &document, Span<const Transform> pose) {
  if (pose.Size() != document.Nodes().size()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(pose.Size()) +
                  " local transforms and the file carries " +
                  std::to_string(document.Nodes().size()) + " nodes");
  }
  return Flatten(document, pose.Data());
}

bool Subject::Flatten(const Document &document, const Transform *pose) {
  /* THE ONE PLACE THE TWO SPELLINGS MEET, so the walk below asks for a placement once however this
   * was entered. The posed overload states the run's length and the unposed one has no run, and
   * neither can be reached with a run that half covers the file. */
  const auto placementOf = [&document, pose](int node, Transform &out) {
    return pose ? document.WorldTransform(node, Span<const Transform>(pose, document.Nodes().size()),
                                          out)
                : document.WorldTransform(node, out);
  };
  Error_.clear();
  Positions_.clear();
  Uv_.clear();
  Normals_.clear();
  Tangents_.clear();
  Indices_.clear();
  Parts_.clear();
  Lights_.clear();
  bool anyUv = false;
  bool anyNormal = false;
  bool anyTangent = false;

  const int sceneIndex = document.DefaultScene();
  if (sceneIndex < 0 || (size_t)sceneIndex >= document.Scenes().size()) {
    return Refuse(document.Path() + ": no default scene to draw");
  }

  /* Depth-first over the hierarchy from the scene's roots; WorldTransform already refuses a cycle
   * and a node index the file does not carry, so this walk needs no visited set of its own. */
  std::vector<int> pending(document.Scenes()[(size_t)sceneIndex].Roots.rbegin(),
                           document.Scenes()[(size_t)sceneIndex].Roots.rend());
  std::vector<double> elements;
  std::vector<uint32_t> run, indices;
  size_t primitives = 0;
  while (!pending.empty()) {
    const int nodeIndex = pending.back();
    pending.pop_back();
    if (nodeIndex < 0 || (size_t)nodeIndex >= document.Nodes().size()) {
      return Refuse(document.Path() + ": scene names node " + std::to_string(nodeIndex) +
                    ", which the file does not carry");
    }
    const Node &node = document.Nodes()[(size_t)nodeIndex];
    for (auto child = node.Children.rbegin(); child != node.Children.rend(); ++child) {
      pending.push_back(*child);
    }
    if (node.Light >= 0) {
      Transform placement;
      if (!placementOf(nodeIndex, placement)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and has no world transform: " + document.Error());
      }
      const LightRef &declared = document.Lights()[(size_t)node.Light];
      PlacedLight placed;
      placed.NodeName = node.Name;
      placed.LightName = declared.Name;
      placed.Light = declared.Light;
      const double origin[3] = {0, 0, 0};
      double position[3];
      placement.Point(origin, position);
      /* THE BEAM IS THE NODE'S -Z, which is `KHR_lights_punctual`'s own rule and the same convention
       * the format gives a camera. A zero-scaled node has no direction to give and is refused rather
       * than pointed somewhere. */
      const double axis[3] = {0, 0, -1};
      double beam[3];
      placement.Direction(axis, beam);
      if (!Normalise(beam)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and its transform collapses the beam to zero length");
      }
      for (int component = 0; component < 3; ++component) {
        placed.Light.Position[component] = (float)position[component];
        placed.Light.Direction[component] = (float)beam[component];
      }
      Lights_.push_back(std::move(placed));
    }
    if (node.Mesh < 0) { continue; }
    if ((size_t)node.Mesh >= document.Meshes().size()) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) + " names mesh " +
                    std::to_string(node.Mesh) + ", which the file does not carry");
    }
    Transform world;
    if (!placementOf(nodeIndex, world)) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                    " has no world transform: " + document.Error());
    }
    for (const Primitive &primitive : document.Meshes()[(size_t)node.Mesh].Primitives) {
      ++primitives;
      Part part;
      part.NodeName = node.Name;
      part.Material = primitive.Material;
      part.FirstVertex = VertexCount();
      part.FirstIndex = Indices_.size();
      if (!DrawsASurface(primitive.Mode)) {
        return Refuse(document.Path() + ": primitive of mesh " + std::to_string(node.Mesh) +
                      " is " + ModeName(primitive.Mode) +
                      ", and this subject draws surfaces only -- TRIANGLES, TRIANGLE_STRIP or "
                      "TRIANGLE_FAN");
      }
      const int position = primitive.Find("POSITION");
      if (position < 0) {
        return Refuse(document.Path() + ": primitive of mesh " + std::to_string(node.Mesh) +
                      " carries no POSITION, and nothing here invents one");
      }
      if (!document.ReadElements(position, elements)) {
        return Refuse(document.Path() + ": POSITION does not decode: " + document.Error());
      }
      if (elements.size() % 3 != 0) {
        return Refuse(document.Path() + ": POSITION decodes to " + std::to_string(elements.size()) +
                      " components, which is not a whole number of points");
      }
      const uint32_t base = (uint32_t)(Positions_.size() / 3);
      const size_t vertices = elements.size() / 3;
      for (size_t vertex = 0; vertex < vertices; ++vertex) {
        double local[3] = {elements[vertex * 3], elements[vertex * 3 + 1],
                           elements[vertex * 3 + 2]};
        double global[3];
        world.Point(local, global);
        for (int axis = 0; axis < 3; ++axis) { Positions_.push_back(global[axis]); }
      }

      /* THE FIRST UV SET, PER PRIMITIVE. The run stays as long as the vertex run whatever the mix
       * is: a primitive that carried none contributes zeros there and is drawn by a pipeline with
       * no uv slot, so those zeros are unread rather than sampled at the image's corner. */
      const int uv = primitive.Find("TEXCOORD_0");
      part.HasUv = uv >= 0;
      anyUv = anyUv || part.HasUv;
      Uv_.resize((Positions_.size() / 3) * 2, 0.0);
      if (uv >= 0) {
        std::vector<double> coordinates;
        if (!document.ReadElements(uv, coordinates)) {
          return Refuse(document.Path() + ": TEXCOORD_0 does not decode: " + document.Error());
        }
        if (coordinates.size() != vertices * 2) {
          return Refuse(document.Path() + ": TEXCOORD_0 decodes to " +
                        std::to_string(coordinates.size() / 2) + " pairs over " +
                        std::to_string(vertices) + " vertices");
        }
        std::copy(coordinates.begin(), coordinates.end(),
                  Uv_.begin() + static_cast<std::ptrdiff_t>(part.FirstVertex * 2));
      }

      /* THE NORMAL, PER PRIMITIVE, ROTATED BY THE INVERSE TRANSPOSE and normalised here so that no
       * consumer has to know whether the node scaled it. The run stays as long as the vertex run
       * whatever the mix is; a primitive that carried none contributes zeros and is drawn by a
       * pipeline with no normal slot, so those zeros are unread rather than shaded as a direction. */
      const int normal = primitive.Find("NORMAL");
      part.HasNormal = normal >= 0;
      anyNormal = anyNormal || part.HasNormal;
      Normals_.resize(Positions_.size(), 0.0);
      if (normal >= 0) {
        std::vector<double> directions;
        if (!document.ReadElements(normal, directions)) {
          return Refuse(document.Path() + ": NORMAL does not decode: " + document.Error());
        }
        if (directions.size() != vertices * 3) {
          return Refuse(document.Path() + ": NORMAL decodes to " +
                        std::to_string(directions.size() / 3) + " vectors over " +
                        std::to_string(vertices) + " vertices");
        }
        for (size_t vertex = 0; vertex < vertices; ++vertex) {
          double local[3] = {directions[vertex * 3], directions[vertex * 3 + 1],
                             directions[vertex * 3 + 2]};
          double global[3];
          if (!world.Normal(local, global)) {
            return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                          " carries a NORMAL and a transform with no inverse, so the surface it is "
                          "perpendicular to has collapsed");
          }
          /* A ZERO-LENGTH NORMAL IS THE FILE'S AND IS CARRIED AS IT ARRIVED. Substituting one here
           * would make a malformed vertex look shaded; the consumer sees a zero and the picture
           * shows it. */
          (void)Normalise(global);
          for (int axis = 0; axis < 3; ++axis) {
            Normals_[(part.FirstVertex + vertex) * 3 + (size_t)axis] = global[axis];
          }
        }
      }

      if (primitive.Indices >= 0) {
        if (!document.ReadIndices(primitive.Indices, run)) {
          return Refuse(document.Path() + ": the index accessor does not decode: " +
                        document.Error());
        }
      } else {
        run.resize(vertices);
        for (size_t vertex = 0; vertex < vertices; ++vertex) { run[vertex] = (uint32_t)vertex; }
      }
      if (!RunIsWhole(primitive.Mode, run.size())) {
        return Refuse(document.Path() + ": " + std::to_string(run.size()) +
                      " indices do not make a whole run of " + ModeName(primitive.Mode));
      }
      Triangulate(primitive.Mode,
                  world.LinearDeterminant() < 0 ? Handedness::Reversed : Handedness::Preserved, run,
                  indices);
      for (uint32_t index : indices) {
        if (index >= vertices) {
          return Refuse(document.Path() + ": index " + std::to_string(index) + " addresses past the " +
                        std::to_string(vertices) + " vertices of its own primitive");
        }
        Indices_.push_back(base + index);
      }
      part.IndexCount = Indices_.size() - part.FirstIndex;
      if (!BuildTangentsFor(document, primitive, world, part, vertices)) { return false; }
      anyTangent = anyTangent || part.HasTangent();
      part.VertexCount = VertexCount() - part.FirstVertex;
      /* A primitive that yielded no triangle is not a part: it is a name a per-part declaration
       * would have to answer for while nothing of it is drawn. */
      if (part.IndexCount > 0) { Parts_.push_back(part); }
    }
  }

  if (!anyUv) { Uv_.clear(); }
  if (!anyNormal) { Normals_.clear(); }
  if (!anyTangent) { Tangents_.clear(); }
  if (Indices_.empty()) {
    return Refuse(document.Path() + ": the default scene draws no triangle over " +
                  std::to_string(primitives) + " primitive(s), so there is nothing to render");
  }

  Bound();
  return true;
}

void Subject::Bound() {
  for (int axis = 0; axis < 3; ++axis) {
    Min_[axis] = Max_[axis] = Positions_[(size_t)axis];
  }
  for (size_t vertex = 1; vertex < VertexCount(); ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const double value = Positions_[vertex * 3 + (size_t)axis];
      if (value < Min_[axis]) { Min_[axis] = value; }
      if (value > Max_[axis]) { Max_[axis] = value; }
    }
  }
}

bool Subject::Assemble(const Assembly &what) {
  Error_.clear();
  Positions_.clear();
  Uv_.clear();
  Normals_.clear();
  Tangents_.clear();
  Indices_.clear();
  Parts_.clear();
  Lights_.clear();
  if (what.Pieces.Empty()) {
    return Refuse("an assembly of no piece draws nothing, and a subject with no triangle is not one");
  }

  bool anyUv = false;
  bool anyNormal = false;
  bool anyTangent = false;
  for (size_t index = 0; index < what.Pieces.Size(); ++index) {
    const Piece &piece = what.Pieces[index];
    const std::string where = "assembled piece " + std::to_string(index);
    if (piece.PositionsM.Empty() || (piece.PositionsM.Size() % 3) != 0) {
      return Refuse(where + " states " + std::to_string(piece.PositionsM.Size()) +
                    " position components, which is not a whole run of points");
    }
    const size_t vertices = piece.PositionsM.Size() / 3;
    if (piece.Indices.Empty() || (piece.Indices.Size() % 3) != 0) {
      return Refuse(where + " states " + std::to_string(piece.Indices.Size()) +
                    " indices, which is not a whole run of triangles");
    }
    if (piece.Material < -1) {
      return Refuse(where + " names material " + std::to_string(piece.Material) +
                    ", and -1 is the only spelling of naming none");
    }
    std::string why;
    if (!RunIsStatable(piece.PositionsM, vertices, 3, "positions", where, why) ||
        !RunIsStatable(piece.Normals, vertices, 3, "normals", where, why) ||
        !RunIsStatable(piece.Uv, vertices, 2, "uv pairs", where, why) ||
        !RunIsStatable(piece.Tangents, vertices, 4, "tangents", where, why)) {
      return Refuse(why);
    }

    Part part;
    part.NodeName = piece.NodeName;
    part.Material = piece.Material;
    part.FirstVertex = VertexCount();
    part.FirstIndex = Indices_.size();
    part.VertexCount = vertices;
    part.IndexCount = piece.Indices.Size();
    part.HasUv = !piece.Uv.Empty();
    part.HasNormal = !piece.Normals.Empty();
    /* A PRODUCER'S BASIS IS A SUPPLIED ONE. `Generated` is what the reader records when it ran
     * MikkTSpace over a file that stated none, and a generator claiming that word would be saying
     * its basis came from an algorithm this subject can re-run. */
    part.Tangent = piece.Tangents.Empty() ? TangentSource::None : TangentSource::Supplied;
    anyUv = anyUv || part.HasUv;
    anyNormal = anyNormal || part.HasNormal;
    anyTangent = anyTangent || part.HasTangent();

    for (const float component : piece.PositionsM) { Positions_.push_back((double)component); }
    /* The three optional runs stay as long as the vertex run whatever the mix is, exactly as the
     * flatten leaves them: a piece that carried none contributes zeros that `HasUv`/`HasNormal`/
     * `Tangent` say are unread. */
    Uv_.resize((Positions_.size() / 3) * 2, 0.0);
    Normals_.resize(Positions_.size(), 0.0);
    Tangents_.resize((Positions_.size() / 3) * 4, 0.0);
    for (size_t at = 0; at < piece.Uv.Size(); ++at) {
      Uv_[part.FirstVertex * 2 + at] = (double)piece.Uv[at];
    }
    for (size_t at = 0; at < piece.Normals.Size(); ++at) {
      Normals_[part.FirstVertex * 3 + at] = (double)piece.Normals[at];
    }
    for (size_t at = 0; at < piece.Tangents.Size(); ++at) {
      Tangents_[part.FirstVertex * 4 + at] = (double)piece.Tangents[at];
    }

    for (const uint32_t local : piece.Indices) {
      if (local >= vertices) {
        return Refuse(where + " addresses vertex " + std::to_string(local) + " of its own " +
                      std::to_string(vertices));
      }
      Indices_.push_back((uint32_t)part.FirstVertex + local);
    }
    Parts_.push_back(part);
  }

  if (!anyUv) { Uv_.clear(); }
  if (!anyNormal) { Normals_.clear(); }
  if (!anyTangent) { Tangents_.clear(); }
  Bound();
  return true;
}

double Subject::RadiusM() const {
  const double span[3] = {Max_[0] - Min_[0], Max_[1] - Min_[1], Max_[2] - Min_[2]};
  return 0.5 * Length(span);
}

void Subject::CentreM(double out[3]) const {
  for (int axis = 0; axis < 3; ++axis) { out[axis] = 0.5 * (Min_[axis] + Max_[axis]); }
}

bool Subject::Frame(Placement &out) const {
  const double radius = RadiusM();
  if (!(radius > 0)) { return false; }
  double centre[3];
  CentreM(centre);

  const double azimuth = kFramingAzimuthDeg * kPi / 180.0;
  const double elevation = kFramingElevationDeg * kPi / 180.0;
  /* Azimuth is measured in glTF's ground plane from +X towards +Z, elevation up from it. */
  double toEye[3] = {std::cos(elevation) * std::cos(azimuth), std::sin(elevation),
                     std::cos(elevation) * std::sin(azimuth)};

  const double yfov = 2.0 * std::atan(kFramingSensorHalfHeightMm / kFramingFocalLengthMm);
  const double distance = radius / std::sin(0.5 * yfov) / kFramingFill;
  double eye[3];
  for (int axis = 0; axis < 3; ++axis) { eye[axis] = centre[axis] + toEye[axis] * distance; }

  if (!Placement::LookAt(eye, centre, 0.0, out)) { return false; }
  out.YfovRad = yfov;
  const double floor = radius * kFramingNearFloorFraction;
  out.ZNearM = (distance - radius > floor) ? distance - radius : floor;
  out.ZFarM = distance + radius;
  return true;
}

bool DeclaredPlacement(const Document &document, int cameraIndex, Placement &out,
                       std::string &error) {
  if (cameraIndex < 0 || (size_t)cameraIndex >= document.Cameras().size()) {
    error = document.Path() + ": camera " + std::to_string(cameraIndex) + " is asked for and the " +
            "document declares " + std::to_string(document.Cameras().size());
    return false;
  }
  size_t holder = 0;
  size_t holders = 0;
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    if (document.Nodes()[node].Camera != cameraIndex) { continue; }
    holder = node;
    ++holders;
  }
  if (holders != 1) {
    error = document.Path() + ": camera " + std::to_string(cameraIndex) + " is referenced by " +
            std::to_string(holders) + " nodes, and a placement is what exactly one node states";
    return false;
  }

  const Camera &lens = document.Cameras()[(size_t)cameraIndex];
  Transform world;
  if (!document.WorldTransform((int)holder, world)) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its world transform does not resolve";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) {
    out.Right[axis] = world.M[axis];
    out.Up[axis] = world.M[4 + axis];
    out.Forward[axis] = -world.M[8 + axis];
    out.EyeM[axis] = world.M[12 + axis];
  }
  if (!Normalise(out.Right) || !Normalise(out.Up) || !Normalise(out.Forward)) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its basis has collapsed";
    return false;
  }
  out.Kind = lens.Kind;
  out.YfovRad = lens.YfovRad;
  out.XMagM = lens.XMagM;
  out.YMagM = lens.YMagM;
  out.ZNearM = lens.ZNearM;
  out.ZFarM = lens.ZFarM;
  return true;
}

double Subject::ProjectedAreaPx(const Transform &clip, const Viewport &viewport) const {
  double total = 0;
  for (size_t triangle = 0; triangle * 3 + 2 < Indices_.size(); ++triangle) {
    double raster[3][2];
    for (int corner = 0; corner < 3; ++corner) {
      const size_t vertex = Indices_[triangle * 3 + (size_t)corner];
      const double point[3] = {Positions_[vertex * 3], Positions_[vertex * 3 + 1],
                               Positions_[vertex * 3 + 2]};
      double ndc[3];
      clip.Point(point, ndc);
      viewport.Raster(ndc, raster[corner]);
    }
    total += 0.5 * std::fabs((raster[1][0] - raster[0][0]) * (raster[2][1] - raster[0][1]) -
                             (raster[2][0] - raster[0][0]) * (raster[1][1] - raster[0][1]));
  }
  return total;
}

} // namespace outshine::Gltf
