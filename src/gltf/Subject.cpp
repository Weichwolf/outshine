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

/* FOUR VALUES AND NOT AN ARRAY, so that the width of the key is spelled at the call. [MEASURED] the
 * `const double basis[4]` form is a pointer with a bound written on it for decoration: a caller
 * handed it a `double[3]` and it read a fourth word off the stack, and because the key is a memcmp
 * over the bits, the split then fragmented at random -- 26 cases died under the sanitiser. A
 * reference to an array would not bind to the pointer into a flat run below, and casting one there
 * is the same defect in a costume, so the components are named and a missing one does not compile. */
BasisKey KeyOf(double x, double y, double z, double w) {
  const double basis[4] = {x, y, z, w};
  BasisKey key;
  for (size_t at = 0; at < 4; ++at) {
    const double folded = basis[at] == 0.0 ? 0.0 : basis[at];
    std::memcpy(&key.Bits[at], &folded, sizeof key.Bits[at]);
  }
  return key;
}

} // namespace

/* THE JOINT MATRICES OF ONE SKIN, one per joint and in the skin's own order. Each takes a vertex out
 * of the joint's bind pose and into the scene: `world(joint) * inverseBind`, which is the format's
 * own product and the order matters -- the inverse bind acts first. An absent `inverseBindMatrices`
 * is the identity by the format's rule, which is why the empty vector needs no second arm. */
/* ONE SEMANTIC'S BLENDED MORPH DELTAS FOR ONE PRIMITIVE, or an empty run where nothing displaces it.
 *
 * glTF states a morphed attribute is `base + SUM(w_i * delta_i)`, and every delta accessor has
 * already been refused at read time unless its count matches the base's -- so this loop needs no
 * bound of its own and a target that leaves this semantic alone simply contributes nothing.
 *
 * THE WEIGHTS ARE NOT NORMALISED AND MUST NOT BE. Unlike a skin's, a morph weight set has no
 * constraint at all in the format: weights outside [0, 1] and sets summing to anything are legal and
 * are how a file states an exaggeration or an inversion. */
bool Subject::MorphDeltasFor(const Document &document, const Primitive &primitive,
                             const char *semantic, const double *weights, size_t count,
                             size_t components, size_t vertices, std::vector<double> &out) {
  out.clear();
  if (count == 0 || primitive.Targets.empty()) { return true; }
  std::vector<double> delta;
  for (size_t target = 0; target < count && target < primitive.Targets.size(); ++target) {
    const double share = weights[target];
    if (share == 0.0) { continue; }
    const int accessor = primitive.Targets[target].Find(semantic);
    if (accessor < 0) { continue; }
    if (!document.ReadElements(accessor, delta)) {
      return Refuse(document.Path() + ": morph target " + std::to_string(target) + "'s " + semantic +
                    " does not decode: " + document.Error());
    }
    if (delta.size() != vertices * components) {
      return Refuse(document.Path() + ": morph target " + std::to_string(target) + "'s " + semantic +
                    " decodes to " + std::to_string(delta.size()) + " components over " +
                    std::to_string(vertices) + " vertices of " + std::to_string(components));
    }
    if (out.empty()) { out.assign(vertices * components, 0.0); }
    for (size_t at = 0; at < delta.size(); ++at) { out[at] += share * delta[at]; }
  }
  return true;
}

Transform Subject::JointMatrix(const Skin &skin, size_t joint, const Transform &world) {
  if (skin.InverseBind.empty()) { return world; }
  return world * Transform::FromColumnMajor(&skin.InverseBind[joint * 16]);
}

/* ONE BLENDED TRANSFORM PER VERTEX, from JOINTS_0 and WEIGHTS_0.
 *
 * THE MATRICES ARE BLENDED AND THEN APPLIED, not applied and then blended, and the two are the same
 * number: the transform is affine and the blend is linear, so the order is chosen for cost -- four
 * matrix adds per vertex against four point transforms per attribute.
 *
 * THE WEIGHTS ARE USED AS THE FILE DECLARES THEM. glTF says a float weight set SHOULD sum to one; it
 * does not say MUST, so renormalising would repair somebody else's asset inside a comparison whose
 * subject IS that asset -- the same argument COLOR_0's range refusal turns the other way, because
 * there the format says MUST. What IS refused is a vertex whose weights sum to zero, which names no
 * position at all rather than an unusual one. */
