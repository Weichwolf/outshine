#ifndef OUTSHINE_TEST_CANONICALGROUNDBODYEXPORT_H
#define OUTSHINE_TEST_CANONICALGROUNDBODYEXPORT_H
#include "Check.h"
#include "GroundPortBodies.h"
#include <cstddef>
#include <cstdio>
#include <format>
#include <print>
#include <string_view>

namespace outshine::Test::CanonicalGroundFixture {
template <class Range, class BodyOf>
void WriteBodies(
    const Range &bodies, BodyOf bodyOf, const char *prefix, size_t which, std::string_view phase) {
  if (prefix == nullptr) { return; }
  const auto path = std::format("{}-{}-{}.json", prefix, which, phase);
  auto *stream = std::fopen(path.c_str(), "wb");
  CHECK(stream != nullptr, "explicit actual-body diagnostics have a writable temp destination");
  if (stream == nullptr) { return; }
  std::fputs(R"({"bodies":[)", stream);
  bool first = true;
  for (const auto &value : bodies) {
    if (!first) { std::fputs(",", stream); }
    first = false;
    const auto &body = bodyOf(value);
    std::print(stream,
               R"({{"source":"{}/{}","begin":{:.17g},"end":{:.17g},"points":[)",
               body.Source.PathId,
               body.Source.SegmentOrdinal,
               body.SourceBegin,
               body.SourceEnd);
    const auto &mesh = body.Geometry;
    for (size_t at = 0; at < mesh.Points.size(); ++at) {
      const auto &point = mesh.Points[at];
      std::print(
          stream, "{}[{:.17g},{:.17g},{:.17g}]", at == 0 ? "" : ",", point[0], point[1], point[2]);
    }
    std::fputs(R"(],"faces":[)", stream);
    for (size_t at = 0; at < mesh.Faces.size(); ++at) {
      const auto face = mesh.Faces[at];
      std::print(stream, "{}[{},{},{}]", at == 0 ? "" : ",", face[0], face[1], face[2]);
    }
    std::print(stream, R"(],"surface_faces":{},"walls":[)", mesh.SurfaceFaces);
    for (size_t at = 0; at < mesh.WallKinds.size(); ++at) {
      std::print(stream, "{}{}", at == 0 ? "" : ",", static_cast<unsigned>(mesh.WallKinds[at]));
    }
    std::fputs("]}", stream);
  }
  std::fputs("]}\n", stream);
  const bool valid = std::ferror(stream) == 0;
  const int closed = std::fclose(stream);
  CHECK(valid && closed == 0, "the actual complete body diagnostic writes and closes successfully");
}
}
#endif
