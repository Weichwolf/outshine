/* THE TANGENT GENERATOR AGAINST AN ANSWER THIS TREE DID NOT PRODUCE. `NormalTangentMirrorTest` was
 * exported by the "Khronos Blender glTF 2.0 exporter" and its `TANGENT` is therefore Blender's own
 * MikkTSpace output over this exact mesh. The format says a client SHOULD generate the basis "using
 * default MikkTSpace algorithms" (`Specification.adoc:1426`), so the exporter's answer is the answer
 * -- and running our generator over the same positions, normals and texture coordinates and getting
 * the file's own attribute back is the only check of this unit that does not grade our arithmetic
 * against itself.
 *
 * WHICH NUMBER CARRIES THE CLAIM: the HANDEDNESS, and it carries no tolerance at all. `w` is a sign,
 * a wrong one mirrors the map's x axis, and it is what `NormalTangentTest`'s README calls the
 * classic failure; 15 720 corners must agree exactly. The ANGLE beside it is a distribution and is
 * published rather than derived -- the exporter ran the same construction in binary32 where this one
 * runs it in binary64, and the two differ by whatever the projection step amplified.
 *
 * AND THE OTHER HALF OF THE PAIR, which is a question about WHICH BASIS WAS USED and not about how
 * good the generated one is. `NormalTangentMirrorTest` supplies `TANGENT` and must be taken
 * verbatim; `NormalTangentTest` supplies none and must be generated; a subject whose material
 * declares no normal texture must get neither, because a basis nothing samples is an attribute the
 * file did not state. An engine that always regenerates passes the first asset and fails the second,
 * which is precisely why Khronos ships both, and `Part::TangentSource` is the field that makes the
 * difference readable instead of inferred. */
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include "Document.h"
#include "Subject.h"
#include "Tangents.h"

using outshine::Gltf::Document;
using outshine::Gltf::Subject;
using outshine::Gltf::TangentSource;

namespace {

/* THE PREPARED ROOT AND NOT THE TREE (board:1364). A case directory carries its manifest and nothing
 * else; every product is under the system temp root, so a subject is addressed through `PreparedRoot()`
 * and the flattened leaf its path becomes. Spelled once there, because a copy of the mapping would
 * drift the moment one side moved. */


const std::string kMirror = outshine::Test::PreparedRoot() + "/test-khronos-glTF-NormalTangentMirrorTest/";
const std::string kGenerated = outshine::Test::PreparedRoot() + "/test-khronos-glTF-NormalTangentTest/";
/* A subject with a base-colour image and no normal map, so that "generated only where it would be
 * read" has a case that must come back with nothing. */
const std::string kUnmapped = outshine::Test::PreparedRoot() + "/test-khronos-glTF-SimpleTexture-simple-texture/";

/* [SET] 0.01 degrees, AND IT IS A REGRESSION GUARD RATHER THAN A DERIVED TOLERANCE. The residual it
 * bounds is the difference between one construction run in binary64 here and in binary32 in
 * Blender, and the projection step of that construction subtracts a nearly-parallel component, so
 * the amplification depends on the mesh rather than on the arithmetic and no bound follows from the
 * format widths. One significant figure above the whole observed distribution, whose four
 * percentiles are printed beside it so that a drift is visible long before this number is. */
constexpr double kWorstAngleDeg = 0.01;

bool Read(const std::string &directory, const std::string &entry, Document &file, Subject &out) {
  return file.ReadFile(directory + entry) && out.Build(file);
}

double AngleDeg(const double *first, const double *second) {
  double dot = 0, left = 0, right = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    dot += first[axis] * second[axis];
    left += first[axis] * first[axis];
    right += second[axis] * second[axis];
  }
  if (!(left > 0) || !(right > 0)) { return 180.0; }
  dot /= std::sqrt(left) * std::sqrt(right);
  dot = std::min(1.0, std::max(-1.0, dot));
  return std::acos(dot) * 180.0 / 3.14159265358979323846;
}

double Percentile(const std::vector<double> &sorted, double fraction) {
  if (sorted.empty()) { return 0.0; }
  const size_t at = (size_t)(fraction * (double)(sorted.size() - 1));
  return sorted[at];
}

} // namespace