bool Subject::BlendSkinFor(const Document &document, const Skin &skin,
                           const std::vector<Transform> &joints, const Primitive &primitive,
                           size_t vertices, std::vector<Transform> &out) {
  const int bones = primitive.Find("JOINTS_0");
  const int weights = primitive.Find("WEIGHTS_0");
  if (bones < 0 || weights < 0) {
    return Refuse(document.Path() + ": a primitive on a skinned node carries " +
                  std::string(bones < 0 ? "no JOINTS_0" : "JOINTS_0") + " and " +
                  std::string(weights < 0 ? "no WEIGHTS_0" : "WEIGHTS_0") +
                  ", and a skin without both binds no vertex to any joint");
  }
  std::vector<double> index;
  std::vector<double> weight;
  if (!document.ReadElements(bones, index)) {
    return Refuse(document.Path() + ": JOINTS_0 does not decode: " + document.Error());
  }
  if (!document.ReadElements(weights, weight)) {
    return Refuse(document.Path() + ": WEIGHTS_0 does not decode: " + document.Error());
  }
  if (index.size() != vertices * 4 || weight.size() != vertices * 4) {
    return Refuse(document.Path() + ": JOINTS_0 decodes to " + std::to_string(index.size() / 4) +
                  " and WEIGHTS_0 to " + std::to_string(weight.size() / 4) + " sets over " +
                  std::to_string(vertices) + " vertices, and glTF states both are VEC4");
  }
  out.assign(vertices, Transform());
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    double blended[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    double sum = 0;
    for (size_t slot = 0; slot < 4; ++slot) {
      const double share = weight[vertex * 4 + slot];
      if (share == 0.0) { continue; }
      const double named = index[vertex * 4 + slot];
      if (!(named >= 0.0) || (size_t)named >= joints.size()) {
        return Refuse(document.Path() + ": JOINTS_0 of vertex " + std::to_string(vertex) +
                      " names joint " + std::to_string((long long)named) + " and the skin declares " +
                      std::to_string(joints.size()));
      }
      sum += share;
      const Transform &matrix = joints[(size_t)named];
      for (int at = 0; at < 16; ++at) { blended[at] += share * matrix.M[at]; }
    }
    if (sum == 0.0) {
      return Refuse(document.Path() + ": WEIGHTS_0 of vertex " + std::to_string(vertex) +
                    " sums to zero, so the vertex is bound to no joint and names no position");
    }
    for (int at = 0; at < 16; ++at) { out[vertex].M[at] = blended[at]; }
  }
  (void)skin;
  return true;
}

