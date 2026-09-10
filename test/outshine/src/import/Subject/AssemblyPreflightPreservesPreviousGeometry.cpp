#include <scene/Geometry.h>
#include "Subject.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array previousPositions{7.0f, 0.0f, 0.0f, 8.0f, 0.0f, 0.0f, 7.0f, 1.0f, 0.0f};
  constexpr std::array positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  constexpr std::array indices{0u, 1u, 2u};
  Geometry previous;
  const int original = previous.addPart("previous", {});
  CHECK(previous.setPositions(original, previousPositions) &&
            previous.setTriangles(original, indices),
        "previous native mesh prepared");
  Gltf::Subject subject;
  CHECK(subject.Assemble(previous), "previous subject assembled");
  const auto *storage = subject.PositionsM().data();
  Geometry candidate;
  for (int part = 0; part < 2; ++part) {
    const int added = candidate.addPart("candidate", {});
    CHECK(candidate.setPositions(added, positions) && candidate.setTriangles(added, indices),
          "candidate mesh prepared");
  }
  CHECK(candidate.setTriangles(1, std::array{0u, 1u, 3u}),
        "unresolved index is stored for validation");
  CHECK(!subject.Assemble(candidate), "invalid later part is rejected");
  CHECK(subject.VertexCount() == 3 && subject.TriangleCount() == 1 && subject.Parts().size() == 1 &&
            subject.Parts()[0].NodeName == "previous" && subject.PositionsM().data() == storage &&
            subject.PositionsM()[0] == 7,
        "structural failure preserves previous data and borrowed storage");
  Geometry empty;
  CHECK(!subject.Assemble(empty) && subject.PositionsM().data() == storage &&
            subject.PositionsM()[0] == 7,
        "empty input also preserves the previous subject");
  CHECK(candidate.setTriangles(1, indices) && candidate.setNormals(1, std::array{0.0f, 0.0f, 1.0f}),
        "incomplete optional attribute prepared");
  CHECK(!subject.Assemble(candidate) && subject.PositionsM().data() == storage &&
            subject.PositionsM()[0] == 7,
        "attribute count failure preserves the previous subject");
  CHECK(candidate.setNormals(1, {}), "remove incomplete optional attribute");
  CHECK(candidate.setTriangles(1, indices) && subject.Assemble(candidate),
        "corrected retry succeeds");
  CHECK(subject.VertexCount() == 6 && subject.TriangleCount() == 2 && subject.Parts().size() == 2,
        "valid assembly publishes every part");
  return Report();
}
