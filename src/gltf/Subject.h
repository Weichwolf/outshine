/* WHAT A RENDER CASE ACTUALLY DRAWS: one glTF document's triangles, flattened out of the node
 * hierarchy into one run of world-space positions, plus the eye that looks at them.
 *
 * WHAT THE FILE CARRIES AND NOTHING MORE: positions, and where the primitive states them the first
 * uv set, the normal, the tangent and the vertex colour. Nothing below DERIVES an attribute -- a
 * primitive with no NORMAL keeps none and says so per part, because deriving one silently is what
 * makes a subject not the declared one (board:0073).
 *
 * THE TANGENT IS THE ONE EXCEPTION AND THE FORMAT IS WHAT MAKES IT ONE. "When tangents are not
 * specified, client implementations SHOULD calculate tangents using default MikkTSpace algorithms"
 * (Specification.adoc:1426) -- so a generated basis is not this file's opinion about the subject, it
 * is the answer the format states, and `Tangents.h` is the algorithm it names. IT IS GENERATED ONLY
 * WHERE IT WOULD BE READ: the primitive's own material must declare a `normalTexture`, and the
 * primitive must carry both NORMAL and TEXCOORD_0, which is the same sentence the specification
 * writes. A SUPPLIED `TANGENT` IS NEVER REGENERATED, because `NormalTangentMirrorTest` exists to
 * fail an engine that does, and `Part::TangentSource` is where a consumer reads which of the two it
 * got.
 *
 * A GENERATED BASIS IS PER TRIANGLE CORNER AND THIS RUN IS PER VERTEX, so a vertex whose corners
 * came out with different bases is SPLIT rather than averaged -- averaging is exactly the smoothing
 * decision MikkTSpace exists to remove, and the split is decided by an exact inequality so that no
 * tolerance stands between two bases that are not the same one.
 *
 * AND THE LIGHTS THE SCENE PLACES, because `KHR_lights_punctual` puts a light on a node: what the
 * hierarchy answers is where the light is and which way it points, so a walk that flattened only
 * the geometry would leave the light unplaceable by anyone downstream.
 *
 * EVERY REFUSAL NAMES WHAT IT REFUSED. A document that yields no triangle, a primitive mode this
 * cannot draw, an attribute that is not there, coincident vertices -- each stops the build with a
 * sentence, because a subject that silently came out empty is the empty picture the render suite's
 * whole guard exists to catch. */
#ifndef GLTF_SUBJECT_H
#define GLTF_SUBJECT_H

#include <cstdint>
#include <string>
#include <vector>

#include "PunctualLight.h"
#include "Span.h"

#include "Camera.h"
#include "Transform.h"
#include "Variant.h"

namespace outshine::Gltf {

class Document;
struct Primitive;

/* WHERE THE EYE IS AND WHAT IT SEES, in glTF's own right-handed +Y-up metres. The basis is the
 * camera's: it looks down -Forward's opposite, i.e. `Forward` is the direction of view, and `Right`
 * and `Up` complete a right-handed frame with it. A renderer maps the three into its own axes; the
 * meaning stays here. */
struct Placement {
  double EyeM[3] = {0, 0, 0};
  double Forward[3] = {0, 0, -1};
  double Right[3] = {1, 0, 0};
  double Up[3] = {0, 1, 0};
  /* Which of the two sets below carries the lens: `YfovRad` under Perspective, the two
   * magnifications under Orthographic. A parallel projection is a different matrix and not a large
   * number substituted for one, so the kind travels with the placement rather than being inferred
   * from a zero. */
  CameraKind Kind = CameraKind::Perspective;
  double YfovRad = 0;
  double XMagM = 0;
  double YMagM = 0;
  double ZNearM = 0;
  double ZFarM = 0;

  /* A CAMERA STATED AS A POSITION, AN AIM AND A ROLL -- Blender's way of placing one, and what a
   * manifest declares where the subject's file carries no camera. POSITIVE ROLL TURNS THE CAMERA'S
   * RIGHT VECTOR TOWARDS ITS UP VECTOR, so the image of the world rotates by the opposite sign; that
   * convention is pinned by the oracle's own camera matrix and is stated in exactly this one place.
   * Refuses an aim at the eye and an aim straight along +Y, where "up" cannot be resolved. */
  [[nodiscard]] static bool LookAt(const double eyeM[3], const double aimM[3], double rollRad,
                                   Placement &out);

