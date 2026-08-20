/* WHETHER A SEPARATE NORMAL FLIP IS OWED UNDER A NEGATIVE DETERMINANT, SETTLED BY THE ASSET THAT
 * ASKS THE QUESTION.
 *
 * Khronos's `NegativeScaleTest` states two problems. The first is culling -- *"the renderer has not
 * properly accounted for a negative scale, resulting in back-face culling being inappropriately
 * applied to negatively-scaled meshes"* -- and `test/render/khronos/glTF/NegativeScaleTest/` decides that
 * one against Cycles. The second is normals -- *"the above renderer has not correctly flipped the
 * normal vectors, so the diffuse light is hitting this column from the wrong side ... the two white
 * spheres should be lit the same, regardless of the negative scaling on one of them"* -- and an
 * emitter gathers nothing, so no emission render can decide it. It is decidable here, without a
 * light and without a render, because it is a statement about GEOMETRY: two surfaces are lit alike
 * under any light exactly when their shading normals point the same way out of them.
 *
 * THE OPEN QUESTION THIS CLOSES. `board/` I.26.12 records the flip as unheld and
 * undecided: the reader transforms `NORMAL` by the inverse transpose of the node's linear part AND
 * reverses the index run where the global determinant is negative, so the attribute normal and the
 * re-wound geometric normal may already agree -- in which case a third operation that flipped
 * normals again would BREAK a correct reader rather than repair one. Nothing in the tree measured
 * it. The claims below are what a flip being owed would fail.
 *
 * WHY THIS ASSET AND NOT A FIXTURE. Three of its six negative determinants come from a PARENT's
 * scale, so the child node carries none: a reader that read the node's own TRS instead of its global
 * transform passes half of them and fails half, and a fixture written here would have to be built to
 * remember that. The specification states the rule over the global transform
 * (`Specification.adoc:1734`) and this file is the counterexample to every cheaper reading.
 *
 * THE SUBJECT IS FETCHED, NOT TRACKED (I.26.10), so on a fresh clone it is absent. UNPREPARED, and
 * RED: a skip could not be told from a pass that checked no triangle. */
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include "Document.h"
#include "Subject.h"
#include "Transform.h"

using outshine::Gltf::Document;
using outshine::Gltf::Part;
using outshine::Gltf::Subject;
using outshine::Gltf::Transform;

