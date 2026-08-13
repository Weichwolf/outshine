/* WHAT AN ACCELERATION STRUCTURE IS FOR IS SPEED, so the only thing that can be wrong with it is the
 * ANSWER -- and the answer it must give is the one a linear scan over every triangle gives. That is
 * this file: the same Moller-Trumbore test, once through the tree and once over the whole soup, and
 * the two verdicts compared ray by ray.
 *
 * THE SCAN IS WRITTEN HERE AND NOT CALLED FROM THE SUBJECT, which is the point: a scan that shared
 * the structure's own intersection code would agree with it about a triangle it read wrong. It is
 * the second implementation, and it is the cheap one precisely because nobody has to make it fast.
 *
 * THE COMPARISON CARRIES ITS OWN NEGATIVE CONTROL. A tie that cannot see a missing occluder would
 * pass over a structure that returned "nothing is in the way" for every ray, which is the exact
 * failure mode a shadow term has: it looks like a scene with no shadows rather than like a bug. So
 * the scan is run once more with ONE triangle removed, and this refuses unless that disagrees.
 *
 * THE SOUP IS DETERMINISTIC AND HAS NO SEED THAT CAN MOVE. A structure whose test set changed
 * between rounds would make a regression and a re-roll the same event. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Check.h"

#include "TriangleBvh.h"

using outshine::BvhNode;
using outshine::BvhTriangle;
using outshine::kBvhNoEscape;
using outshine::Span;
using outshine::TriangleBvh;

namespace {

/* A COUNTER-BASED HASH RATHER THAN A GENERATOR WITH STATE: the n-th value is a function of n, so a
 * loop that changes shape does not change the set. */
double Unit(uint32_t at) {
  uint32_t bits = at * 2654435761u + 1013904223u;
  bits ^= bits >> 15u;
  bits *= 2246822519u;
  bits ^= bits >> 13u;
  bits *= 3266489917u;
  bits ^= bits >> 16u;
  return (double)bits * 2.3283064365386963e-10;
}

/* THE SOUP: triangles clustered rather than uniform, because a uniform cloud is the one distribution
 * a bad split heuristic still handles. Each triangle is a small facet placed on one of a handful of
 * shells, which is what a real subject's surface looks like to a tree. */
struct Soup {
  std::vector<float> PositionsM;
  std::vector<uint32_t> Indices;
};

Soup Grown(uint32_t triangles) {
  Soup out;
  for (uint32_t at = 0; at < triangles; ++at) {
    const double shell = 0.3 + 0.7 * (double)(at % 5u) / 4.0;
    const double lon = 2.0 * 3.14159265358979323846 * Unit(at * 7u);
    const double lat = std::acos(2.0 * Unit(at * 7u + 1u) - 1.0);
    const double centre[3] = {shell * std::sin(lat) * std::cos(lon), shell * std::cos(lat),
                              shell * std::sin(lat) * std::sin(lon)};
    for (int corner = 0; corner < 3; ++corner) {
      out.Indices.push_back((uint32_t)(out.PositionsM.size() / 3u));
      for (int axis = 0; axis < 3; ++axis) {
        const double jitter = 0.08 * (Unit(at * 31u + (uint32_t)(corner * 3 + axis)) - 0.5);
        out.PositionsM.push_back((float)(centre[axis] + jitter));
      }
    }
  }
  return out;
}

/* THE SCAN THE STRUCTURE REPLACES. `skip` is the negative control's whole mechanism: at any index
 * inside the soup one triangle stops being an occluder, and a comparison that cannot see that is a
 * comparison that could not see the structure losing one either. */
