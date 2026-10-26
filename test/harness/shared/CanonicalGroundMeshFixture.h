#ifndef OUTSHINE_TEST_CANONICALGROUNDMESHFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDMESHFIXTURE_H

#include "Check.h"
#include "CppAllocationProbe.h"
#include "RoadGroundMesh.h"
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <format>
#include <optional>
#include <ratio>
#include <string>
#include <utility>

namespace outshine::Test::CanonicalGroundFixture {
inline void WriteMesh(const Generators::RoadGroundMesh &mesh, size_t which, const char *path) {
  if (path == nullptr) { return; }
  std::string text = "{\"points\":[";
  for (size_t at = 0; at < mesh.Surface.Points.size(); ++at) {
    if (at != 0) { text += ','; }
    text += std::format("[{:.17g},{:.17g},{:.17g}]",
                        mesh.Surface.Points[at][0],
                        mesh.Surface.Points[at][1],
                        mesh.HeightsM[at]);
  }
  text += "],\"triangles\":[";
  for (size_t at = 0; at < mesh.Surface.Faces.size(); ++at) {
    if (at != 0) { text += ','; }
    const auto &f = mesh.Surface.Faces[at];
    text += std::format("[{},{},{}]", f[0], f[1], f[2]);
  }
  text += "]}";
  const auto filename = std::format("{}.mesh-{}.json", path, which);
  std::FILE *file = std::fopen(filename.c_str(), "wb");
  CHECK(file != nullptr, "explicit finite mesh diagnostics have a writable destination");
  if (file == nullptr) { return; }
  CHECK(std::fwrite(text.data(), 1, text.size(), file) == text.size(),
        "finite mesh diagnostics are complete");
  CHECK(std::fclose(file) == 0, "finite mesh diagnostics close successfully");
}

inline std::optional<Generators::RoadGroundMesh>
CheckFiniteMesh(const Generators::RoadGroundField &source,
                const Generators::RoadPortField &field,
                size_t which,
                bool reverse,
                const char *path) {
  Generators::RoadGroundMeshBudget budget;
  if (reverse) {
    budget.Vertices *= 2;
    budget.Faces *= 2;
  }
  const auto begin = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  auto mesh = Generators::BuildRoadGroundMesh(source, field, budget);
  const auto allocations = EndCppAllocationProbe();
  Note("actual finite mesh total elapsed",
       std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count(),
       "ms");
  CHECK(mesh.has_value(),
        "both actual components produce a complete independently qualified finite surface");
  if (!mesh) { return std::nullopt; }
  CHECK(mesh->Audit.NumericalQualityMet() && mesh->Work.CoverageErrorM2 <= 1e-6 &&
            mesh->Work.MaximumRootAreaErrorM2 <= 1e-6,
        "finite physics, whole original ports and original-domain coverage hold together");
  Note("actual finite mesh vertices", static_cast<double>(mesh->Surface.Points.size()), "vertices");
  Note("actual finite mesh faces", static_cast<double>(mesh->Surface.Faces.size()), "faces");
  Note("actual finite mesh rounds", static_cast<double>(mesh->Work.Rounds), "rounds");
  Note("actual finite mesh interval visits",
       static_cast<double>(mesh->Work.IntervalVisits),
       "visits");
  Note("actual finite mesh source face visits",
       static_cast<double>(mesh->Work.SourceFaceVisits),
       "visits");
  Note("actual finite mesh edge queries", static_cast<double>(mesh->Work.EdgeQueries), "queries");
  Note("actual finite mesh chord bound", mesh->Audit.MaximumHeightChordErrorM, "m");
  Note("actual finite mesh normalized grade", mesh->Audit.MaximumNormalizedFiniteGrade, "ratio");
  Note("actual finite mesh contact jump", mesh->Audit.MaximumContactGradientJump, "m/m");
  Note("actual finite mesh root area error", mesh->Work.MaximumRootAreaErrorM2, "m2");
  Note("actual finite mesh coverage error", mesh->Work.CoverageErrorM2, "m2");
  Note("actual finite mesh C++ allocations", static_cast<double>(allocations.Calls), "calls");
  Note("actual finite mesh C++ requested bytes", static_cast<double>(allocations.Bytes), "B");
  Note("actual finite mesh slice workspace capacity",
       static_cast<double>(mesh->Work.ContainerCapacityBytes),
       "B");
  if (!reverse) { WriteMesh(*mesh, which, path); }
  return std::move(*mesh);
}
}
#endif