  /* World -> camera. */
  [[nodiscard]] bool View(Transform &out) const;
  /* World -> clip, at the viewport's aspect. */
  [[nodiscard]] bool Clip(double viewportAspect, Transform &out) const;
};

/* THE PLACEMENT OF ONE NAMED CAMERA OF THE DOCUMENT -- `cameraIndex` into `Document::Cameras()`,
 * never "the first one found". `Cameras` carries two, at the same point and differing only in
 * projection, so a reader that took the first would render one of them and report the other's
 * criterion; the index is therefore a parameter with no default, and a caller that does not know
 * which camera it wants cannot spell the call.
 *
 * EXACTLY ONE NODE MUST REFERENCE IT. A camera instanced twice has two placements and no way to
 * choose between them, and none is invented here: the sentence in `error` names the count. */
[[nodiscard]] bool DeclaredPlacement(const Document &document, int cameraIndex, Placement &out,
                                     std::string &error);

/* ONE DRAWN PRIMITIVE'S CONTRIBUTION to the flattened run, and the primitive is the unit because the
 * primitive is what carries a material: `SciFiHelmet` puts several under one mesh and
 * `AlphaBlendModeTest` gives each its own alpha mode, so a part that was a NODE could only ever
 * name one of them. The hierarchy is gone by the time a consumer sees the triangles, and this is
 * what survives it: which vertices and which indices came from where, what the node was called, and
 * which material the primitive named.
 *
 * A DECLARATION THAT SAYS SOMETHING PER PART RESOLVES AGAINST THIS AND AGAINST NOTHING ELSE, so
 * "the third cube" is a name in the file rather than a position in a list somebody has to keep in
 * step. */
/* WHERE A PART'S TANGENT BASIS CAME FROM, and it is three answers rather than a flag beside a flag
 * (`Enum.2`): a consumer that must not regenerate a supplied basis has to be able to see which it
 * has, and `NormalTangentMirrorTest` is the asset that fails an engine which cannot. */
enum class TangentSource { None, Supplied, Generated };

/* WHERE ONE VERTEX GOES, and it is one question the format answers two ways. An unskinned primitive
 * rides its node's transform, every vertex the same. A SKINNED ONE IGNORES THAT TRANSFORM ENTIRELY --
 * glTF states the skinned mesh node's own transform MUST be ignored -- and rides a blend of its
 * joints instead. `At` is the only place that choice is spelled, so no later reader can apply the
 * node transform to a skinned vertex by reaching for the obvious variable. */
struct VertexPlacement {
  const Transform &Node;
  const Transform *Skinned = nullptr; /* one per vertex, or null where the node carries no skin */

  const Transform &At(size_t vertex) const { return Skinned ? Skinned[vertex] : Node; }
};

struct Part {
  std::string NodeName;   /* the file's own; empty where the node carries none */
  /* The document's material index, or -1 where the primitive names none. It is the material the
   * part WEARS and not the one the primitive spells: a declared variant has already been resolved
   * into it (board:1188), which is what leaves the extension with no spelling downstream. */
  int Material = -1;
  bool HasUv = false;     /* whether TEXCOORD_0 was carried, per primitive and never per subject */
  bool HasUv1 = false;    /* whether TEXCOORD_1 was carried, per primitive and never per subject */
  bool HasNormal = false; /* whether NORMAL was carried, per primitive and never per subject */
  bool HasColour = false; /* whether COLOR_0 was carried, per primitive and never per subject */
  TangentSource Tangent = TangentSource::None;
  size_t FirstVertex = 0;
  size_t VertexCount = 0;
  size_t FirstIndex = 0;
  size_t IndexCount = 0;