bool ScanOccludes(const Soup &soup, const float originM[3], const float direction[3], float nearM,
                  float distanceM, size_t skip) {
  const size_t triangles = soup.Indices.size() / 3u;
  for (size_t tri = 0; tri < triangles; ++tri) {
    if (tri == skip) { continue; }
    double corner[3][3];
    for (int which = 0; which < 3; ++which) {
      const uint32_t vertex = soup.Indices[tri * 3u + (size_t)which];
      for (int axis = 0; axis < 3; ++axis) {
        corner[which][axis] = soup.PositionsM[(size_t)vertex * 3u + (size_t)axis];
      }
    }
    double e1[3], e2[3];
    for (int axis = 0; axis < 3; ++axis) {
      e1[axis] = corner[1][axis] - corner[0][axis];
      e2[axis] = corner[2][axis] - corner[0][axis];
    }
    const double d[3] = {direction[0], direction[1], direction[2]};
    const double pvec[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2],
                            d[0] * e2[1] - d[1] * e2[0]};
    const double determinant = e1[0] * pvec[0] + e1[1] * pvec[1] + e1[2] * pvec[2];
    if (std::fabs(determinant) < 1.0e-20) { continue; }
    const double reciprocal = 1.0 / determinant;
    const double tvec[3] = {(double)originM[0] - corner[0][0], (double)originM[1] - corner[0][1],
                            (double)originM[2] - corner[0][2]};
    const double u = (tvec[0] * pvec[0] + tvec[1] * pvec[1] + tvec[2] * pvec[2]) * reciprocal;
    if (u < 0.0 || u > 1.0) { continue; }
    const double qvec[3] = {tvec[1] * e1[2] - tvec[2] * e1[1], tvec[2] * e1[0] - tvec[0] * e1[2],
                            tvec[0] * e1[1] - tvec[1] * e1[0]};
    const double v = (d[0] * qvec[0] + d[1] * qvec[1] + d[2] * qvec[2]) * reciprocal;
    if (v < 0.0 || u + v > 1.0) { continue; }
    const double hit = (e2[0] * qvec[0] + e2[1] * qvec[1] + e2[2] * qvec[2]) * reciprocal;
    if (hit > (double)nearM && hit < (double)distanceM) { return true; }
  }
  return false;
}

/* ONE RAY OF THE SET: from a point on a sphere well outside the soup, aimed at a point well inside
 * it, so roughly half the set hits and half misses. A set that all hit or all missed would agree
 * with a structure that answered the same thing every time. */
struct Ray {
  float OriginM[3];
  float Direction[3];
  float DistanceM;
};

Ray RayAt(uint32_t at) {
  Ray out;
  const double lon = 2.0 * 3.14159265358979323846 * Unit(at * 13u);
  const double lat = std::acos(2.0 * Unit(at * 13u + 1u) - 1.0);
  const double origin[3] = {3.0 * std::sin(lat) * std::cos(lon), 3.0 * std::cos(lat),
                            3.0 * std::sin(lat) * std::sin(lon)};
  const double aim[3] = {1.6 * (Unit(at * 13u + 2u) - 0.5), 1.6 * (Unit(at * 13u + 3u) - 0.5),
                         1.6 * (Unit(at * 13u + 4u) - 0.5)};
  double length = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double along = aim[axis] - origin[axis];
    length += along * along;
  }
  length = std::sqrt(length);
  for (int axis = 0; axis < 3; ++axis) {
    out.OriginM[axis] = (float)origin[axis];
    out.Direction[axis] = (float)((aim[axis] - origin[axis]) / length);
  }
  out.DistanceM = (float)length;
  return out;
}

/* EVERY NODE REACHED EXACTLY ONCE AND EVERY TRIANGLE NAMED EXACTLY ONCE. It is the invariant the
 * escape links carry, and it is the one thing the ray comparison cannot see: a tree that lost a
 * subtree would simply answer "not occluded" a little more often, which reads as a scene with fewer
 * shadows rather than as a structure with a hole. */
bool WholeAndOnce(const TriangleBvh &built) {
  std::vector<int> nodeSeen(built.Nodes().Size(), 0);
  std::vector<int> triangleSeen(built.Triangles().Size(), 0);
  uint32_t at = 0;
  size_t steps = 0;
  const size_t ceiling = built.Nodes().Size() * 2u + 8u;
  while (at != kBvhNoEscape && steps < ceiling) {
    ++steps;
    if (at >= built.Nodes().Size()) { return false; }
    const BvhNode &node = built.Nodes()[at];
    ++nodeSeen[at];
    if (node.IsLeaf()) {
      for (uint32_t which = 0; which < node.TriangleCount(); ++which) {
        const uint32_t triangle = node.FirstTriangle() + which;
        if (triangle >= triangleSeen.size()) { return false; }
        ++triangleSeen[triangle];
      }
      at = node.Escape;
      continue;
    }
    at = at + 1u;
  }
  if (at != kBvhNoEscape) { return false; }
  for (const int seen : nodeSeen) {
    if (seen != 1) { return false; }
  }
  for (const int seen : triangleSeen) {
    if (seen != 1) { return false; }
  }
  return true;
}

} // namespace

