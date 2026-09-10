#include "Structures.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include "math/Vec3.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Generators::Structures producer;
  for (const auto location : std::array<LongitudeLatitude, 4>{
           {{0, 0}, {16.37, 48.21}, {-122.42, 37.77}, {151.21, -33.87}}}) {
    for (const uint64_t seed : {0u, 1u, 31u}) {
      Generators::Request request;
      request.LatitudeDeg = location.LatitudeDeg;
      request.LongitudeDeg = location.LongitudeDeg;
      request.Seed = seed;
      Geometry geometry;
      CHECK(producer.make(request, geometry), "building generation succeeds across hemispheres");
      for (int part = 0; part < geometry.parts(); ++part) {
        const auto positions = geometry.positionsOf(part);
        const auto normals = geometry.normalsOf(part);
        const auto indices = geometry.trianglesOf(part);
        double minY = std::numeric_limits<double>::infinity();
        double maxY = -minY;
        for (size_t at = 0; at < positions.size(); at += 3) {
          // Default footprint is 12 m, with eaves under one metre. Height is 6..18 m.
          CHECK(std::abs(positions[at]) < 7 && std::abs(positions[at + 2]) < 7,
                "horizontal coordinates stay local to the twelve metre footprint");
          minY = std::min(minY, static_cast<double>(positions[at + 1]));
          maxY = std::max(maxY, static_cast<double>(positions[at + 1]));
        }
        CHECK(minY > -1 && minY < 1 && maxY > 6 && maxY < 25,
              "ground and roof use native Y-up metres");
        const auto vertex = [positions](uint32_t index) {
          const size_t at = static_cast<size_t>(index) * 3;
          return Vec3{{positions[at], positions[at + 1], positions[at + 2]}};
        };
        for (size_t at = 0; at < indices.size(); at += 3) {
          const Vec3 a = vertex(indices[at]);
          const Vec3 face = Cross(vertex(indices[at + 1]) - a, vertex(indices[at + 2]) - a);
          const double area = std::sqrt(Dot(face, face));
          CHECK(area > 0, "float conversion preserves nondegenerate mesh triangles");
          if (area == 0) { continue; }
          for (size_t corner = 0; corner < 3; ++corner) {
            const size_t offset = static_cast<size_t>(indices[at + corner]) * 3;
            const Vec3 normal{{normals[offset], normals[offset + 1], normals[offset + 2]}};
            // cos(1 degree) bounds the complete quantization/position error to one degree.
            CHECK(Dot(face, normal) / area > 0.9998476951563913,
                  "all decoded normals agree with native CCW faces within one degree");
          }
        }
      }
    }
  }
  return Report();
}