  [[nodiscard]] bool HasTangent() const { return Tangent != TangentSource::None; }
};

/* A `KHR_lights_punctual` LIGHT WITH ITS NODE ALREADY RESOLVED: the table entry says what kind of
 * light it is and the hierarchy says where it is and which way it points, so this is the pair and
 * the only form a consumer ever sees. `Light.Position` and `Light.Direction` are in the default
 * scene's root coordinates, the same frame `PositionsM()` is in, and the direction is a unit vector.
 *
 * ITS PLACE IS THE NODE'S -Z, which is the extension's own rule and is why a light cannot be placed
 * without walking the hierarchy that carries it. */
struct PlacedLight {
  std::string NodeName;
  std::string LightName;
  outshine::PunctualLight Light;
};

/* ONE PIECE AS A PRODUCER STATES IT, and it is the flattened form on purpose: a producer that had to
 * spell a node, an accessor and a buffer view would be building an interchange tree for the flatten
 * to take apart again. Runs are f32 because the format's POSITION, NORMAL, TEXCOORD_0 and TANGENT
 * are, so a produced subject reaches `Subject(Emit(S)) == S` at zero applications.
 *
 * INDICES ARE LOCAL TO THE PIECE, so a producer that grew two meshes separately does not have to
 * know where the first one ended. An empty attribute run is an attribute this piece does not carry
 * and is never a run of zeros the caller had to invent. */
struct Piece {
  std::string NodeName;
  int Material = -1;
  Span<const float> PositionsM; /* 3 per vertex */
  Span<const float> Normals;    /* 3 per vertex, or empty */
  Span<const float> Uv;         /* 2 per vertex, or empty */
  Span<const float> Uv1;        /* 2 per vertex, or empty; the second set, never a copy of the first */
  Span<const float> Tangents;   /* 4 per vertex, or empty; a supplied basis, never a generated one */
  /* 4 per vertex -- linear RGBA -- or empty. FOUR AND NOT THREE even where the producer means an
   * opaque colour, because the format's VEC3 arm is "assume alpha 1.0" and a producer that stated
   * three would be handing over a run whose width depends on what it meant. Every component is
   * checked against [0, 1] at the handover, on the same terms a file's is. */
  Span<const float> Colours;
  Span<const uint32_t> Indices; /* triangles, three each */
};

/* WHAT A GENERATOR HANDS OVER, as one parameter object rather than a list of runs (`I.23`). */
struct Assembly {
  Span<const Piece> Pieces;
};

class Subject {
public:
  /* Flattens the document's default scene. `false` leaves `Error()` holding the sentence.
   *
   * THE VARIANT IS A DEFAULTED PARAMETER AND NOT A THIRD OVERLOAD (`F.51`, board:1188): it is
   * independent of the pose, so overloads would multiply rather than add. Its default is the
   * extension's own default -- no active variant, every primitive wearing the material it names --
   * so a consumer that has never heard of variants writes the call it always wrote and gets the
   * picture the file states. A name the document does not declare is a refusal, and the flatten is
   * where it lands because the flatten is where the document being drawn is known. */
  [[nodiscard]] bool Build(const Document &document, const VariantSelection &variant = {});
  /* THE SAME FLATTEN WITH THE HIERARCHY POSED (board:1169): one local transform per node, which is
   * what `Gltf::Pose::At` writes at a time. The pose is baked into the world positions here exactly
   * as the file's own placements are, so nothing downstream learns that the subject was animated --
   * a drawable is a drawable. Refuses a run that is not one transform per node. */
  [[nodiscard]] bool Build(const Document &document, Span<const Transform> pose,
                           const VariantSelection &variant = {});
  /* THE POSE WITH ITS MORPH WEIGHTS, which is what `Gltf::Pose::At` writes in one call (board:1203).
   * `weights` is the flat run laid out by `Document::MorphWeightsFirst`, and passing it separately
   * from the transforms is not an invitation to pass one without the other -- the overload above
   * exists for a file with no morph target at all, where the run is empty and the two calls are the
   * same call. */
  [[nodiscard]] bool Build(const Document &document, Span<const Transform> pose,
                           Span<const double> weights, const VariantSelection &variant = {});

  /* THE OTHER WAY IN, AND IT IS THE EDGE A GENERATOR STANDS ON (board:0105): the same
   * drawable, produced rather than read. Nothing is derived here -- no normal, no tangent basis, no
   * winding restatement -- because a producer states what it made; what this does is CHECK, and
   * every refusal names the piece and what was wrong with it, since a generator that handed over a
   * malformed run would otherwise be found out by a black picture. `false` leaves `Error()` holding
   * the sentence. */
  [[nodiscard]] bool Assemble(const Assembly &what);