bool Subject::BuildTangentsFor(const Document &document, const Primitive &primitive,
                               const VertexPlacement &place, Span<const double> morphWeights,
                               Part &part, size_t vertices) {
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
    /* A MORPH TARGET'S TANGENT DELTA IS VEC3 AND THE BASE TANGENT IS VEC4, which the format states
     * and which is not a quirk: `w` is the bitangent's SIGN, a handedness and not a direction, so
     * there is nothing for a delta to add to it. Blending it would produce a fourth component
     * between -1 and 1 that names no handedness at all. */
    std::vector<double> morphedTangents;
    if (!MorphDeltasFor(document, primitive, "TANGENT", morphWeights.Data(), morphWeights.Size(), 3,
                        vertices, morphedTangents)) {
      return false;
    }
    for (size_t vertex = 0; vertex < vertices && !morphedTangents.empty(); ++vertex) {
      for (size_t axis = 0; axis < 3; ++axis) {
        elements[vertex * 4 + axis] += morphedTangents[vertex * 3 + axis];
      }
    }

    /* A TANGENT TRANSFORMS LIKE A DIRECTION IN THE SURFACE and not like the normal: it lies IN the
     * tangent plane, so the node's linear part carries it and the inverse transpose would tilt it
     * out of the plane on any non-uniform scale. */
    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      const Transform &placed = place.At(vertex);
      const double mirrored = placed.LinearDeterminant() < 0 ? -1.0 : 1.0;
      const double local[3] = {elements[vertex * 4], elements[vertex * 4 + 1],
                               elements[vertex * 4 + 2]};
      double global[3];
      placed.Direction(local, global);
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
    BasisKey key = KeyOf(basis[0], basis[1], basis[2], basis[3]);
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
    /* THE SECOND SET IS SPLIT WITH THE FIRST (board:1182). A vertex duplicated for its tangent
     * basis is the same point of the surface, so every run it appears in has to follow it -- a run
     * left short would leave the copy addressing zero, which is the image's corner. */
    if (!Uv1_.empty()) {
      Uv1_.resize((size_t)made * 2 + 2, 0.0);
      Uv1_[(size_t)made * 2] = Uv1_[vertex * 2];
      Uv1_[(size_t)made * 2 + 1] = Uv1_[vertex * 2 + 1];
    }
    /* AND THE VERTEX COLOUR FOLLOWS IT FOR THE SAME REASON (board:1193): the copy is the same point
     * of the surface, so a run left short would leave it multiplying base colour by zero -- a black
     * wedge along a seam, which reads as shading rather than as a missing run. */
    if (!Colours_.empty()) {
      Colours_.resize((size_t)made * 4 + 4, 0.0);
      for (size_t channel = 0; channel < 4; ++channel) {
        Colours_[(size_t)made * 4 + channel] = Colours_[vertex * 4 + channel];
      }
    }
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

/* **THE FLAT NORMAL THE FORMAT REQUIRES WHERE A PRIMITIVE DECLARES NONE** (board:1471).
 *
 * glTF 2.0, meshes: *When normals are not specified, client implementations MUST calculate flat
 * normals and the provided tangents (if present) MUST be ignored.* It is a MUST and this reader did
 * not meet it -- `Clients::Show` refused a lit scene over such a part instead, deliberately and by
 * name, so that nothing was drawn black in a scene where everything else is lit. **A game engine has
 * to display its assets**, and 16 of the 34 models of the generator's two skinning groups declare no
 * NORMAL at all.
 *
 * **FLAT MEANS ONE NORMAL PER TRIANGLE, WHICH MEANS ONE VERTEX PER CORNER.** A shared vertex can hold
 * exactly one normal, so a primitive that is to be flat-shaded is de-indexed: every corner becomes its
 * own vertex carrying its own face's normal. That is the same split `BuildTangentsFor` performs for a
 * tangent basis, without the dedup -- there is nothing to share.
 *
 * THE TANGENTS ARE DROPPED AND THE FORMAT SAYS SO. A basis defined against a normal the file did not
 * declare is a basis about a different surface. */
bool Subject::FlatNormalsFor(Part &part) {
  if (part.HasNormal || part.IndexCount == 0) { return true; }
  const size_t before = VertexCount();
  Normals_.resize(Positions_.size(), 0.0);

  /* **THE SPLIT IS A FUNCTION OF THE INDEX RUN AND OF NOTHING ELSE** (board:1473). It asks which
   * corner reached a vertex first and never asks whether two faces happen to agree, because a
   * coplanarity test reads POSITIONS -- and this runs after the pose is baked, so on a skinned
   * subject the answer changes with the frame and the vertex COUNT becomes a function of time.
   * [MEASURED] `Animation_Skin_07` and `Animation_Skin_09` posed 2 frames and carried a different
   * number of vertices at each; the harness caught it as *the posed subject carries the same
   * vertices at every frame of the grid*, which is the invariant a static index buffer with a
   * dynamic vertex stream rests on. A bake decides topology once and a pose writes values into it. */
  std::vector<char> owned(before, 0);
  for (size_t triangle = 0; triangle + 2 < part.IndexCount; triangle += 3) {
    uint32_t of[3] = {Indices_[part.FirstIndex + triangle],
                      Indices_[part.FirstIndex + triangle + 1],
                      Indices_[part.FirstIndex + triangle + 2]};
    for (size_t corner = 0; corner < 3; ++corner) {
      const uint32_t vertex = of[corner];
      if (vertex < before && !owned[vertex]) {
        owned[vertex] = 1;
        continue;
      }
      /* A CORNER THAT ARRIVES SECOND TAKES A COPY, and every array a vertex is addressed by grows
       * with it. There is no dedup to look the copy up in: a face normal belongs to one triangle. */
      const uint32_t made = (uint32_t)VertexCount();
      for (size_t axis = 0; axis < 3; ++axis) {
        Positions_.push_back(Positions_[(size_t)vertex * 3 + axis]);
      }
      Normals_.resize((size_t)made * 3 + 3, 0.0);
      if (!Uv_.empty()) {
        Uv_.resize((size_t)made * 2 + 2, 0.0);
        Uv_[(size_t)made * 2] = Uv_[(size_t)vertex * 2];
        Uv_[(size_t)made * 2 + 1] = Uv_[(size_t)vertex * 2 + 1];
      }
      if (!Uv1_.empty()) {
        Uv1_.resize((size_t)made * 2 + 2, 0.0);
        Uv1_[(size_t)made * 2] = Uv1_[(size_t)vertex * 2];
        Uv1_[(size_t)made * 2 + 1] = Uv1_[(size_t)vertex * 2 + 1];
      }
      if (!Colours_.empty()) {
        Colours_.resize((size_t)made * 4 + 4, 0.0);
        for (size_t channel = 0; channel < 4; ++channel) {
          Colours_[(size_t)made * 4 + channel] = Colours_[(size_t)vertex * 4 + channel];
        }
      }
      if (!Tangents_.empty()) { Tangents_.resize((size_t)made * 4 + 4, 0.0); }
      of[corner] = made;
      Indices_[part.FirstIndex + triangle + corner] = made;
    }

    double edge[2][3];
    for (size_t axis = 0; axis < 3; ++axis) {
      edge[0][axis] = Positions_[(size_t)of[1] * 3 + axis] - Positions_[(size_t)of[0] * 3 + axis];
      edge[1][axis] = Positions_[(size_t)of[2] * 3 + axis] - Positions_[(size_t)of[0] * 3 + axis];
    }
    double face[3] = {edge[0][1] * edge[1][2] - edge[0][2] * edge[1][1],
                      edge[0][2] * edge[1][0] - edge[0][0] * edge[1][2],
                      edge[0][0] * edge[1][1] - edge[0][1] * edge[1][0]};
    const double length = std::sqrt(face[0] * face[0] + face[1] * face[1] + face[2] * face[2]);
    /* A DEGENERATE TRIANGLE HAS NO NORMAL AND IS GIVEN THE ZERO ONE RATHER THAN A GUESS: it covers
     * no pixel, so nothing samples it, and inventing a direction would make it shade something. */
    if (length > 0.0) {
      for (size_t axis = 0; axis < 3; ++axis) { face[axis] /= length; }
    } else {
      face[0] = face[1] = face[2] = 0.0;
    }
    for (size_t corner = 0; corner < 3; ++corner) {
      for (size_t axis = 0; axis < 3; ++axis) { Normals_[(size_t)of[corner] * 3 + axis] = face[axis]; }
    }
  }
  part.HasNormal = true;
  part.VertexCount += VertexCount() - before;
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
  Uv1_.clear();
  Normals_.clear();
  Tangents_.clear();
  Colours_.clear();
  Indices_.clear();
  Parts_.clear();
  return false;
}

bool Subject::Build(const Document &document, const VariantSelection &variant) {
  return Flatten(document, nullptr, nullptr, variant);
}

bool Subject::Build(const Document &document, Span<const Transform> pose,
                    Span<const double> weights, const VariantSelection &variant) {
  if (pose.Size() != document.Nodes().size()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(pose.Size()) +
                  " local transforms and the file carries " +
                  std::to_string(document.Nodes().size()) + " nodes");
  }
  /* AN EMPTY WEIGHT RUN IS "THE FILE'S OWN", not "no morph", which is why it is not refused against
   * a document that declares targets: a caller posing a file with no animation on its weights has
   * nothing to say about them and the mesh's own values stand. A run of the WRONG length is a
   * different statement and is refused. */
  if (weights.Size() != 0 && weights.Size() != document.MorphWeightsTotal()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(weights.Size()) +
                  " morph weights and the file's nodes carry " +
                  std::to_string(document.MorphWeightsTotal()));
  }
  return Flatten(document, pose.Data(), weights.Size() ? weights.Data() : nullptr, variant);
}

