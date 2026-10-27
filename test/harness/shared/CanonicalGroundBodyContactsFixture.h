#ifndef OUTSHINE_TEST_CANONICALGROUNDBODYCONTACTSFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDBODYCONTACTSFIXTURE_H

#include "Check.h"
#include "CppAllocationProbe.h"
#include "EarthworkDeformation.h"
#include "GroundPortBodies.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <format>
#include <print>
#include <ratio>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Test::CanonicalGroundFixture {
inline void WriteContactSamples(std::span<const EastNorth> samples,
                                std::span<const double> heights,
                                std::span<const double> wanted,
                                size_t which,
                                const char *path) {
  if (path == nullptr) { return; }
  const auto filename = std::format("{}.contacts-{}.csv", path, which);
  std::FILE *file = std::fopen(filename.c_str(), "wb");
  CHECK(file != nullptr, "explicit native deformation diagnostics have a writable destination");
  if (file == nullptr) { return; }
  CHECK(std::fprintf(file, "east_m,north_m,initial_m,actual_bed_m,body_bed_m\n") > 0,
        "native deformation diagnostic fields are explicit");
  for (size_t at = 0; at < samples.size(); ++at) {
    CHECK(std::fprintf(file,
                       "%.17g,%.17g,%.17g,%.17g,%.17g\n",
                       samples[at].EastM,
                       samples[at].NorthM,
                       wanted[at] - 1,
                       heights[at],
                       wanted[at]) > 0,
          "each exported diagnostic height comes from the actual completed terrain job");
  }
  CHECK(std::fclose(file) == 0, "native deformation diagnostics close successfully");
}

inline void CheckNativeContactDeformation(std::vector<EarthworkStamp> stamps,
                                          std::span<const EastNorth> samples,
                                          std::span<const double> wanted,
                                          size_t which,
                                          const char *path) {
  for (const bool reverse : {false, true}) {
    if (reverse) { std::ranges::reverse(stamps); }
    for (const double offsetM : {-1.0, 1.0}) {
      std::vector<double> heights(wanted.begin(), wanted.end());
      for (auto &height : heights) { height += offsetM; }
      const auto begin = std::chrono::steady_clock::now();
      BeginCppAllocationProbe();
      EarthworkDeformationJob job(stamps, samples, heights, 2);
      size_t heapBytes = job.HeapBytes();
      size_t advances = 1;
      while (!job.Advance(256)) {
        heapBytes = std::max(heapBytes, job.HeapBytes());
        ++advances;
      }
      heapBytes = std::max(heapBytes, job.HeapBytes());
      const auto pressed = job.Take();
      const auto allocations = EndCppAllocationProbe();
      const auto elapsed =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin)
              .count();
      std::println("GROUND_CONTACT_CONSUMER faces={} points={} reverse={} offset_m={} ms={} "
                   "allocations={} requested_bytes={} observed_job_heap={} advances={} "
                   "inside_claims={} moved={}",
                   stamps.size(),
                   samples.size(),
                   reverse,
                   offsetM,
                   elapsed,
                   allocations.Calls,
                   allocations.Bytes,
                   heapBytes,
                   advances,
                   pressed.Inside.size(),
                   pressed.Moved);
      if (!reverse && offsetM < 0) { WriteContactSamples(samples, heights, wanted, which, path); }
      CHECK(pressed.Structures == 0 && pressed.Held == 0,
            "the actual native deformation consumer accepts the complete finite bed contacts");
      for (size_t at = 0; at < wanted.size(); ++at) {
        CHECK_NEAR(heights[at],
                   wanted[at],
                   1e-6,
                   "m",
                   "raised and lowered control terrain meets each real affine/C1 body underside "
                   "independently of contact publication order");
      }
    }
  }
}

inline void
CheckBodyContacts(const Generators::GroundPortBodies &batch, size_t which, const char *path) {
  std::vector<EarthworkStamp> stamps;
  std::vector<EastNorth> samples;
  std::vector<double> wanted;
  for (const auto &body : batch.Bodies) {
    CHECK(body.TerrainContacts.size() == body.Geometry.SurfaceFaces,
          "each actual source body underside supplies one owned terrain contact per finite face");
    if (body.TerrainContacts.size() != body.Geometry.SurfaceFaces) { return; }
    for (size_t face = 0; face < body.Geometry.SurfaceFaces; ++face) {
      const auto &stamp = body.TerrainContacts[face];
      CHECK(stamp.CorridorKey == body.Source.PathId && stamp.Fills &&
                stamp.Kind == EarthworkKind::Corridor && stamp.PlanarSupportM == 0 &&
                stamp.RingEastNorthM == stamp.SeamEastNorthM && stamp.RingEastNorthM.size() == 6,
            "terrain contacts keep actual physical source identity and the existing planar seam");
      const auto indices = body.Geometry.Faces[body.Geometry.SurfaceFaces + face];
      std::array<std::array<double, 3>, 3> bottom{};
      for (size_t corner = 0; corner < 3; ++corner) {
        bottom[corner] = body.Geometry.Points[indices[2 - corner]];
        const auto point = bottom[corner];
        CHECK(stamp.RingEastNorthM[2 * corner] == point[0] &&
                  stamp.RingEastNorthM[2 * corner + 1] == point[1],
              "every contact footprint is exactly the actual underside face");
        CHECK_NEAR(stamp.WantsAt({.EastM = point[0], .NorthM = point[1]}),
                   point[2],
                   1e-8,
                   "m",
                   "the shared contact plane passes through every actual bed vertex");
      }
      samples.push_back({.EastM = (bottom[0][0] + bottom[1][0] + bottom[2][0]) / 3,
                         .NorthM = (bottom[0][1] + bottom[1][1] + bottom[2][1]) / 3});
      wanted.push_back((bottom[0][2] + bottom[1][2] + bottom[2][2]) / 3);
      stamps.push_back(stamp);
    }
  }
  CheckNativeContactDeformation(std::move(stamps), samples, wanted, which, path);
}
}
#endif