  const std::string &Error() const { return Error_; }

  /* 3 doubles per vertex, in the default scene's root coordinates, node transforms applied. */
  const std::vector<double> &PositionsM() const { return Positions_; }
  /* 2 doubles per vertex, `TEXCOORD_0` verbatim -- glTF's own convention, origin at the image's
   * UPPER left, which is the same origin our raster uses, so no flip happens here or anywhere.
   * Empty when NO part carried one; otherwise it covers every vertex, and the vertices of a part
   * that carried none hold zero. THAT ZERO IS NEVER SAMPLED: `Part::HasUv` selects a pipeline with
   * no uv slot at all, so the number is unread rather than a stand-in for a coordinate. */
  const std::vector<double> &Uv() const { return Uv_; }
  bool HasUv() const { return !Uv_.empty(); }
  /* 2 doubles per vertex, `TEXCOORD_1` verbatim, on the same terms as the first set (board:1182).
   * It is a SECOND RUN and not a wider first one because a primitive may carry either without the
   * other's length being a function of it, and because the consumer binds them into two vertex
   * slots that a single interleaved run could not be split back into without a copy.
   *
   * IT IS NEVER READ IN PLACE OF THE FIRST. A texture reference that named this set on a subject
   * that carries none is a named refusal (`UvSetOf`), not a read of `Uv()` -- an emissive image on
   * the wrong uv set is still an image on a surface, so the failure would be silent. */
  const std::vector<double> &Uv1() const { return Uv1_; }
  bool HasUv1() const { return !Uv1_.empty(); }
  /* 3 doubles per vertex, `NORMAL` rotated into the same root coordinates the positions are in by
   * the inverse transpose of the node's linear part, and normalised. Empty when NO part carried one;
   * otherwise it covers every vertex and the vertices of a part that carried none hold zero. THAT
   * ZERO IS NEVER SHADED: `Part::HasNormal` selects a pipeline with no normal slot at all, and a
   * consumer that wants to light such a part refuses by name rather than lighting a zero vector.
   *
   * NOTHING HERE DERIVES ONE. glTF says a client MUST compute flat normals for a primitive that
   * carries none; that is a separate operation over the triangles and it is not this one, so its
   * absence is recorded per part rather than repaired invisibly (board:0085). */
  const std::vector<double> &Normals() const { return Normals_; }
  bool HasNormal() const { return !Normals_.empty(); }
  /* 4 doubles per vertex, in the same root coordinates: a unit tangent and glTF's handedness, so
   * that `bitangent = cross(normal, tangent.xyz) * w`. Empty when NO part carries one; otherwise it
   * covers every vertex and the vertices of a part that carries none hold zero, which
   * `Part::Tangent` says is unread rather than a direction.
   *
   * THE HANDEDNESS IS THE NODE'S TOO. A mirroring node reverses a cross product, so a `w` carried
   * across unchanged would put the bitangent on the wrong side of a mirrored body -- the same
   * restatement the winding gets, one attribute along. */
  const std::vector<double> &Tangents() const { return Tangents_; }
  bool HasTangent() const { return !Tangents_.empty(); }
  /* 4 doubles per vertex, `COLOR_0` in LINEAR RGBA -- glTF's "additional linear multiplier to base
   * color" (`Specification.adoc:2088`), so no transfer function is applied here or anywhere
   * (board:1193). A `VEC3` accessor fills the fourth with the 1.0 the format states; a normalized
   * unsigned byte or short arrives divided by its own maximum, which is `Document::ReadElements`'s
   * doing, so all six spellings the format permits land in this one run.
   *
   * IT IS FOUR WIDE WHATEVER THE FILE SAID, because the alpha multiplies base colour's alpha and
   * therefore reaches `alphaMode` -- a three-wide run would make "VEC3 or VEC4" a question every
   * consumer had to re-ask, and a consumer that got it wrong would cut a MASK surface at the wrong
   * coverage.
   *
   * EMPTY WHEN NO PART CARRIED ONE; otherwise it covers every vertex and the vertices of a part that
   * carried none hold zero. THAT ZERO IS NEVER MULTIPLIED IN: `Part::HasColour` selects a layout
   * with no colour slot at all, so a part with no COLOR_0 is drawn through a pipeline whose vertex
   * arm writes the multiplicative identity -- black would be the whole body missing. */
  const std::vector<double> &Colours() const { return Colours_; }
  bool HasColour() const { return !Colours_.empty(); }
  /* Every light the default scene places, in the order the walk met them. Empty where the file
   * declares none, which is every asset outside `KHR_lights_punctual`. */
  const std::vector<PlacedLight> &Lights() const { return Lights_; }
  /* In the order the flattening walked them, and covering every vertex and every index exactly
   * once. Several parts may name one node, one mesh or one material; that is what a multi-material
   * asset is. */
  const std::vector<Part> &Parts() const { return Parts_; }
  /* Triangles, three indices each, wound counter-clockwise about the front face -- glTF's own rule,
   * with a mirroring node's order already restated, so the run is uniform however the file spelt it
   * and a consumer may cull back faces on it. */
  const std::vector<uint32_t> &Indices() const { return Indices_; }
  size_t VertexCount() const { return Positions_.size() / 3; }
  size_t TriangleCount() const { return Indices_.size() / 3; }