int main() {
  using namespace outshine::Test;

  Document mirrorFile;
  Subject mirror;
  const bool mirrorRead = Read(kMirror, "NormalTangentMirrorTest.gltf", mirrorFile, mirror);
  CHECK(mirrorRead, "the mirror case's own glTF reads and flattens");
  if (!mirrorRead) { return Report(); }

  CHECK(mirror.Parts().size() == 1 && mirror.Parts()[0].Tangent == TangentSource::Supplied,
        "a primitive that supplies TANGENT has it taken verbatim and is never regenerated");
  CHECK(mirror.HasTangent() && mirror.Tangents().size() == mirror.VertexCount() * 4,
        "the supplied basis covers every vertex, four numbers each");

  /* THE SAME RUNS THE READER FLATTENED, handed straight back to the generator: the node of this
   * subject carries no transform, so the positions and normals in the subject's frame ARE the
   * file's, and nothing about the comparison is a change of basis. */
  outshine::Gltf::TangentSubject over;
  over.PositionsM = mirror.PositionsM().data();
  over.Normals = mirror.Normals().data();
  over.Uv = mirror.Uv().data();
  over.VertexCount = mirror.VertexCount();
  over.Indices = mirror.Indices().data();
  over.IndexCount = mirror.Indices().size();
  std::vector<double> corners;
  std::string error;
  const bool generated = outshine::Gltf::GenerateTangents(over, corners, error);
  CHECK(generated, "MikkTSpace runs over the subject the exporter ran it over");
  if (!generated) {
    Note(error.c_str());
    return Report();
  }
  CHECK(corners.size() == mirror.Indices().size() * 4,
        "the generator answers per triangle corner, four numbers each");

  size_t handednessApart = 0;
  std::vector<double> angles;
  angles.reserve(mirror.Indices().size());
  for (size_t corner = 0; corner < mirror.Indices().size(); ++corner) {
    const size_t vertex = mirror.Indices()[corner];
    const double *ours = &corners[corner * 4];
    const double *theirs = &mirror.Tangents()[vertex * 4];
    if (ours[3] * theirs[3] <= 0) { ++handednessApart; }
    angles.push_back(AngleDeg(ours, theirs));
  }
  std::sort(angles.begin(), angles.end());
  Note("corners compared against the exporter's own TANGENT", (double)angles.size(), "corners");
  Note("angle from the exporter's tangent, p50", Percentile(angles, 0.50), "degrees");
  Note("angle from the exporter's tangent, p95", Percentile(angles, 0.95), "degrees");
  Note("angle from the exporter's tangent, p99", Percentile(angles, 0.99), "degrees");
  Note("angle from the exporter's tangent, worst", angles.empty() ? 0.0 : angles.back(), "degrees");
  CHECK(handednessApart == 0,
        "every generated handedness is the one Blender's exporter wrote, exactly");
  CHECK(!angles.empty() && angles.back() <= kWorstAngleDeg,
        "no generated tangent is further from the exporter's than the guard above");

  Document generatedFile;
  Subject subject;
  const bool subjectRead = Read(kGenerated, "NormalTangentTest.gltf", generatedFile, subject);
  CHECK(subjectRead, "the generation case's own glTF reads and flattens");
  if (subjectRead) {
    CHECK(subject.Parts().size() == 1 && subject.Parts()[0].Tangent == TangentSource::Generated,
          "a primitive whose material samples a normal map and supplies no TANGENT gets one");
    double worstAgainstNormal = 0, worstLengthError = 0;
    for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
      const double *tangent = &subject.Tangents()[vertex * 4];
      const double *normal = &subject.Normals()[vertex * 3];
      double along = 0, square = 0;
      for (size_t axis = 0; axis < 3; ++axis) {
        along += tangent[axis] * normal[axis];
        square += tangent[axis] * tangent[axis];
      }
      worstAgainstNormal = std::max(worstAgainstNormal, std::fabs(along));
      worstLengthError = std::max(worstLengthError, std::fabs(std::sqrt(square) - 1.0));
      if (std::fabs(std::fabs(tangent[3]) - 1.0) > 0) { worstLengthError = 1.0; }
    }
    Note("worst |T.N| over the generated basis", worstAgainstNormal, "dimensionless");
    Note("worst |T| - 1 over the generated basis", worstLengthError, "dimensionless");
    /* A GENERATED BASIS IS ORTHONORMAL BY CONSTRUCTION AND THE BOUND IS THE ARITHMETIC'S: the last
     * step of the construction projects the tangent onto the plane of the shading normal and
     * normalises it, so what is left is the rounding of one dot product and one square root in
     * binary64 -- 2^-50, four orders below anything a shader could show. */
    CHECK(worstAgainstNormal < 1e-15,
          "every generated tangent lies in the plane of its own shading normal");
    CHECK(worstLengthError < 1e-15, "every generated tangent is unit and its handedness is +/-1");
  }

  Document plainFile;
  Subject plain;
  const bool plainRead = Read(kUnmapped, "scene.gltf", plainFile, plain);
  CHECK(plainRead, "a subject with no normal map reads and flattens");
  if (plainRead) {
    bool anyTangent = false;
    for (const outshine::Gltf::Part &part : plain.Parts()) {
      anyTangent = anyTangent || part.HasTangent();
    }
    CHECK(!anyTangent && !plain.HasTangent(),
          "a material that samples no normal map gets no basis, because nothing would read it");
  }

  return Report();
}
