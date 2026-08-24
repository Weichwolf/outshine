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

double Unit(uint32_t at) {
  uint32_t bits = at * 2654435761u + 1013904223u;
  bits ^= bits >> 15u;
  bits *= 2246822519u;
  bits ^= bits >> 13u;
  bits *= 3266489917u;
  bits ^= bits >> 16u;
  return (double)bits * 2.3283064365386963e-10;
}

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

}

int main(void) {
  using namespace outshine::Test;

  // board:1788: the two arms of this case measure different things, and only one of them
  // needs the full population.
  //
  // The PLAIN arm is the agreement proof: every ray the tree answers must agree with the scan
  // it replaces, and a rare disagreement is found by asking often. 4096 x 4096 = 16.8 million
  // ray-triangle tests is that population, and it stays.
  //
  // The SANITISED arm is a MEMORY proof. ASan and UBSan do not look for disagreement -- they
  // look for a read past a node, an unaligned load, a signed overflow in an index. Those fire
  // on the FIRST wrong access, not the millionth. So the TREE stays whole -- it is the
  // structure being walked, and a smaller one walks fewer paths -- and only the number of
  // WALKS falls. A first attempt cut the triangles too and the case objected: 1024 triangles
  // put the occluded share at 50 of 512, under the mixture bar this proof needs, because a
  // thinner scene is a different scene. The tree is the subject; the ray count is the sample.
  constexpr uint32_t kTriangles = 4096;
#if defined(__has_feature) && (__has_feature(address_sanitizer) || \
                               __has_feature(undefined_behavior_sanitizer))
  constexpr uint32_t kRays = 512;
#else
  constexpr uint32_t kRays = 4096;
#endif
  std::printf("NOTE triangles = %u, rays = %u, tests = %ld\n", kTriangles, kRays,
              (long)kTriangles * (long)kRays);
  const Soup soup = Grown(kTriangles);
  const TriangleBvh built =
      TriangleBvh::Over(Span<const float>(soup.PositionsM.data(), soup.PositionsM.size()),
                        Span<const uint32_t>(soup.Indices.data(), soup.Indices.size()));

  CHECK(!built.Empty(), "a soup of triangles builds a structure");
  CHECK(built.Triangles().Size() == kTriangles,
        "the structure holds every triangle it was given, once");
  CHECK(built.Depth() > 1,
        "and the population is deep enough to have interior nodes and escapes -- a flat tree "
        "exercises no walk (board:1788)");
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

  CHECK(occluded > kRays / 8 && occluded < kRays - kRays / 8,
        "the ray set is genuinely mixed, so agreement is not agreement with a constant");

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

  const TriangleBvh nothing = TriangleBvh::Over(Span<const float>(), Span<const uint32_t>());
  const float origin[3] = {0, 0, 0};
  const float along[3] = {0, 0, 1};
  CHECK(nothing.Empty(), "no triangles builds no structure");
  CHECK(!nothing.Occludes(origin, along, 0.0f, 1.0e30f), "every ray misses an empty structure");

  const uint32_t stray[4] = {0, 1, 2, 0};
  CHECK(TriangleBvh::Over(Span<const float>(soup.PositionsM.data(), soup.PositionsM.size()),
                          Span<const uint32_t>(stray, 4))
            .Empty(),
        "an index run that is not a multiple of three builds nothing");

  return Report();
}
