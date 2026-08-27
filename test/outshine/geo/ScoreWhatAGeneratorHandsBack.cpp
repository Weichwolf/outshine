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
  CHECK((size_t)handed.Parts() == 1 && handed.PositionsOf(0).size() == kVertices * 3 &&
            handed.TextureOf(0).size() == kVertices * 2 &&
            handed.NormalsOf(0).size() == kVertices * 3 &&
            handed.TrianglesOf(0).size() == kVertices,
        "and the counts come out at the right arity: three floats a position, two a UV, three a "
        "normal, one index a vertex");
  if (handed.Parts() == 0) { return Report(); }

  bool everyFieldLanded = true;
  for (size_t vertex = 0; vertex < kVertices; ++vertex) {
    const float which = (float)vertex;
    everyFieldLanded = everyFieldLanded &&
                       handed.PositionsOf(0)[vertex * 3] == 100.0f + which &&
                       handed.PositionsOf(0)[vertex * 3 + 1] == 200.0f + which &&
                       handed.PositionsOf(0)[vertex * 3 + 2] == 300.0f + which &&
                       handed.TextureOf(0)[vertex * 2] == 0.1f * which &&
                       handed.TextureOf(0)[vertex * 2 + 1] == 0.2f * which &&
                       handed.TrianglesOf(0)[vertex] == (uint32_t)vertex;
  }
  std::printf("FIRST POSITION %.1f %.1f %.1f   FIRST UV %.2f %.2f\n",
              handed.PositionsOf(0)[0], handed.PositionsOf(0)[1],
              handed.PositionsOf(0)[2], handed.TextureOf(0)[0], handed.TextureOf(0)[1]);
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
              (size_t)two.Parts() > 1 ? two.TrianglesOf(0).front() : 0u,
              (size_t)two.Parts() > 1 ? two.TrianglesOf(0).back() : 0u,
              (size_t)two.Parts() > 1 ? two.TrianglesOf(1).front() : 0u,
              (size_t)two.Parts() > 1 ? two.TrianglesOf(1).back() : 0u);
  // THIS CHECK ASSERTED THE DEFECT AND HAD TO BE RESPECIFIED, which is the one thing a failing case
  // is ever allowed to do. It required a second part's indices to CONTINUE where the first's ended,
  // on the reasoning that the handed value carries one vertex array and a part is a reach into it.
  //
  // The reasoning was half right and the half it got wrong is fatal. The value did carry one array
  // -- but each part also handed out its OWN positions, a sub-span starting at that part's first
  // vertex. So part 1 offered `kVertices` positions and indices numbered `kVertices .. 2*kVertices`
  // into them: every index out of range, on a span it was the sole describer of. Nothing caught it
  // because nothing validated an index against the span beside it.
  //
  // A part's indices address that part's positions. That is not our convention -- it is glTF's,
  // where `indices` addresses the accessor named by the primitive's own `POSITION`, and there is no
  // reading of the format in which a primitive indexes another primitive's vertices. `Whole()` now
  // refuses any index at or past the part's vertex count, which is the check whose absence let the
  // wrong rule stand long enough to be written down here.
  CHECK((size_t)two.Parts() == 2 && two.TrianglesOf(1).front() == 0u,
        "and a second part's indices address the SECOND part's positions, from zero: a part hands "
        "out its own vertices, so an index that continued from the first part's count would point "
        "past the end of the very span it describes");

  Covers("world: a generator's interleaved soup crosses into the door's geometry builder, field by "
         "field and part by part; a soup that is not whole triangles is refused rather than "
         "truncated, and each part's indices address that part's own vertices as glTF requires");
  return Report();
}