/* EVERY WORLD TRANSFORM ONE MESH NODE DRAWS AT (board:1416). Without `EXT_mesh_gpu_instancing` that is
 * the node's own and nothing else, which is the one-element vector below; with it, the node's world
 * transform composed with each declared instance, in the extension's own order.
 *
 * A MISSING ATTRIBUTE IS ITS IDENTITY AND NOT AN ERROR: the three are independently optional, so a
 * file giving translations alone instances at the rest rotation and scale, which is what the format
 * means by leaving one out.
 *
 * THE COUNT IS ALREADY AGREED. The reader refused a node whose attributes have different lengths, so
 * the first non-empty run's length is the instance count and no second opinion is formed here. */
bool InstanceTransforms(const Document &document, const Node &node, const Transform &world,
                        std::vector<Transform> &out) {
  out.clear();
  const int named[3] = {node.InstanceTranslation, node.InstanceRotation, node.InstanceScale};
  bool any = false;
  for (const int accessor : named) { any = any || accessor >= 0; }
  if (!any) {
    out.push_back(world);
    return true;
  }
  std::vector<double> translation, rotation, scale;
  if (node.InstanceTranslation >= 0 &&
      !document.ReadElements(node.InstanceTranslation, translation)) {
    return false;
  }
  if (node.InstanceRotation >= 0 && !document.ReadElements(node.InstanceRotation, rotation)) {
    return false;
  }
  if (node.InstanceScale >= 0 && !document.ReadElements(node.InstanceScale, scale)) { return false; }
  size_t count = 0;
  if (!translation.empty()) { count = translation.size() / 3; }
  else if (!rotation.empty()) { count = rotation.size() / 4; }
  else if (!scale.empty()) { count = scale.size() / 3; }
  out.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    const double t[3] = {translation.empty() ? 0.0 : translation[at * 3 + 0],
                         translation.empty() ? 0.0 : translation[at * 3 + 1],
                         translation.empty() ? 0.0 : translation[at * 3 + 2]};
    const double r[4] = {rotation.empty() ? 0.0 : rotation[at * 4 + 0],
                         rotation.empty() ? 0.0 : rotation[at * 4 + 1],
                         rotation.empty() ? 0.0 : rotation[at * 4 + 2],
                         rotation.empty() ? 1.0 : rotation[at * 4 + 3]};
    const double sc[3] = {scale.empty() ? 1.0 : scale[at * 3 + 0],
                          scale.empty() ? 1.0 : scale[at * 3 + 1],
                          scale.empty() ? 1.0 : scale[at * 3 + 2]};
    out.push_back(world * Transform::FromTrs(t, r, sc));
  }
  return true;
}

