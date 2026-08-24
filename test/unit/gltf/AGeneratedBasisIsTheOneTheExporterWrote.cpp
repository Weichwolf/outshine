#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include <filesystem>

#include "PreparedRoot.h"

#include "Document.h"
#include "Subject.h"
#include "Tangents.h"

using outshine::Gltf::Document;
using outshine::Gltf::Subject;
using outshine::Gltf::TangentSource;

namespace {

const std::string kMirror = outshine::Test::PreparedRoot() + "/test-render-khronos-glTF-NormalTangentMirrorTest/";
const std::string kGenerated = outshine::Test::PreparedRoot() + "/test-render-khronos-glTF-NormalTangentTest/";

const std::string kUnmapped = outshine::Test::PreparedRoot() + "/test-render-khronos-glTF-SimpleTexture-simple-texture/";

constexpr double kWorstAngleDeg = 0.01;

// board:1798: a prepared subject that is not on disk is UNPREPARED and never FAILED. The two
// words mean different things -- UNPREPARED says this run judged nothing here, FAIL says the
// code is wrong -- and spending the second on the first makes a swept temp directory read as a
// regression. It also names the path, which is what lets the runner find the case that OWNS
// the subject and rebuild it (board:1797).
bool Present(const std::string &path) {
  return std::filesystem::exists(path);
}

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

}

int main() {
  using namespace outshine::Test;

  for (const std::string &needed : {kMirror + "NormalTangentMirrorTest.gltf",
                                    kGenerated + "NormalTangentTest.gltf",
                                    kUnmapped + "scene.gltf"}) {
    if (!Present(needed)) {
      Unprepared((needed + " is not prepared -- run python3 "
                           "test/harness/shared/corpus/prepare.py all --manifest <its manifest>")
                     .c_str());
      return Report();
    }
  }

  Document mirrorFile;
  Subject mirror;
  const bool mirrorRead = Read(kMirror, "NormalTangentMirrorTest.gltf", mirrorFile, mirror);
  CHECK(mirrorRead, "the mirror case's own glTF reads and flattens");
  if (!mirrorRead) { return Report(); }

  CHECK(mirror.Parts().size() == 1 && mirror.Parts()[0].Tangent == TangentSource::Supplied,
        "a primitive that supplies TANGENT has it taken verbatim and is never regenerated");
  CHECK(mirror.HasTangent() && mirror.Tangents().size() == mirror.VertexCount() * 4,
        "the supplied basis covers every vertex, four numbers each");

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
