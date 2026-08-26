#include <cmath>
#include <cstdio>
#include <vector>

#include "Check.h"
#include "Meshed.h"

namespace {

// A GENERATOR'S MESH IS THE INTERCHANGE VALUE, OR IT HAS ONE CONSUMER. `BuildingMesh::Mesh` fills
// a `std::vector<float>` soup at eight floats a vertex -- three position, two UV, three normal,
// the same order `ChunkVtx` uses -- and `TreeMesh` fills bark and leaf arrays with their own
// indices. Neither is `outshine::Geometry`, and `DrawSink`, the interface that was to carry them,
// is implemented by NOBODY: `grep -rln 'public DrawSink'` over `src/` returns nothing and
// `ClusterId` appears in two files. The draw half of the generator tier has never run.
//
// `Meshed` is the crossing. It de-interleaves a soup into the value the door takes, and this case
// is the oracle for it: what goes in comes out, in order, in the right array.
//
// THE ORACLE IS A SOUP WHOSE EVERY FIELD IS DISTINGUISHABLE. Position, UV and normal carry
// different magnitudes on purpose -- 100s, 0.x and unit -- so a de-interleave that reads the
// stride wrong cannot land on plausible numbers. The tile reader that read a normal for a
// position produced 746 m of relief over 815 m of ground and no exception (board:1512); a
// stride defect is loud only if the values make it so.
constexpr size_t kVertices = 6;
constexpr size_t kFloats = kVertices * outshine::Generators::kSoupFloatsPerVertex;

[[nodiscard]] std::vector<float> Soup() {
  std::vector<float> out;
  out.reserve(kFloats);
  for (size_t vertex = 0; vertex < kVertices; ++vertex) {
    const float which = (float)vertex;
    out.push_back(100.0f + which);
    out.push_back(200.0f + which);
    out.push_back(300.0f + which);
    out.push_back(0.1f * which);
    out.push_back(0.2f * which);
    out.push_back(which == 0.0f ? 1.0f : 0.0f);
    out.push_back(which == 1.0f ? 1.0f : 0.0f);
    out.push_back(which > 1.0f ? 1.0f : 0.0f);
  }
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Generators;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<float> soup = Soup();
  Meshed meshed;
  const bool took = meshed.Take("a wall", 3, soup.data(), soup.size());
  std::printf("TOOK %s   parts %zu\n", took ? "yes" : meshed.Error().c_str(), meshed.Parts());
  CHECK(took && meshed.Parts() == 1,
        "a soup of whole triangles at the declared stride is taken as one part");
  if (!took) { return Report(); }

  const outshine::Geometry handed = meshed.Handed();
  CHECK(handed.Parts.size() == 1 && handed.Parts[0].PositionsM.size() == kVertices * 3 &&
            handed.Parts[0].Uv.size() == kVertices * 2 &&
            handed.Parts[0].Normals.size() == kVertices * 3 &&
            handed.Parts[0].Indices.size() == kVertices,
        "and the counts come out at the right arity: three floats a position, two a UV, three a "
        "normal, one index a vertex");
  if (handed.Parts.empty()) { return Report(); }

  bool everyFieldLanded = true;
  for (size_t vertex = 0; vertex < kVertices; ++vertex) {
    const float which = (float)vertex;
    everyFieldLanded = everyFieldLanded &&
                       handed.Parts[0].PositionsM[vertex * 3] == 100.0f + which &&
                       handed.Parts[0].PositionsM[vertex * 3 + 1] == 200.0f + which &&
                       handed.Parts[0].PositionsM[vertex * 3 + 2] == 300.0f + which &&
                       handed.Parts[0].Uv[vertex * 2] == 0.1f * which &&
                       handed.Parts[0].Uv[vertex * 2 + 1] == 0.2f * which &&
                       handed.Parts[0].Indices[vertex] == (uint32_t)vertex;
  }
  std::printf("FIRST POSITION %.1f %.1f %.1f   FIRST UV %.2f %.2f\n",
              handed.Parts[0].PositionsM[0], handed.Parts[0].PositionsM[1],
              handed.Parts[0].PositionsM[2], handed.Parts[0].Uv[0], handed.Parts[0].Uv[1]);
  CHECK(everyFieldLanded,
        "**EVERY FIELD LANDS IN ITS OWN ARRAY, IN ORDER**: a soup is interleaved and the value is "
        "not, so the crossing is a de-interleave and a stride read one float wide puts a normal "
        "where a position goes. The three fields carry different magnitudes here on purpose -- "
        "hundreds, tenths and unit -- because a stride defect that lands on plausible numbers is "
        "one nobody sees");

  Meshed second;
  const bool tooShort = second.Take("half a vertex", 0, soup.data(), kFloats - 1);
  std::printf("A PARTIAL VERTEX  %s -- %s\n", tooShort ? "TAKEN" : "refused",
              second.Error().c_str());
  CHECK(!tooShort,
        "a soup that is not a whole number of vertices is REFUSED with the count in the reason, "
        "rather than truncated into a mesh that is nearly right");

  Meshed third;
  const bool notTriangles =
      third.Take("two vertices", 0, soup.data(), 2 * kSoupFloatsPerVertex);
  CHECK(!notTriangles,
        "and neither is a whole number of vertices that is not a whole number of triangles -- a "
        "soup carries its own topology and two thirds of a triangle is not one");

  Meshed both;
  (void)both.Take("first", 1, soup.data(), kFloats);
  (void)both.Take("second", 2, soup.data(), kFloats);
  const outshine::Geometry two = both.Handed();
  std::printf("TWO PARTS  indices %u..%u then %u..%u\n",
              two.Parts.size() > 1 ? two.Parts[0].Indices.front() : 0u,
              two.Parts.size() > 1 ? two.Parts[0].Indices.back() : 0u,
              two.Parts.size() > 1 ? two.Parts[1].Indices.front() : 0u,
              two.Parts.size() > 1 ? two.Parts[1].Indices.back() : 0u);
  CHECK(two.Parts.size() == 2 && two.Parts[1].Indices.front() == (uint32_t)kVertices,
        "and a second part's indices continue where the first's ended, because the value carries "
        "ONE vertex array and a part is a reach into it -- a part that indexed from zero would "
        "draw the first part's triangles twice");

  Covers("world: a generator's interleaved soup crosses into the door's geometry value, field by "
         "field and part by part, and a soup that is not whole triangles is refused rather than "
         "truncated");
  return Report();
}
