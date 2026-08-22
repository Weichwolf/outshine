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

const std::string kAsset = outshine::Test::PreparedRoot() + "/test-render-khronos-glTF-NegativeScaleTest/scene.gltf";

const char *const kWhiteSphere = "NotShiny1";
const char *const kWhiteSphereMirrored = "NotShinyMinus1";

const char *const kPlate = "PositiveScaleTest";
const char *const kPlateMirrored = "NegativeScaleFront";

constexpr double kDegenerateSine = 1e-9;

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

}

int main() {
  using namespace outshine::Test;

  if (Slurp(kAsset.c_str()).empty()) {
    Unprepared((kAsset + " is not prepared -- run test/harness/shared/corpus/prepare.py").c_str());
    return Report();
  }

  Document document;
  const bool read = document.ReadFile(kAsset.c_str());
  CHECK(read, "Khronos's NegativeScaleTest reads as a .gltf with its buffer beside it");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

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

  Facing whole;
  for (const Part &part : subject.Parts()) {
    const Facing part_facing = FacingOf(subject, part);
    whole.Triangles += part_facing.Triangles;
    whole.Degenerate += part_facing.Degenerate;
    whole.GeometryAgainstAttribute += part_facing.GeometryAgainstAttribute;
  }

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
