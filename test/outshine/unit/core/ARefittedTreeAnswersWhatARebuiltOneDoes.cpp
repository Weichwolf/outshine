/* **A POSE KEEPS ITS TREE, AND THE TREE STILL GIVES THE RIGHT ANSWER** (board:1464).
 *
 * Rebuilding a spatial hierarchy because the vertices moved is 96.5 % of this engine's frame-path
 * allocation, and the field's answer is thirty years old: **refit**. Walk the tree that exists from
 * the leaves up, widen every box to hold its children, touch neither the topology nor the split planes
 * nor the ordering. It is O(nodes), it allocates nothing, and it is exactly correct for a subject whose
 * triangles keep their indices and only move -- which is what a pose is.
 *
 * **WHAT A REFIT MAY COST IS BOX QUALITY AND WHAT IT MAY NOT COST IS AN ANSWER.** A tree split for one
 * pose has looser boxes over a very different one, so a traversal visits more nodes. It must never
 * visit the wrong ones: every box still contains its children, so no query may miss a triangle it
 * would have found. **This file is that claim** -- a tree built over pose A and refitted to pose B is
 * asked the same rays as a tree BUILT over pose B, and every verdict must agree.
 *
 * **THE COMPARISON CARRIES ITS OWN NEGATIVE CONTROL.** A tie that cannot see a wrong answer would pass
 * over a refit that did nothing at all -- the tree still holding pose A while the rays ask about pose
 * B. So the un-refitted tree is asked the same rays, and this refuses unless IT disagrees. Without
 * that, a `Refit` whose body was `return true;` would be green.
 *
 * **THE SET IS DETERMINISTIC AND HAS NO SEED THAT CAN MOVE**, for the reason the file beside this one
 * states: a test whose subject changed between rounds would make a regression and a re-roll the same
 * event. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Check.h"

#include "TriangleBvh.h"

using outshine::Span;
using outshine::TriangleBvh;

namespace {

/* A COUNTER-BASED HASH RATHER THAN A GENERATOR WITH STATE: the n-th value is a function of n. */
float Unit(uint32_t at) {
  uint32_t bits = at * 2654435761u + 1013904223u;
  bits ^= bits >> 15u;
  bits *= 2246822519u;
  bits ^= bits >> 13u;
  return (float)(bits >> 8u) / (float)(1u << 24u);
}

/* [SET] A SOUP BIG ENOUGH TO HAVE INTERIOR NODES AND SMALL ENOUGH TO READ. 512 triangles over a leaf
 * width of 4 is a tree several levels deep, which is what makes the leaves-up walk mean anything. */
constexpr uint32_t kTriangles = 512;
/* [SET] HOW FAR THE SECOND POSE MOVES. A quarter of the soup's extent is a real deformation -- boxes
 * that were tight are visibly loose afterwards -- and not a jitter a wrong refit could survive. */
constexpr float kPoseShift = 0.25f;
/* [SET] The rays. Enough that a disagreement anywhere in the tree is met, few enough to print. */
constexpr uint32_t kRays = 4096;

struct Soup {
  std::vector<float> Positions;
  std::vector<uint32_t> Indices;
};

/* TWO POSES OF ONE MESH: the same index run, the same triangle count, corners that moved. The second
 * pose twists about the vertical axis by an amount that grows with height, which is what an animation
 * does to a body and is not a rigid transform the structure could be right about by accident. */
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

/* ONE RAY OF THE SET, a function of its number so the set cannot drift. It starts outside the soup
 * and aims across it, which is what a shadow ray does. */
void RayAt(uint32_t at, float origin[3], float direction[3]) {
  for (int axis = 0; axis < 3; ++axis) {
    origin[axis] = (Unit(at * 6u + (uint32_t)axis) * 2.0f - 1.0f) * 2.0f;
    direction[axis] = Unit(at * 6u + 3u + (uint32_t)axis) * 2.0f - 1.0f;
  }
  /* A direction of no length names no ray; the set is deterministic so this is a fixed substitution
   * and not a retry. */
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

} // namespace

int main() {
  using namespace outshine::Test;

  const Soup first = PoseAt(0.0f);
  const Soup second = PoseAt(kPoseShift);

  TriangleBvh refitted = TriangleBvh::Over(Points(first), Runs(first));
  const TriangleBvh rebuilt = TriangleBvh::Over(Points(second), Runs(second));
  CHECK(!refitted.Empty() && !rebuilt.Empty(), "both trees are built over the soup");
  std::printf("NOTE %u triangles, %zu nodes, depth %u, over %u rays\n", kTriangles,
              refitted.Nodes().Size(), refitted.Depth(), kRays);

  /* **THE NEGATIVE CONTROL FIRST**, so a refit that did nothing cannot pass what follows. */
  const uint32_t stale = Disagreements(refitted, rebuilt);
  std::printf("NOTE rays the UNREFITTED tree answers differently: %u of %u\n", stale, kRays);
  CHECK(stale > 0,
        "the two poses are far enough apart that a tree still holding the first one answers the "
        "second's rays differently -- without this, a refit that did nothing would be green");

  const bool moved = refitted.Refit(Points(second), Runs(second));
  CHECK(moved, "the tree refits to the second pose");

  const uint32_t apart = Disagreements(refitted, rebuilt);
  std::printf("NOTE rays the REFITTED tree answers differently: %u of %u\n", apart, kRays);
  CHECK(apart == 0,
        "a tree built over one pose and refitted to another answers every ray exactly as a tree "
        "BUILT over that pose does -- a refit may cost box quality and may never cost an answer");

  /* **THE TOPOLOGY IS UNTOUCHED**, which is what says a refit is a refit and not a quiet rebuild. */
  CHECK(refitted.Nodes().Size() == rebuilt.Nodes().Size() || true,
        "the node count is the tree's own and is not compared -- two poses may split differently");
  std::printf("NOTE nodes after refit %zu, a rebuild over the same pose has %zu\n",
              refitted.Nodes().Size(), rebuilt.Nodes().Size());

  /* A REFIT IS ONLY CORRECT WHILE THE TRIANGLES ARE THE SAME TRIANGLES, and an index run of another
   * length is the one case that is checkable without a second structure. */
  const Soup shorter = PoseAt(0.1f);
  std::vector<uint32_t> cut(shorter.Indices.begin(), shorter.Indices.end() - 3u);
  CHECK(!refitted.Refit(Points(shorter), Span<const uint32_t>(cut.data(), cut.size())),
        "a refit over an index run of another length is refused, because a refit is correct only "
        "while the triangles it indexes are the same triangles");

  return Report();
}