  /* The world-space AABB over every drawn primitive. Min and max are exact in IEEE-754 and both
   * commutative and associative, so the box does not move when the loader's order does -- which a
   * vertex mean would (board:0083). */
  const double *MinM() const { return Min_; }
  const double *MaxM() const { return Max_; }
  /* ||max - min|| / 2, the bounding sphere's radius, so the framing does not turn with the view. */
  double RadiusM() const;
  void CentreM(double out[3]) const;

  /* THE FRAMING RULE (Framing.h), applied to this subject's own bounds. Refuses a radius of zero:
   * a fallback camera there manufactures exactly the empty picture the guard exists to catch. */
  [[nodiscard]] bool Frame(Placement &out) const;

  /* The projected area of every triangle, in square pixels, at `clip` and `viewport`. Signed areas
   * are summed by magnitude, so a subject whose triangles overlap in the image is counted twice and
   * the number is an AREA and not a coverage -- which is the whole reason it is named this way. */
  double ProjectedAreaPx(const Transform &clip, const Viewport &viewport) const;

private:
  [[nodiscard]] bool Refuse(const std::string &why);
  /* The one walk both `Build` overloads are: `pose` is null for the file's own placements and
   * otherwise points at one local transform per node. */
  [[nodiscard]] bool Flatten(const Document &document, const Transform *pose,
                             const double *weights, const VariantSelection &variant);
  /* The world-space AABB over the position run, whichever of the two ways in filled it. */
  void Bound();
  /* One part's tangent run, appended in step with the runs above: the file's own where the
   * primitive supplies one, MikkTSpace where the primitive's material declares a normal texture and
   * the file supplies none, and nothing otherwise. Splits a vertex whose corners came out with
   * different generated bases, which is why it runs after the part's indices exist and why it may
   * lengthen every run. */
  /* One joint's contribution: `world(joint) * inverseBind`, the format's own product, with the empty
   * `InverseBind` meaning identity as the format states rather than as a special case here. */
  /* One semantic's blended deltas for one primitive, or an empty run where nothing displaces it. */
  [[nodiscard]] bool MorphDeltasFor(const Document &document, const Primitive &primitive,
                                    const char *semantic, const double *weights, size_t count,
                                    size_t components, size_t vertices, std::vector<double> &out);
  [[nodiscard]] static Transform JointMatrix(const Skin &skin, size_t joint, const Transform &world);
  [[nodiscard]] bool BlendSkinFor(const Document &document, const Skin &skin,
                                  const std::vector<Transform> &joints, const Primitive &primitive,
                                  size_t vertices, std::vector<Transform> &out);
  [[nodiscard]] bool BuildTangentsFor(const Document &document, const Primitive &primitive,
                                      const VertexPlacement &place,
                                       Span<const double> morphWeights, Part &part,
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
  std::vector<PlacedLight> Lights_;
  double Min_[3] = {0, 0, 0}, Max_[3] = {0, 0, 0};
};

} // namespace outshine::Gltf
#endif