int main(void) {
  using namespace outshine::Test;

  constexpr uint32_t kTriangles = 4096;
  constexpr uint32_t kRays = 4096;
  const Soup soup = Grown(kTriangles);
  const TriangleBvh built =
      TriangleBvh::Over(Span<const float>(soup.PositionsM.data(), soup.PositionsM.size()),
                        Span<const uint32_t>(soup.Indices.data(), soup.Indices.size()));

  CHECK(!built.Empty(), "a soup of triangles builds a structure");
  CHECK(built.Triangles().Size() == kTriangles,
        "the structure holds every triangle it was given, once");
  std::printf("BVH %zu nodes over %zu triangles, depth %u\n", built.Nodes().Size(),
              built.Triangles().Size(), built.Depth());
  CHECK(WholeAndOnce(built),
        "the escape links reach every node once and name every triangle once");

  long occluded = 0;
  long disagreed = 0;
  for (uint32_t at = 0; at < kRays; ++at) {
    const Ray ray = RayAt(at);
    const bool tree = built.Occludes(ray.OriginM, ray.Direction, 0.0f, ray.DistanceM);
    const bool scan = ScanOccludes(soup, ray.OriginM, ray.Direction, 0.0f, ray.DistanceM,
                                   soup.Indices.size());
    if (tree) { ++occluded; }
    if (tree != scan) { ++disagreed; }
  }
  std::printf("RAYS %u, occluded %ld, disagreements %ld\n", kRays, occluded, disagreed);
  CHECK(disagreed == 0, "the structure gives the linear scan's answer on every ray");
  /* A SET THAT ALL HIT OR ALL MISSED WOULD AGREE WITH A CONSTANT, so the split is checked before
   * the agreement is believed. */
  CHECK(occluded > kRays / 8 && occluded < kRays - kRays / 8,
        "the ray set is genuinely mixed, so agreement is not agreement with a constant");

  /* THE NEGATIVE CONTROL. One triangle stops being an occluder in the scan; the structure still has
   * it, so the two must part company somewhere. The triangle is the one the ray set hits most, found
   * by asking rather than assumed -- a control aimed at a triangle nothing hits proves nothing. */
  long bestHits = 0;
  size_t bestTriangle = 0;
  for (size_t candidate = 0; candidate < 32; ++candidate) {
    long hits = 0;
    for (uint32_t at = 0; at < kRays; ++at) {
      const Ray ray = RayAt(at);
      if (ScanOccludes(soup, ray.OriginM, ray.Direction, 0.0f, ray.DistanceM, soup.Indices.size()) !=
          ScanOccludes(soup, ray.OriginM, ray.Direction, 0.0f, ray.DistanceM, candidate)) {
        ++hits;
      }
    }
    if (hits > bestHits) {
      bestHits = hits;
      bestTriangle = candidate;
    }
  }
  std::printf("CONTROL triangle %zu is the sole occluder of %ld rays\n", bestTriangle, bestHits);
  CHECK(bestHits > 0,
        "at least one of the first thirty-two triangles is the sole occluder of some ray, so the "
        "control has something to remove");
  long controlDisagreed = 0;
  for (uint32_t at = 0; at < kRays; ++at) {
    const Ray ray = RayAt(at);
    if (built.Occludes(ray.OriginM, ray.Direction, 0.0f, ray.DistanceM) !=
        ScanOccludes(soup, ray.OriginM, ray.Direction, 0.0f, ray.DistanceM, bestTriangle)) {
      ++controlDisagreed;
    }
  }
  CHECK(controlDisagreed == bestHits,
        "removing one occluder from the scan moves exactly the rays it occluded, so the comparison "
        "can see a triangle the structure would have lost");

  /* THE EMPTY STRUCTURE IS A STATE AND NOT A FAILURE: nothing occludes, which is what an absent
   * subject means to a light. */
  const TriangleBvh nothing = TriangleBvh::Over(Span<const float>(), Span<const uint32_t>());
  const float origin[3] = {0, 0, 0};
  const float along[3] = {0, 0, 1};
  CHECK(nothing.Empty(), "no triangles builds no structure");
  CHECK(!nothing.Occludes(origin, along, 0.0f, 1.0e30f), "every ray misses an empty structure");

  /* AN INDEX RUN THAT IS NOT A MULTIPLE OF THREE IS NOT A TRIANGLE LIST, and a structure over its
   * first floor(n/3) triangles would be a picture missing an occluder nobody could attribute. */
  const uint32_t stray[4] = {0, 1, 2, 0};
  CHECK(TriangleBvh::Over(Span<const float>(soup.PositionsM.data(), soup.PositionsM.size()),
                          Span<const uint32_t>(stray, 4))
            .Empty(),
        "an index run that is not a multiple of three builds nothing");

  return Report();
}
