#include "TriangleBvh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace outshine {
namespace {

/* HOW MANY BUCKETS THE SPLIT SEARCH USES PER AXIS. [SET] 12, which is what a binned surface-area
 * heuristic is conventionally built with: the cost curve over a bucket count is flat past about
 * eight and every bucket is one more pass over the axis. */
constexpr int kBins = 12;
/* WHERE A SPLIT STOPS BEING TRIED. A node whose triangles all share one centroid has no split at
 * all -- the box would be handed to one child whole and the recursion would not end -- so it
 * becomes a leaf however many triangles it holds. */
constexpr uint32_t kMaxLeafBits = 8;
constexpr uint32_t kMaxLeafTriangles = (1u << kMaxLeafBits) - 1u;

struct Box {
  float Min[3] = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                  std::numeric_limits<float>::infinity()};
  float Max[3] = {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                  -std::numeric_limits<float>::infinity()};

  void Cover(const float point[3]) {
    for (int axis = 0; axis < 3; ++axis) {
      Min[axis] = std::min(Min[axis], point[axis]);
      Max[axis] = std::max(Max[axis], point[axis]);
    }
  }
  /* COMPONENT-WISE, AND NEVER `Cover(other.Min); Cover(other.Max);`. An empty box carries `+inf` as
   * its minimum and `-inf` as its maximum, and covering those two as POINTS makes the result
   * infinite in both directions -- which is what an empty split bin did to every surface-area cost
   * downstream of it, so no split ever beat not splitting and the tree was built entirely by the
   * fallback. This form absorbs an empty box exactly: `min(x, +inf)` and `max(x, -inf)` are `x`. */
  void Cover(const Box &other) {
    for (int axis = 0; axis < 3; ++axis) {
      Min[axis] = std::min(Min[axis], other.Min[axis]);
      Max[axis] = std::max(Max[axis], other.Max[axis]);
    }
  }
  [[nodiscard]] float HalfArea() const {
    const float dx = Max[0] - Min[0], dy = Max[1] - Min[1], dz = Max[2] - Min[2];
    if (dx < 0.0f || dy < 0.0f || dz < 0.0f) { return 0.0f; }
    return dx * dy + dy * dz + dz * dx;
  }
};

/* THE BUILD'S OWN WORKING STATE, one object rather than six arrays threaded through a recursion
 * (`I.23`). `Order` is the permutation being sorted in place; everything else is read-only after
 * the first pass. */
struct Building {
  std::vector<Box> Bounds;
  std::vector<float> Centroids;   /* 3 per triangle */
  std::vector<uint32_t> Order;
  std::vector<BvhNode> Nodes;
  /* The right child of each interior node, in the same index space as `Nodes`. The left child is
   * always the next node, so only this one needs recording -- and it is dropped once the escape
   * links are threaded, which is why it is here and not in the node. */
  std::vector<uint32_t> Right;
  uint32_t Depth = 0;
};

/* Emits one subtree over `Order[first, first + count)` and returns its node index. Depth-first, so
 * the left child lands at `here + 1` and needs no link of its own. */
