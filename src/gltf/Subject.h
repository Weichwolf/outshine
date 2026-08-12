/* WHAT A RENDER CASE ACTUALLY DRAWS: one glTF document's triangles, flattened out of the node
 * hierarchy into one run of world-space positions, plus the eye that looks at them.
 *
 * POSITIONS ONLY, AND THAT IS THE SUBJECT'S OWN LIMIT RATHER THAN A CHOICE HERE. Coverage is decided
 * from the oracle's alpha and from our `depth != far`, so the comparison needs no normal at either
 * end (doc/requirements.md I.26); nothing below derives one, and a rung that needs shading needs a
 * mesh type that carries what shading reads.
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

#include "Camera.h"
#include "Transform.h"

namespace outshine::Gltf {

class Document;

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

/* ONE MESH-BEARING NODE'S CONTRIBUTION to the flattened run. The hierarchy is gone by the time a
 * consumer sees the triangles, and this is the one thing about it that survives: which vertices came
 * from which node, and what that node was called. A declaration that says something per node -- what
 * each part of the subject emits -- resolves against this and against nothing else, so "the third
 * cube" is a name in the file rather than a position in a list somebody has to keep in step. */
struct Part {
  std::string NodeName;   /* the file's own; empty where the node carries none */
  size_t FirstVertex = 0;
  size_t VertexCount = 0;
};

class Subject {
public:
  /* Flattens the document's default scene. `false` leaves `Error()` holding the sentence. */
  [[nodiscard]] bool Build(const Document &document);

  const std::string &Error() const { return Error_; }

  /* 3 doubles per vertex, in the default scene's root coordinates, node transforms applied. */
  const std::vector<double> &PositionsM() const { return Positions_; }
  /* 2 doubles per vertex, `TEXCOORD_0` verbatim -- glTF's own convention, origin at the image's
   * UPPER left, which is the same origin our raster uses, so no flip happens here or anywhere.
   * Empty when no primitive of the subject carried one; a subject where SOME primitives carry one
   * is refused rather than half-filled, because a zero uv is a number meaning nothing. */
  const std::vector<double> &Uv() const { return Uv_; }
  bool HasUv() const { return !Uv_.empty(); }
  /* In the order the flattening walked them, and covering every vertex exactly once. */
  const std::vector<Part> &Parts() const { return Parts_; }
  /* The one material every drawn primitive of the subject names, or -1 where none does. A subject
   * whose primitives name two different materials is refused: this draw carries one surface, and
   * silently drawing the second primitive with the first's texture is the defect a multi-material
   * asset exists to expose. */
  int Material() const { return Material_; }
  /* Triangles, three indices each, wound counter-clockwise about the front face -- glTF's own rule,
   * with a mirroring node's order already restated, so the run is uniform however the file spelt it
   * and a consumer may cull back faces on it. */
  const std::vector<uint32_t> &Indices() const { return Indices_; }
  size_t VertexCount() const { return Positions_.size() / 3; }
  size_t TriangleCount() const { return Indices_.size() / 3; }

  /* The world-space AABB over every drawn primitive. Min and max are exact in IEEE-754 and both
   * commutative and associative, so the box does not move when the loader's order does -- which a
   * vertex mean would (doc/requirements.md I.26.10). */
  const double *MinM() const { return Min_; }
  const double *MaxM() const { return Max_; }
  /* ||max - min|| / 2, the bounding sphere's radius, so the framing does not turn with the view. */
  double RadiusM() const;
  void CentreM(double out[3]) const;

  /* THE FRAMING RULE (Framing.h), applied to this subject's own bounds. Refuses a radius of zero:
   * a fallback camera there manufactures exactly the empty picture the guard exists to catch. */
  [[nodiscard]] bool Frame(Placement &out) const;

  /* The camera the document itself declares, if any node references one. A declared camera is used
   * verbatim and no framing rule runs. */
  [[nodiscard]] bool DeclaredPlacement(const Document &document, Placement &out) const;

  /* The projected area of every triangle, in square pixels, at `clip` and `viewport`. Signed areas
   * are summed by magnitude, so a subject whose triangles overlap in the image is counted twice and
   * the number is an AREA and not a coverage -- which is the whole reason it is named this way. */
  double ProjectedAreaPx(const Transform &clip, const Viewport &viewport) const;

private:
  [[nodiscard]] bool Refuse(const std::string &why);

  std::string Error_;
  std::vector<double> Positions_;
  std::vector<double> Uv_;
  std::vector<uint32_t> Indices_;
  std::vector<Part> Parts_;
  int Material_ = -1;
  double Min_[3] = {0, 0, 0}, Max_[3] = {0, 0, 0};
};

} // namespace outshine::Gltf
#endif
