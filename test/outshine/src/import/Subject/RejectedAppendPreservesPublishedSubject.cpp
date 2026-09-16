#include <scene/Geometry.h>
#include "Check.h"
#include "Subject.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry input;
  const int part = input.addPart("previous", {}).value();
  CHECK(input.setPositions(part, std::array<float, 9>{7, 0, 0, 8, 0, 0, 7, 1, 0}) &&
            input.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
        "published native input is valid");
  Gltf::Subject published;
  CHECK(published.Assemble(input), "published subject assembles");
  const auto *const positions = published.PositionsM().data();
  const auto *const parts = published.Parts().data();
  Gltf::Subject empty;
  CHECK(!published.Append(empty), "empty append is rejected");
  CHECK(published.VertexCount() == 3 && published.TriangleCount() == 1 &&
            published.Parts().size() == 1 && published.PositionsM().data() == positions &&
            published.Parts().data() == parts && published.PositionsM()[0] == 7,
        "rejected append preserves published data and borrowed views");
  Gltf::Subject valid;
  CHECK(valid.Assemble(input) && published.Append(valid), "valid retry appends after rejection");
  CHECK(published.VertexCount() == 6 && published.TriangleCount() == 2 &&
            published.Parts().size() == 2,
        "valid retry publishes the appended subject");
  return Report();
}