bool Subject::Flatten(const Document &document, const Transform *pose, const double *weights,
                      const VariantSelection &variant) {
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
  Uv1_.clear();
  Normals_.clear();
  Tangents_.clear();
  Colours_.clear();
  Indices_.clear();
  Parts_.clear();
  Lights_.clear();
  Undrawn_ = Undrawn();
  bool anyUv = false;
  bool anyUv1 = false;
  bool anyNormal = false;
  bool anyTangent = false;
  bool anyColour = false;

  const int sceneIndex = document.DefaultScene();
  if (sceneIndex < 0 || (size_t)sceneIndex >= document.Scenes().size()) {
    return Refuse(document.Path() + ": no default scene to draw");
  }

  /* THE SELECTION IS SPENT HERE AND GOES NO FURTHER (board:1188): from this line on it is an index
   * into the file's own variant table, and one line below it is a material index like any other. */
  int activeVariant = -1;
  {
    std::string why;
    if (!variant.Against(document, activeVariant, why)) {
      return Refuse(document.Path() + ": the declaration " + why);
    }
  }

  /* Depth-first over the hierarchy from the scene's roots; WorldTransform already refuses a cycle
   * and a node index the file does not carry, so this walk needs no visited set of its own. */
  std::vector<int> pending(document.Scenes()[(size_t)sceneIndex].Roots.rbegin(),
                           document.Scenes()[(size_t)sceneIndex].Roots.rend());
  /* `KHR_node_visibility` IS RESOLVED BY NOT DESCENDING, which is the whole of its implementation
   * here. The extension states *a node is visible if and only if its own visible property is true
   * and all its parents are visible*, so an invisible node makes its entire subtree invisible and
   * the cheapest correct answer is to stop: no part, no light, no children pushed.
   *
   * WHAT IS DELIBERATELY NOT SKIPPED IS A CAMERA, because the extension says so -- *visibility
   * affects neither cameras, nor node's interactivity features*. Nothing here has to arrange that:
   * a camera is resolved by its own scan over `document.Nodes()` further down this file, which never
   * consulted this walk. **If a later round moves camera resolution into this walk, this comment is
   * the reason it may not simply inherit the skip.** */
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
    if (!node.Visible) { continue; }
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
    /* THE WEIGHTS THIS NODE MORPHS BY: the pose's where one was given, and the mesh's own where it
     * was not -- which is the same rule `pose` itself follows one field up, so "the file's own" has
     * one meaning for both halves of a pose. */
    const size_t morphCount = document.MorphWeightsCount(nodeIndex);
    std::vector<double> nodeWeights;
    if (morphCount > 0) {
      const std::vector<double> &declared = document.Meshes()[(size_t)node.Mesh].Weights;
      for (size_t at = 0; at < morphCount; ++at) {
        nodeWeights.push_back(weights ? weights[document.MorphWeightsFirst(nodeIndex) + at]
                                      : (at < declared.size() ? declared[at] : 0.0));
      }
    }

    /* THE SKIN'S JOINT MATRICES, ONCE PER NODE AND NOT PER PRIMITIVE: they are a property of the
     * skin and the pose, and every primitive of the mesh rides the same ones. */
    std::vector<Transform> jointMatrices;
    if (node.Skin >= 0) {
      const Skin &skin = document.Skins()[(size_t)node.Skin];
      jointMatrices.assign(skin.Joints.size(), Transform());
      for (size_t joint = 0; joint < skin.Joints.size(); ++joint) {
        Transform placed;
        if (!placementOf(skin.Joints[joint], placed)) {
          return Refuse(document.Path() + ": joint node " + std::to_string(skin.Joints[joint]) +
                        " has no world transform: " + document.Error());
        }
        jointMatrices[joint] = JointMatrix(skin, joint, placed);
      }
    }
    /* `EXT_mesh_gpu_instancing`: ONE BODY PER INSTANCE, and the extension's own composition --
     * *an object space transform that should be multiplied by the node's world transform*. A node
     * with no instancing has exactly one, which is its world transform, so the loop below is the
     * un-instanced path unchanged and costs it nothing.
     *
     * THE CHILDREN ARE NOT INSTANCED. The extension says instancing applies to a node's MESH and is
     * silent about a subtree; the conservative reading is that a child follows its parent once, and
     * a reading that multiplied a subtree would be inventing a behaviour the format does not define.
     *
     * IT IS AN EXPANSION AND NOT GPU INSTANCING, WHICH IS SAID HERE BECAUSE THE NAMES COLLIDE. What
     * this produces is N parts sharing one mesh's vertices through their own transforms; a draw list
     * that batches them into one call is the compositor's business and the extension's own note says
     * so -- *GPU instancing and other optimizations are possible, and encouraged, even without this
     * extension*. */
    std::vector<Transform> instances;
    if (!InstanceTransforms(document, node, world, instances)) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                    " instances on an accessor this reader cannot decode: " + document.Error());
    }
    for (const Transform &world : instances) {
      for (const Primitive &primitive : document.Meshes()[(size_t)node.Mesh].Primitives) {
        ++primitives;
        Part part;
        part.NodeName = node.Name;
        part.Material = primitive.MaterialUnder(activeVariant);
        part.FirstVertex = VertexCount();
        part.FirstIndex = Indices_.size();
        /* A MODE THIS RASTERISER HAS NO PASS FOR IS SKIPPED AND COUNTED, NEVER FATAL (board:1399).
         * All seven modes are glTF 2.0 and a file is entitled to all of them; refusing the whole
         * subject over one point cloud lost twelve drawable primitives across two of the 148 models --
         * `MeshPrimitiveModes`, whose entire purpose is that TRIANGLE_STRIP and TRIANGLE_FAN
         * triangulate to the same surface, never reached that question. **Degrade on detail; refuse
         * only on existence.** A subject with no surface primitive at all is still a refusal, and that
         * one is below: `Indices_` stays empty and nothing is drawn. */
        if (!DrawsASurface(primitive.Mode)) {
          ++Undrawn_.Primitives;
          const size_t mode = (size_t)primitive.Mode;
          if (mode < 7) { ++Undrawn_.ByMode[mode]; }
          /* `part` is still local here -- it is pushed at the foot of this body -- so the skip drops
           * it by not reaching that line, and nothing already in `Parts_` is touched. */
          continue;
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
        /* THE MORPH IS APPLIED FIRST, BEFORE THE SKIN AND BEFORE THE NODE, which is the order glTF
         * states: the targets displace the mesh in its own space, the skin then binds that displaced
         * vertex to its joints, and only an unskinned node's transform places it. Any other order
         * puts the deltas through a matrix they were never expressed in. */
        std::vector<double> morphedPositions;
        if (!MorphDeltasFor(document, primitive, "POSITION", nodeWeights.data(), morphCount, 3,
                            vertices, morphedPositions)) {
          return false;
        }
        for (size_t at = 0; at < morphedPositions.size(); ++at) { elements[at] += morphedPositions[at]; }
        std::vector<Transform> skinned;
        if (node.Skin >= 0 &&
            !BlendSkinFor(document, document.Skins()[(size_t)node.Skin], jointMatrices, primitive,
                          vertices, skinned)) {
          return false;
        }
        const VertexPlacement place{world, skinned.empty() ? nullptr : skinned.data()};
        for (size_t vertex = 0; vertex < vertices; ++vertex) {
          double local[3] = {elements[vertex * 3], elements[vertex * 3 + 1],
                             elements[vertex * 3 + 2]};
          double global[3];
          place.At(vertex).Point(local, global);
          for (int axis = 0; axis < 3; ++axis) { Positions_.push_back(global[axis]); }
        }

        /* THE TWO UV SETS, PER PRIMITIVE. Each run stays as long as the vertex run whatever the mix
         * is: a primitive that carried none contributes zeros there and is drawn by a pipeline with
         * no slot for it, so those zeros are unread rather than sampled at the image's corner.
         *
         * THE SECOND SET IS READ HERE AND NOT DERIVED FROM THE FIRST (board:1182), which is the whole
         * of what `MultiUVTest` separates: its two accessors sit in two buffer views 192 bytes apart
         * and place the same face 0.25 uv units -- 256 texels of a 1024 image -- from each other. */
        const struct {
          const char *Semantic;
          bool Part::*Carried;
          bool *Any;
          std::vector<double> *Into;
        } sets[kUvSets] = {{"TEXCOORD_0", &Part::HasUv, &anyUv, &Uv_},
                           {"TEXCOORD_1", &Part::HasUv1, &anyUv1, &Uv1_}};
        for (const auto &set : sets) {
          const int uv = primitive.Find(set.Semantic);
          part.*set.Carried = uv >= 0;
          *set.Any = *set.Any || part.*set.Carried;
          set.Into->resize((Positions_.size() / 3) * 2, 0.0);
          if (uv < 0) { continue; }
          std::vector<double> coordinates;
          if (!document.ReadElements(uv, coordinates)) {
            return Refuse(document.Path() + ": " + set.Semantic + " does not decode: " +
                          document.Error());
          }
          if (coordinates.size() != vertices * 2) {
            return Refuse(document.Path() + ": " + set.Semantic + " decodes to " +
                          std::to_string(coordinates.size() / 2) + " pairs over " +
                          std::to_string(vertices) + " vertices");
          }
          std::copy(coordinates.begin(), coordinates.end(),
                    set.Into->begin() + static_cast<std::ptrdiff_t>(part.FirstVertex * 2));
        }

        /* THE VERTEX COLOUR, PER PRIMITIVE, WIDENED TO RGBA AND OTHERWISE UNTOUCHED (board:1193). It
         * is glTF's "additional linear multiplier to base color", so no transfer function is applied
         * to it -- here or at the sampler -- and the widening is the format's own sentence about VEC3
         * rather than a convenience: alpha 1.0 is the multiplicative identity of base colour's alpha.
         *
         * OUT OF RANGE IS A REFUSAL AND NOT A CLAMP (board:1193), and the decision is written here
         * because all three answers are defensible and only one can be tested. The format says every
         * component MUST lie in [0, 1] (`Specification.adoc:1356`), so a file outside it is malformed;
         * CLAMPING would repair somebody else's asset inside a comparison whose subject IS that asset,
         * and TRUSTING would multiply base colour past one and publish a brighter body that reads as
         * authored. The refusal names the vertex, the channel and the value. IT IS SPELLABLE ON TWO OF
         * THE SIX CELLS ONLY: a normalized unsigned byte or short cannot leave [0, 1], so on four of
         * them the range is carried by the type and this arm is unreachable. */
        const int colour = primitive.Find("COLOR_0");
        part.HasColour = colour >= 0;
        anyColour = anyColour || part.HasColour;
        Colours_.resize((Positions_.size() / 3) * 4, 0.0);
        if (colour >= 0) {
          if ((size_t)colour >= document.Accessors().size()) {
            return Refuse(document.Path() + ": COLOR_0 names accessor " + std::to_string(colour) +
                          ", which the file does not carry");
          }
          size_t components = 0;
          std::string why;
          if (!VertexColourComponents(document.Accessors()[(size_t)colour], components, why)) {
            return Refuse(document.Path() + ": COLOR_0 " + why);
          }
          std::vector<double> tints;
          if (!document.ReadElements(colour, tints)) {
            return Refuse(document.Path() + ": COLOR_0 does not decode: " + document.Error());
          }
          if (tints.size() != vertices * components) {
            return Refuse(document.Path() + ": COLOR_0 decodes to " +
                          std::to_string(tints.size() / components) + " colours over " +
                          std::to_string(vertices) + " vertices");
          }
          for (size_t vertex = 0; vertex < vertices; ++vertex) {
            for (size_t channel = 0; channel < 4; ++channel) {
              const double value =
                  channel < components ? tints[vertex * components + channel] : 1.0;
              if (!(value >= 0.0) || !(value <= 1.0)) {
                return Refuse(document.Path() + ": COLOR_0 of vertex " + std::to_string(vertex) +
                              " carries " + std::to_string(value) + " in channel " +
                              std::to_string(channel) +
                              ", and the format requires every component in [0, 1]");
              }
              Colours_[(part.FirstVertex + vertex) * 4 + channel] = value;
            }
          }
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
          std::vector<double> morphedNormals;
          if (!MorphDeltasFor(document, primitive, "NORMAL", nodeWeights.data(), morphCount, 3,
                              vertices, morphedNormals)) {
            return false;
          }
          for (size_t at = 0; at < morphedNormals.size(); ++at) { directions[at] += morphedNormals[at]; }
          for (size_t vertex = 0; vertex < vertices; ++vertex) {
            double local[3] = {directions[vertex * 3], directions[vertex * 3 + 1],
                               directions[vertex * 3 + 2]};
            double global[3];
            /* A COLLAPSED TRANSFORM IS A SURFACE WITH NO AREA, AND THAT IS A PICTURE RATHER THAN A
             * REFUSAL (board:1439). A scale of zero is legal glTF and `InterpolationTest` animates one
             * on purpose -- its three samplers pulse `(1,1,1)` to `(0,0,0)` and back, which is the whole
             * subject of the case -- so a node at that instant has every vertex on one point, every
             * triangle degenerate and nothing to draw. **The engine's rule is degrade on detail and
             * refuse on existence**, and a node scaled to nothing still exists; it is merely
             * infinitely small.
             *
             * THE NORMAL IS ZERO AND IT IS THE ARITHMETIC'S OWN ANSWER, not a substitute for one: the
             * surface the normal was perpendicular to has no orientation left, and the line below
             * already carries that rule for a zero-length normal the FILE declares -- *the consumer
             * sees a zero and the picture shows it*. This is the same statement one step earlier. */
            if (!place.At(vertex).Normal(local, global)) {
              global[0] = global[1] = global[2] = 0.0;
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
        /* THE WINDING RULE IS STATED FOR THE NODE'S TRANSFORM, AND A SKINNED PRIMITIVE IGNORES THAT
         * TRANSFORM -- so for a skin the sign is taken from the vertices themselves, and a primitive
         * whose blended matrices do not agree on it is refused rather than drawn with a guess: one
         * primitive cannot carry two windings, and picking either would flip half its triangles. */
        Handedness handedness = Handedness::Preserved;
        if (skinned.empty()) {
          handedness = world.LinearDeterminant() < 0 ? Handedness::Reversed : Handedness::Preserved;
        } else {
          const bool mirrored = skinned[0].LinearDeterminant() < 0;
          for (size_t vertex = 1; vertex < skinned.size(); ++vertex) {
            if ((skinned[vertex].LinearDeterminant() < 0) != mirrored) {
              return Refuse(document.Path() + ": vertex " + std::to_string(vertex) +
                            " of a skinned primitive blends to a transform whose determinant has the "
                            "opposite sign to vertex 0's, so the primitive would need two windings");
            }
          }
          handedness = mirrored ? Handedness::Reversed : Handedness::Preserved;
        }
        Triangulate(primitive.Mode, handedness, run, indices);
        for (uint32_t index : indices) {
          if (index >= vertices) {
            return Refuse(document.Path() + ": index " + std::to_string(index) + " addresses past the " +
                          std::to_string(vertices) + " vertices of its own primitive");
          }
          Indices_.push_back(base + index);
        }
        part.IndexCount = Indices_.size() - part.FirstIndex;
        /* **BEFORE THE TANGENT BASIS, BECAUSE THE BASIS IS DEFINED AGAINST A NORMAL** (board:1471).
         * A primitive that declares none gets the flat one the format requires here, so what follows
         * sees a part with normals like any other -- and `BuildTangentsFor` refuses a part without
         * them, which is why the order is load-bearing rather than tidy. */
        if (!FlatNormalsFor(part)) { return false; }
        if (!BuildTangentsFor(document, primitive, place,
                              Span<const double>(nodeWeights.data(), morphCount), part, vertices)) {
          return false;
        }
        anyTangent = anyTangent || part.HasTangent();
        anyNormal = anyNormal || part.HasNormal;
        part.VertexCount = VertexCount() - part.FirstVertex;
        /* A primitive that yielded no triangle is not a part: it is a name a per-part declaration
         * would have to answer for while nothing of it is drawn. */
        if (part.IndexCount > 0) { Parts_.push_back(part); }
      }
    }

  }

  if (!anyUv) { Uv_.clear(); }
  if (!anyUv1) { Uv1_.clear(); }
  if (!anyNormal) { Normals_.clear(); }
  if (!anyTangent) { Tangents_.clear(); }
  if (!anyColour) { Colours_.clear(); }
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
  Uv1_.clear();
  Normals_.clear();
  Tangents_.clear();
  Colours_.clear();
  Indices_.clear();
  Parts_.clear();
  Lights_.clear();
  if (what.Pieces.Empty()) {
    return Refuse("an assembly of no piece draws nothing, and a subject with no triangle is not one");
  }

  bool anyUv = false;
  bool anyUv1 = false;
  bool anyNormal = false;
  bool anyTangent = false;
  bool anyColour = false;
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
        !RunIsStatable(piece.Uv1, vertices, 2, "second-set uv pairs", where, why) ||
        !RunIsStatable(piece.Tangents, vertices, 4, "tangents", where, why) ||
        !RunIsStatable(piece.Colours, vertices, 4, "vertex colours", where, why)) {
      return Refuse(why);
    }
    /* THE RANGE IS CHECKED ON A PRODUCED RUN TOO (board:1193), on the same terms a file's is: the
     * format's [0, 1] is a property of the QUANTITY and not of where it came from, and a generator
     * that multiplied base colour past one would be publishing a brighter body nobody declared. */
    for (size_t at = 0; at < piece.Colours.Size(); ++at) {
      if (piece.Colours[at] >= 0.0f && piece.Colours[at] <= 1.0f) { continue; }
      return Refuse(where + " states a vertex colour component of " +
                    std::to_string(piece.Colours[at]) + " at " + std::to_string(at) +
                    ", and the format requires every component in [0, 1]");
    }

    Part part;
    part.NodeName = piece.NodeName;
    part.Material = piece.Material;
    part.FirstVertex = VertexCount();
    part.FirstIndex = Indices_.size();
    part.VertexCount = vertices;
    part.IndexCount = piece.Indices.Size();
    part.HasUv = !piece.Uv.Empty();
    part.HasUv1 = !piece.Uv1.Empty();
    part.HasNormal = !piece.Normals.Empty();
    part.HasColour = !piece.Colours.Empty();
    /* A PRODUCER'S BASIS IS A SUPPLIED ONE. `Generated` is what the reader records when it ran
     * MikkTSpace over a file that stated none, and a generator claiming that word would be saying
     * its basis came from an algorithm this subject can re-run. */
    part.Tangent = piece.Tangents.Empty() ? TangentSource::None : TangentSource::Supplied;
    anyUv = anyUv || part.HasUv;
    anyUv1 = anyUv1 || part.HasUv1;
    anyNormal = anyNormal || part.HasNormal;
    anyTangent = anyTangent || part.HasTangent();
    anyColour = anyColour || part.HasColour;

    for (const float component : piece.PositionsM) { Positions_.push_back((double)component); }
    /* The three optional runs stay as long as the vertex run whatever the mix is, exactly as the
     * flatten leaves them: a piece that carried none contributes zeros that `HasUv`/`HasNormal`/
     * `Tangent` say are unread. */
    Uv_.resize((Positions_.size() / 3) * 2, 0.0);
    Uv1_.resize((Positions_.size() / 3) * 2, 0.0);
    Normals_.resize(Positions_.size(), 0.0);
    Tangents_.resize((Positions_.size() / 3) * 4, 0.0);
    Colours_.resize((Positions_.size() / 3) * 4, 0.0);
    for (size_t at = 0; at < piece.Uv.Size(); ++at) {
      Uv_[part.FirstVertex * 2 + at] = (double)piece.Uv[at];
    }
    for (size_t at = 0; at < piece.Uv1.Size(); ++at) {
      Uv1_[part.FirstVertex * 2 + at] = (double)piece.Uv1[at];
    }
    for (size_t at = 0; at < piece.Normals.Size(); ++at) {
      Normals_[part.FirstVertex * 3 + at] = (double)piece.Normals[at];
    }
    for (size_t at = 0; at < piece.Tangents.Size(); ++at) {
      Tangents_[part.FirstVertex * 4 + at] = (double)piece.Tangents[at];
    }
    for (size_t at = 0; at < piece.Colours.Size(); ++at) {
      Colours_[part.FirstVertex * 4 + at] = (double)piece.Colours[at];
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
  if (!anyUv1) { Uv1_.clear(); }
  if (!anyNormal) { Normals_.clear(); }
  if (!anyTangent) { Tangents_.clear(); }
  if (!anyColour) { Colours_.clear(); }
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

bool Subject::Frame(Placement &out, double fill) const {
  return FramingFor(Min_, Max_, out, fill);
}

/* THE RULE OVER BOUNDS THAT NEED NOT BE ONE POSE'S (board:1366). An animated subject's stored pose is
 * one frame of its motion, and a camera derived from that pose frames a shape the subject leaves:
 * measured on `AnimatedTriangle`, which spins 360 degrees about the origin, reaching 2.12 from a centre
 * this rule covers to 1.178. The caller that knows the frame grid unions the poses and hands the union
 * here, and `Frame` above is this called with the subject's own box. */
bool FramingFor(const double minM[3], const double maxM[3], Placement &out, double fill) {
  const double span[3] = {maxM[0] - minM[0], maxM[1] - minM[1], maxM[2] - minM[2]};
  const double radius = 0.5 * Length(span);
  if (!(radius > 0)) { return false; }
  double centre[3];
  for (int axis = 0; axis < 3; ++axis) { centre[axis] = 0.5 * (minM[axis] + maxM[axis]); }

  const double azimuth = kFramingAzimuthDeg * kPi / 180.0;
  const double elevation = kFramingElevationDeg * kPi / 180.0;
  /* Azimuth is measured in glTF's ground plane from +X towards +Z, elevation up from it. */
  double toEye[3] = {std::cos(elevation) * std::cos(azimuth), std::sin(elevation),
                     std::cos(elevation) * std::sin(azimuth)};

  const double yfov = 2.0 * std::atan(kFramingSensorHalfHeightMm / kFramingFocalLengthMm);
  const double distance = radius / std::sin(0.5 * yfov) / (fill > 0 ? fill : kFramingFill);
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