uint32_t Emit(Building &work, uint32_t first, uint32_t count, uint32_t depth) {
  const uint32_t here = (uint32_t)work.Nodes.size();
  work.Nodes.emplace_back();
  work.Right.push_back(0);
  work.Depth = std::max(work.Depth, depth + 1u);

  Box box;
  Box centroidBox;
  for (uint32_t at = 0; at < count; ++at) {
    const uint32_t tri = work.Order[first + at];
    box.Cover(work.Bounds[tri]);
    centroidBox.Cover(work.Centroids.data() + (size_t)tri * 3u);
  }

  const auto MakeLeaf = [&](void) {
    work.Nodes[here].Leaf = (count << kBvhLeafFirstBits) | first;
  };

  uint32_t split = 0;
  if (count > kBvhLeafTriangles) {
    /* THE SURFACE-AREA HEURISTIC OVER THE WIDEST CENTROID AXIS. Splitting the widest axis is what
     * makes the buckets mean anything: on a narrow axis every triangle lands in one bucket and the
     * search returns the split it started with. */
    int axis = 0;
    float widest = centroidBox.Max[0] - centroidBox.Min[0];
    for (int candidate = 1; candidate < 3; ++candidate) {
      const float width = centroidBox.Max[candidate] - centroidBox.Min[candidate];
      if (width > widest) {
        widest = width;
        axis = candidate;
      }
    }
    if (widest > 0.0f) {
      const float scale = (float)kBins / widest;
      Box binBox[kBins];
      uint32_t binCount[kBins] = {};
      const auto BinOf = [&](uint32_t tri) {
        const float offset = work.Centroids[(size_t)tri * 3u + (size_t)axis] - centroidBox.Min[axis];
        const int at = (int)(offset * scale);
        return std::min(std::max(at, 0), kBins - 1);
      };
      for (uint32_t at = 0; at < count; ++at) {
        const uint32_t tri = work.Order[first + at];
        const int bin = BinOf(tri);
        binBox[bin].Cover(work.Bounds[tri]);
        ++binCount[bin];
      }
      /* The sweep from each side, so every candidate plane's two half-costs are one array read. */
      float leftCost[kBins - 1] = {};
      float rightCost[kBins - 1] = {};
      Box sweep;
      uint32_t running = 0;
      for (int bin = 0; bin < kBins - 1; ++bin) {
        sweep.Cover(binBox[bin]);
        running += binCount[bin];
        leftCost[bin] = sweep.HalfArea() * (float)running;
      }
      sweep = Box();
      running = 0;
      for (int bin = kBins - 1; bin > 0; --bin) {
        sweep.Cover(binBox[bin]);
        running += binCount[bin];
        rightCost[bin - 1] = sweep.HalfArea() * (float)running;
      }
      int bestPlane = -1;
      float bestCost = std::numeric_limits<float>::infinity();
      for (int bin = 0; bin < kBins - 1; ++bin) {
        const float cost = leftCost[bin] + rightCost[bin];
        if (cost < bestCost) {
          bestCost = cost;
          bestPlane = bin;
        }
      }
      /* THE SPLIT IS TAKEN ONLY IF IT BEATS NOT SPLITTING, which is what stops a leaf being cut
       * into two leaves that a ray then has to enter both of. The parent's own cost is its area
       * times its triangle count; the traversal step is one node fetch and is priced at one
       * triangle test, which is the conventional ratio and is a [SET] number here. */
      const float leafCost = box.HalfArea() * (float)count;
      if (bestPlane >= 0 && bestCost + box.HalfArea() < leafCost) {
        const auto middle = std::partition(
            work.Order.begin() + first, work.Order.begin() + first + count,
            [&](uint32_t tri) { return BinOf(tri) <= bestPlane; });
        split = (uint32_t)(middle - (work.Order.begin() + first));
      }
    }
  }

  if (split == 0 || split == count) {
    if (count <= kMaxLeafTriangles) {
      MakeLeaf();
    } else {
      /* NO SPLIT AND TOO MANY TO NAME IN ONE LEAF: cut the run in half by index. It is not a
       * heuristic and it is not meant to be -- it is what keeps a pile of coincident centroids from
       * being a leaf the count field cannot hold. */
      split = count / 2u;
    }
  }

  if (work.Nodes[here].Leaf == kBvhInterior) {
    Emit(work, first, split, depth + 1u);
    /* THE CALL FIRST AND THE SUBSCRIPT AFTER, because `work.Right` grows inside the call and a
     * reference taken before it would be into the old block (`ES.43`). */
    const uint32_t right = Emit(work, first + split, count - split, depth + 1u);
    work.Right[here] = right;
  }

  std::memcpy(work.Nodes[here].MinM, box.Min, sizeof box.Min);
  std::memcpy(work.Nodes[here].MaxM, box.Max, sizeof box.Max);
  return here;
}

/* THREADS THE ESCAPE LINKS over a finished depth-first tree. A node's escape is where a traversal
 * that has finished with this node goes: for a left child that is its sibling, for a right child
 * that is whatever its parent escapes to. */
void Thread(Building &work, uint32_t here, uint32_t escape) {
  work.Nodes[here].Escape = escape;
  if (work.Nodes[here].IsLeaf()) { return; }
  Thread(work, here + 1u, work.Right[here]);
  Thread(work, work.Right[here], escape);
}

} // namespace

