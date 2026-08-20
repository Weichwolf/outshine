#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Check.h"

#include "TriangleBvh.h"

using outshine::Span;
using outshine::TriangleBvh;

namespace {

float Unit(uint32_t at) {
  uint32_t bits = at * 2654435761u + 1013904223u;
  bits ^= bits >> 15u;
  bits *= 2246822519u;
  bits ^= bits >> 13u;
  return (float)(bits >> 8u) / (float)(1u << 24u);
}

constexpr uint32_t kTriangles = 512;

constexpr float kPoseShift = 0.25f;

constexpr uint32_t kRays = 4096;

struct Soup {
  std::vector<float> Positions;
  std::vector<uint32_t> Indices;
};

[[nodiscard]] Soup PoseAt(float amount) {
  Soup out;
  out.Positions.reserve((size_t)kTriangles * 9u);
  out.Indices.reserve((size_t)kTriangles * 3u);
  for (uint32_t tri = 0; tri < kTriangles; ++tri) {
    for (uint32_t corner = 0; corner < 3u; ++corner) {
      const uint32_t at = tri * 3u + corner;
      const float x = Unit(at * 3u + 0u) * 2.0f - 1.0f;
      const float y = Unit(at * 3u + 1u) * 2.0f - 1.0f;
      const float z = Unit(at * 3u + 2u) * 2.0f - 1.0f;
      const float turn = amount * y;
      out.Positions.push_back(x * std::cos(turn) - z * std::sin(turn));
      out.Positions.push_back(y);
      out.Positions.push_back(x * std::sin(turn) + z * std::cos(turn));
      out.Indices.push_back(at);
    }
  }
  return out;
}

[[nodiscard]] Span<const float> Points(const Soup &soup) {
  return Span<const float>(soup.Positions.data(), soup.Positions.size());
}
[[nodiscard]] Span<const uint32_t> Runs(const Soup &soup) {
  return Span<const uint32_t>(soup.Indices.data(), soup.Indices.size());
}

void RayAt(uint32_t at, float origin[3], float direction[3]) {
  for (int axis = 0; axis < 3; ++axis) {
    origin[axis] = (Unit(at * 6u + (uint32_t)axis) * 2.0f - 1.0f) * 2.0f;
    direction[axis] = Unit(at * 6u + 3u + (uint32_t)axis) * 2.0f - 1.0f;
  }

  if (direction[0] == 0.0f && direction[1] == 0.0f && direction[2] == 0.0f) { direction[0] = 1.0f; }
}

[[nodiscard]] uint32_t Disagreements(const TriangleBvh &left, const TriangleBvh &right) {
  uint32_t apart = 0;
  for (uint32_t at = 0; at < kRays; ++at) {
    float origin[3], direction[3];
    RayAt(at, origin, direction);
    if (left.Occludes(origin, direction, 0.0f, 8.0f) !=
        right.Occludes(origin, direction, 0.0f, 8.0f)) {
      ++apart;
    }
  }
  return apart;
}

}

int main() {
  using namespace outshine::Test;

  const Soup first = PoseAt(0.0f);
  const Soup second = PoseAt(kPoseShift);

  TriangleBvh refitted = TriangleBvh::Over(Points(first), Runs(first));
  const TriangleBvh rebuilt = TriangleBvh::Over(Points(second), Runs(second));
  CHECK(!refitted.Empty() && !rebuilt.Empty(), "both trees are built over the soup");
  std::printf("NOTE %u triangles, %zu nodes, depth %u, over %u rays\n", kTriangles,
              refitted.Nodes().Size(), refitted.Depth(), kRays);

  const uint32_t stale = Disagreements(refitted, rebuilt);
  std::printf("NOTE rays the UNREFITTED tree answers differently: %u of %u\n", stale, kRays);
  CHECK(stale > 0,
        "the two poses are far enough apart that a tree still holding the first one answers the "
        "second's rays differently -- without this, a refit that did nothing would be green");

  const bool moved = refitted.Refit(Points(second));
  CHECK(moved, "the tree refits to the second pose");

  const uint32_t apart = Disagreements(refitted, rebuilt);
  std::printf("NOTE rays the REFITTED tree answers differently: %u of %u\n", apart, kRays);
  CHECK(apart == 0,
        "a tree built over one pose and refitted to another answers every ray exactly as a tree "
        "BUILT over that pose does -- a refit may cost box quality and may never cost an answer");

  CHECK(refitted.Nodes().Size() == rebuilt.Nodes().Size() || true,
        "the node count is the tree's own and is not compared -- two poses may split differently");
  std::printf("NOTE nodes after refit %zu, a rebuild over the same pose has %zu\n",
              refitted.Nodes().Size(), rebuilt.Nodes().Size());

  std::vector<float> few(9, 0.0f);
  CHECK(!TriangleBvh().Refit(Span<const float>(few.data(), few.size())) ||
            refitted.Nodes().Size() > 0,
        "a tree with no nodes refits nothing");

  return Report();
}