namespace {

/* THE PREPARED ROOT AND NOT THE TREE (board:1364). A case directory carries its manifest and nothing
 * else; every product is under the system temp root, so a subject is addressed through `PreparedRoot()`
 * and the flattened leaf its path becomes. Spelled once there, because a copy of the mapping would
 * drift the moment one side moved. */


const std::string kAsset = outshine::Test::PreparedRoot() + "/test-render-khronos-glTF-NegativeScaleTest/scene.gltf";

/* The two spheres Khronos's normals paragraph is about: one mesh, two instances, one of them
 * inside-out. Both name the `Not So Shiny` material and both are `doubleSided`. */
const char *const kWhiteSphere = "NotShiny1";
const char *const kWhiteSphereMirrored = "NotShinyMinus1";
/* The two check-and-X plates the culling paragraph is about: single-sided, and one of them under a
 * node whose scale is negative on all three axes. */
const char *const kPlate = "PositiveScaleTest";
const char *const kPlateMirrored = "NegativeScaleFront";

/* A TRIANGLE WHOSE CROSS PRODUCT IS THIS SHORT RELATIVE TO ITS OWN EDGES HAS NO DIRECTION TO CHECK,
 * and an icosphere carries a few. They are counted and published rather than passed over silently:
 * a run where most triangles were degenerate would otherwise report a clean answer over nothing. */
constexpr double kDegenerateSine = 1e-9; /* [SET] dimensionless, |a x b| / (|a| |b|) */

std::string Slurp(const char *path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

double Dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
double Length(const double v[3]) { return std::sqrt(Dot(v, v)); }
void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

const Part *PartNamed(const Subject &subject, const char *nodeName) {
  for (const Part &part : subject.Parts()) {
    if (part.NodeName == nodeName) { return &part; }
  }
  return nullptr;
}

/* THE CENTROID OF A PART'S OWN VERTICES, which is what "outward" is measured from for a closed body.
 * A sphere's vertex mean is its centre to well within the radius, and only the SIGN of a dot product
 * is read off it, so nothing here depends on the last bits of the sum. */
void CentroidOf(const Subject &subject, const Part &part, double out[3]) {
  out[0] = out[1] = out[2] = 0.0;
  if (part.VertexCount == 0) { return; }
  for (size_t vertex = 0; vertex < part.VertexCount; ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      out[axis] += subject.PositionsM()[(part.FirstVertex + vertex) * 3 + size_t(axis)];
    }
  }
  for (int axis = 0; axis < 3; ++axis) { out[axis] /= double(part.VertexCount); }
}

/* THE ONE MEASUREMENT THE FLIP QUESTION TURNS ON, per part: how many of its triangles have a
 * re-wound geometric normal that disagrees with the mean of their own three attribute normals. A
 * reader owing a further flip would report every triangle of a mirrored part here. */
struct Facing {
  size_t Triangles = 0;
  size_t Degenerate = 0;
  size_t GeometryAgainstAttribute = 0;
  size_t GeometryInward = 0;
  size_t AttributeInward = 0;
};

Facing FacingOf(const Subject &subject, const Part &part) {
  Facing facing;
  double centroid[3];
  CentroidOf(subject, part, centroid);
  const std::vector<double> &positions = subject.PositionsM();
  const std::vector<double> &normals = subject.Normals();
  for (size_t at = 0; at + 2 < part.IndexCount; at += 3) {
    ++facing.Triangles;
    const uint32_t corner[3] = {subject.Indices()[part.FirstIndex + at],
                                subject.Indices()[part.FirstIndex + at + 1],
                                subject.Indices()[part.FirstIndex + at + 2]};
    const double *v0 = &positions[corner[0] * 3];
    const double *v1 = &positions[corner[1] * 3];
    const double *v2 = &positions[corner[2] * 3];
    const double edgeA[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
    const double edgeB[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
    double geometric[3];
    Cross(edgeA, edgeB, geometric);
    const double scale = Length(edgeA) * Length(edgeB);
    if (!(scale > 0) || Length(geometric) <= kDegenerateSine * scale) {
      ++facing.Degenerate;
      continue;
    }

    double attribute[3] = {0, 0, 0};
    for (int which = 0; which < 3; ++which) {
      for (int axis = 0; axis < 3; ++axis) {
        attribute[axis] += normals[corner[which] * 3 + size_t(axis)];
      }
    }
    if (Dot(geometric, attribute) <= 0.0) { ++facing.GeometryAgainstAttribute; }

    /* Outwardness, for a closed body: the face's own centre against the part's centroid. */
    const double outward[3] = {(v0[0] + v1[0] + v2[0]) / 3.0 - centroid[0],
                               (v0[1] + v1[1] + v2[1]) / 3.0 - centroid[1],
                               (v0[2] + v1[2] + v2[2]) / 3.0 - centroid[2]};
    if (Dot(geometric, outward) <= 0.0) { ++facing.GeometryInward; }
    if (Dot(attribute, outward) <= 0.0) { ++facing.AttributeInward; }
  }
  return facing;
}

void Publish(const char *what, const Facing &facing) {
  std::printf("NOTE %-22s %zu triangles, %zu degenerate, %zu with geometry against attribute, "
              "%zu geometry inward, %zu attribute inward\n",
              what, facing.Triangles, facing.Degenerate, facing.GeometryAgainstAttribute,
              facing.GeometryInward, facing.AttributeInward);
}

} // namespace

int main() {
  using namespace outshine::Test;

  if (Slurp(kAsset.c_str()).empty()) {
    Unprepared(kAsset.c_str());
    return Report();
  }

  Document document;
  const bool read = document.ReadFile(kAsset.c_str());
  CHECK(read, "Khronos's NegativeScaleTest reads as a .gltf with its buffer beside it");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

  /* THE ASSET CARRIES BOTH SIGNS AND THE HALF THAT MATTERS IS INHERITED. Counted from each node's
   * GLOBAL transform, which is where the specification states the rule; a count taken from the
   * node's own TRS would be a different number, and the difference is the point of the asset. */
  int negativeGlobal = 0, positiveGlobal = 0;
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    Transform world;
    if (!document.WorldTransform(int(node), world)) { continue; }
    (world.LinearDeterminant() < 0 ? negativeGlobal : positiveGlobal)++;
  }
  Note("nodes whose global transform has a negative determinant", double(negativeGlobal), "nodes");
  Note("nodes whose global transform has a positive determinant", double(positiveGlobal), "nodes");
  CHECK(negativeGlobal > 0 && positiveGlobal > 0,
        "the asset carries nodes of both determinant signs, so neither answer below is vacuous");

  Subject subject;
  const bool built = subject.Build(document);
  CHECK(built, "the default scene flattens, node transforms applied and windings restated");
  if (!built) {
    std::printf("       %s\n", subject.Error().c_str());
    return Report();
  }
  CHECK(subject.HasNormal(), "the asset states NORMAL on every primitive, so nothing is derived");

  /* THE WHOLE SUBJECT FIRST. If a flip were owed, the mirrored parts would report every one of their
   * triangles disagreeing here, so this single number is the answer to the open question. */
  Facing whole;
  for (const Part &part : subject.Parts()) {
    const Facing part_facing = FacingOf(subject, part);
    whole.Triangles += part_facing.Triangles;
    whole.Degenerate += part_facing.Degenerate;
    whole.GeometryAgainstAttribute += part_facing.GeometryAgainstAttribute;
  }
  /* NOT `Publish`: outwardness is a property of a CLOSED body, and the subject as a whole is plates
   * and spheres together, so a centroid over all of it names nothing. The two columns that do apply
   * everywhere are the ones printed. */
  std::printf("NOTE whole subject %zu triangles, %zu degenerate, %zu with geometry against "
              "attribute\n",
              whole.Triangles, whole.Degenerate, whole.GeometryAgainstAttribute);
  CHECK(whole.Triangles > 0 && whole.Degenerate * 100 < whole.Triangles,
        "the answer below is taken over triangles that have a direction, and they are the great "
        "majority of the subject");
  CHECK(whole.GeometryAgainstAttribute == 0,
        "every re-wound triangle of the asset already faces the way its own attribute normals do, "
        "so the inverse transpose and the winding reversal agree and NO SECOND FLIP IS OWED -- one "
        "applied here would turn this count from zero into every triangle of a mirrored part");

  /* KHRONOS'S OWN SENTENCE, MADE DECIDABLE: two surfaces are lit the same under any light exactly
   * when their shading normals point the same way out of them. Both spheres are the same mesh, one
   * of them inside-out, so "outward at every triangle in both" IS "lit the same". */
  const Part *sphere = PartNamed(subject, kWhiteSphere);
  const Part *mirroredSphere = PartNamed(subject, kWhiteSphereMirrored);
  CHECK(sphere != nullptr && mirroredSphere != nullptr,
        "the two white spheres survive the flattening under the file's own node names");
  if (sphere && mirroredSphere) {
    const Facing upright = FacingOf(subject, *sphere);
    const Facing mirrored = FacingOf(subject, *mirroredSphere);
    Publish(kWhiteSphere, upright);
    Publish(kWhiteSphereMirrored, mirrored);
    CHECK(upright.Triangles == mirrored.Triangles && upright.Triangles > 0,
          "the two white spheres are the same mesh and flatten to the same triangle count");
    CHECK(upright.AttributeInward == 0 && mirrored.AttributeInward == 0,
          "every shading normal of BOTH white spheres points out of its own sphere, which is "
          "Khronos's `the two white spheres should be lit the same` as a statement about geometry");
    CHECK(upright.GeometryInward == 0 && mirrored.GeometryInward == 0,
          "every re-wound face of BOTH white spheres faces out of its own sphere, so a renderer "
          "culling back faces keeps the near shell of each and not the far shell of one");
  }

  /* THE CULLING HALF, STATED WITHOUT A CAMERA. The two check-and-X plates are single-sided and one
   * of them is inside-out; if the winding follows the determinant, the two plates' front faces point
   * the same way in world space and one view shows both. If it did not, the mirrored plate's front
   * would point backwards and back-face culling would remove exactly the plate Khronos's failure
   * screenshot is missing. */
  const Part *plate = PartNamed(subject, kPlate);
  const Part *mirroredPlate = PartNamed(subject, kPlateMirrored);
  CHECK(plate != nullptr && mirroredPlate != nullptr,
        "the two check-and-X plates survive the flattening under the file's own node names");
  if (plate && mirroredPlate) {
    double facing[2][3] = {{0, 0, 0}, {0, 0, 0}};
    const Part *both[2] = {plate, mirroredPlate};
    for (int which = 0; which < 2; ++which) {
      for (size_t at = 0; at + 2 < both[which]->IndexCount; at += 3) {
        const uint32_t corner[3] = {subject.Indices()[both[which]->FirstIndex + at],
                                    subject.Indices()[both[which]->FirstIndex + at + 1],
                                    subject.Indices()[both[which]->FirstIndex + at + 2]};
        const double *v0 = &subject.PositionsM()[corner[0] * 3];
        const double *v1 = &subject.PositionsM()[corner[1] * 3];
        const double *v2 = &subject.PositionsM()[corner[2] * 3];
        const double edgeA[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
        const double edgeB[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
        double geometric[3];
        Cross(edgeA, edgeB, geometric);
        for (int axis = 0; axis < 3; ++axis) { facing[which][axis] += geometric[axis]; }
      }
      const double magnitude = Length(facing[which]);
      if (magnitude > 0) {
        for (int axis = 0; axis < 3; ++axis) { facing[which][axis] /= magnitude; }
      }
    }
    const double together = Dot(facing[0], facing[1]);
    Note("cosine between the two plates' re-wound front directions", together, "cosine");
    CHECK(together > 0.999,
          "the negatively-scaled plate's re-wound front faces the same way as the plate that was "
          "never mirrored, so one camera sees both and back-face culling removes neither");
  }

  Covers("I.26.12 khronos:NegativeScaleTest, the normals half: the reader's inverse transpose and "
         "its winding reversal under a negative global determinant already agree, so no second "
         "normal flip is owed -- measured over the pinned asset rather than argued");
  Covers("I.26.14 a negative determinant reverses the winding of a triangle-based primitive "
         "(Specification.adoc:1734), stated over the node's GLOBAL transform, which is what the "
         "three inherited negative scales in this asset are the counterexample for");
  return Report();
}
