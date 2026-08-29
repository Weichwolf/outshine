#include "ClusterCook.h"

#include <algorithm>
#include <cmath>

namespace outshine {
namespace {

[[nodiscard]] uint32_t Spread(uint32_t bits) {
  bits &= 0x3ffu;
  bits = (bits | (bits << 16)) & 0x030000ffu;
  bits = (bits | (bits << 8)) & 0x0300f00fu;
  bits = (bits | (bits << 4)) & 0x030c30c3u;
  bits = (bits | (bits << 2)) & 0x09249249u;
  return bits;
}

[[nodiscard]] uint32_t Morton(const float at[3], const float least[3], const float span[3]) {
  uint32_t held[3];
  for (int axis = 0; axis < 3; ++axis) {
    const float part = span[axis] > 0.0f ? (at[axis] - least[axis]) / span[axis] : 0.0f;
    const float held01 = part < 0.0f ? 0.0f : (part > 1.0f ? 1.0f : part);
    held[axis] = (uint32_t)(held01 * 1023.0f);
  }
  return (Spread(held[0]) << 2) | (Spread(held[1]) << 1) | Spread(held[2]);
}

}

Cooked CookClusters(std::span<const float> positionsM, std::span<const uint32_t> indices,
                    uint32_t mostTriangles) {
  Cooked out;
  const size_t triangles = indices.size() / 3;
  if (triangles == 0 || positionsM.size() < 3 || mostTriangles == 0) { return out; }

  float least[3] = {positionsM[0], positionsM[1], positionsM[2]};
  float most[3] = {positionsM[0], positionsM[1], positionsM[2]};
  for (size_t vertex = 0; vertex * 3 + 2 < positionsM.size(); ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const float held = positionsM[vertex * 3 + (size_t)axis];
      least[axis] = held < least[axis] ? held : least[axis];
      most[axis] = held > most[axis] ? held : most[axis];
    }
  }
  const float span[3] = {most[0] - least[0], most[1] - least[1], most[2] - least[2]};

  struct Sorted {
    uint32_t Code = 0;
    uint32_t Triangle = 0;
  };
  std::vector<Sorted> order;
  order.reserve(triangles);
  const size_t vertices = positionsM.size() / 3;
  for (size_t triangle = 0; triangle < triangles; ++triangle) {
    float centre[3] = {0.0f, 0.0f, 0.0f};
    for (int corner = 0; corner < 3; ++corner) {
      const uint32_t at = indices[triangle * 3 + (size_t)corner];
      if ((size_t)at >= vertices) { continue; }
      for (int axis = 0; axis < 3; ++axis) {
        centre[axis] += positionsM[(size_t)at * 3 + (size_t)axis] / 3.0f;
      }
    }
    order.push_back(Sorted{Morton(centre, least, span), (uint32_t)triangle});
  }
  std::sort(order.begin(), order.end(),
            [](const Sorted &a, const Sorted &b) { return a.Code < b.Code; });

  out.Index.reserve(indices.size());
  out.Clusters.reserve((triangles + mostTriangles - 1) / mostTriangles);
  for (size_t at = 0; at < order.size(); at += mostTriangles) {
    const size_t upTo = at + mostTriangles < order.size() ? at + mostTriangles : order.size();
    DagCluster made{};
    made.First = (uint32_t)out.Index.size();
    made.Count = (uint32_t)((upTo - at) * 3);
    made.ParentErr = kDagRootErr;

    float low[3] = {0.0f, 0.0f, 0.0f};
    float high[3] = {0.0f, 0.0f, 0.0f};
    bool any = false;
    for (size_t step = at; step < upTo; ++step) {
      const uint32_t triangle = order[step].Triangle;
      for (int corner = 0; corner < 3; ++corner) {
        const uint32_t index = indices[(size_t)triangle * 3 + (size_t)corner];
        out.Index.push_back(index);
        if ((size_t)index >= vertices) { continue; }
        for (int axis = 0; axis < 3; ++axis) {
          const float held = positionsM[(size_t)index * 3 + (size_t)axis];
          if (!any || held < low[axis]) { low[axis] = held; }
          if (!any || held > high[axis]) { high[axis] = held; }
        }
        any = true;
      }
    }
    double radius = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      made.SelfCenter[axis] = 0.5f * (low[axis] + high[axis]);
      const double half = 0.5 * ((double)high[axis] - (double)low[axis]);
      radius += half * half;
    }
    made.SelfRadius = (float)std::sqrt(radius);
    out.Clusters.push_back(made);
  }
  return out;
}

}