TriangleBvh TriangleBvh::Over(Span<const float> positionsM, Span<const uint32_t> indices) {
  TriangleBvh built;
  const size_t triangles = indices.Size() / 3u;
  if (triangles == 0 || indices.Size() % 3u != 0 || triangles > kBvhLeafFirstMask) { return built; }
  const size_t vertices = positionsM.Size() / 3u;

  Building work;
  work.Bounds.resize(triangles);
  work.Centroids.resize(triangles * 3u);
  work.Order.resize(triangles);
  built.Tris_.resize(triangles);

  for (size_t tri = 0; tri < triangles; ++tri) {
    work.Order[tri] = (uint32_t)tri;
    float corner[3][3];
    for (int corner_at = 0; corner_at < 3; ++corner_at) {
      const uint32_t vertex = indices[tri * 3u + (size_t)corner_at];
      /* AN INDEX PAST THE RUN IS THE ORIGIN AND NOT A REFUSAL: the reader is what validates a
       * document, and a degenerate triangle here misses every ray, which is the same answer a
       * missing triangle gives. */
      for (int axis = 0; axis < 3; ++axis) {
        corner[corner_at][axis] =
            vertex < vertices ? positionsM[(size_t)vertex * 3u + (size_t)axis] : 0.0f;
      }
      work.Bounds[tri].Cover(corner[corner_at]);
    }
    for (int axis = 0; axis < 3; ++axis) {
      work.Centroids[tri * 3u + (size_t)axis] =
          (work.Bounds[tri].Min[axis] + work.Bounds[tri].Max[axis]) * 0.5f;
    }
  }

  work.Nodes.reserve(triangles * 2u);
  work.Right.reserve(triangles * 2u);
  Emit(work, 0, (uint32_t)triangles, 0);
  Thread(work, 0, kBvhNoEscape);

  /* THE TRIANGLES ARE WRITTEN IN THE PERMUTATION'S ORDER, so a leaf's run is contiguous in memory
   * and the traversal reads it as one. A leaf naming scattered indices would cost one indirection
   * per triangle for the whole of the structure's life. */
  for (size_t at = 0; at < triangles; ++at) {
    const uint32_t tri = work.Order[at];
    float corner[3][3];
    for (int corner_at = 0; corner_at < 3; ++corner_at) {
      const uint32_t vertex = indices[(size_t)tri * 3u + (size_t)corner_at];
      for (int axis = 0; axis < 3; ++axis) {
        corner[corner_at][axis] =
            vertex < vertices ? positionsM[(size_t)vertex * 3u + (size_t)axis] : 0.0f;
      }
    }
    BvhTriangle &out = built.Tris_[at];
    for (int axis = 0; axis < 3; ++axis) {
      out.V0[axis] = corner[0][axis];
      out.E1[axis] = corner[1][axis] - corner[0][axis];
      out.E2[axis] = corner[2][axis] - corner[0][axis];
    }
  }

  built.Nodes_ = std::move(work.Nodes);
  built.Depth_ = work.Depth;
  return built;
}

/* THE SAME TRAVERSAL THE SHADER RUNS, and it is here so that the device's answer has something to
 * be checked against on a processor that has no device. */
bool TriangleBvh::Occludes(const float originM[3], const float direction[3], float nearM,
                           float distanceM) const {
  if (Nodes_.empty()) { return false; }
  float inverse[3];
  for (int axis = 0; axis < 3; ++axis) {
    inverse[axis] = 1.0f / direction[axis];
  }

  uint32_t at = 0;
  while (at != kBvhNoEscape) {
    const BvhNode &node = Nodes_[at];
    float enter = nearM;
    float leave = distanceM;
    for (int axis = 0; axis < 3; ++axis) {
      const float first = (node.MinM[axis] - originM[axis]) * inverse[axis];
      const float second = (node.MaxM[axis] - originM[axis]) * inverse[axis];
      enter = std::max(enter, std::min(first, second));
      leave = std::min(leave, std::max(first, second));
    }
    if (enter > leave) {
      at = node.Escape;
      continue;
    }
    if (!node.IsLeaf()) {
      at = at + 1u;
      continue;
    }
    const uint32_t first = node.FirstTriangle();
    const uint32_t count = node.TriangleCount();
    for (uint32_t which = 0; which < count; ++which) {
      const BvhTriangle &tri = Tris_[first + which];
      /* MOLLER-TRUMBORE, TWO-SIDED. A shadow does not ask which face of an occluder it met:
       * culling by winding here would let a subject wound the other way stop casting. */
      float pvec[3] = {direction[1] * tri.E2[2] - direction[2] * tri.E2[1],
                       direction[2] * tri.E2[0] - direction[0] * tri.E2[2],
                       direction[0] * tri.E2[1] - direction[1] * tri.E2[0]};
      const float determinant =
          tri.E1[0] * pvec[0] + tri.E1[1] * pvec[1] + tri.E1[2] * pvec[2];
      if (std::fabs(determinant) < 1.0e-20f) { continue; }
      const float reciprocal = 1.0f / determinant;
      const float tvec[3] = {originM[0] - tri.V0[0], originM[1] - tri.V0[1],
                             originM[2] - tri.V0[2]};
      const float u = (tvec[0] * pvec[0] + tvec[1] * pvec[1] + tvec[2] * pvec[2]) * reciprocal;
      if (u < 0.0f || u > 1.0f) { continue; }
      const float qvec[3] = {tvec[1] * tri.E1[2] - tvec[2] * tri.E1[1],
                             tvec[2] * tri.E1[0] - tvec[0] * tri.E1[2],
                             tvec[0] * tri.E1[1] - tvec[1] * tri.E1[0]};
      const float v =
          (direction[0] * qvec[0] + direction[1] * qvec[1] + direction[2] * qvec[2]) * reciprocal;
      if (v < 0.0f || u + v > 1.0f) { continue; }
      const float hit =
          (tri.E2[0] * qvec[0] + tri.E2[1] * qvec[1] + tri.E2[2] * qvec[2]) * reciprocal;
      if (hit > nearM && hit < distanceM) { return true; }
    }
    at = node.Escape;
  }
  return false;
}

} // namespace outshine
