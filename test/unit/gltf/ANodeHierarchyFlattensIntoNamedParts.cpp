/* WHAT SURVIVES THE FLATTENING. A subject is one run of triangles by the time anything draws it, and
 * the hierarchy that produced it is gone -- except for `Parts()`, which says which vertices came from
 * which node and what that node was called.
 *
 * IT EXISTS BECAUSE A DECLARATION SAYS THINGS PER NODE. What each body of a subject emits is one
 * colour per node (doc/requirements.md I.26.13), and a colour resolved by POSITION in a list is a
 * second thing to keep in step on both sides of a comparison; resolved by NAME it is a fact about the
 * file. This test pins the three properties a name-keyed declaration stands on: one part per
 * mesh-bearing node, the file's own names, and a partition of the vertices that leaves none out and
 * counts none twice.
 *
 * THE SUBJECT IS GENERATED, NOT TRACKED (I.26.10: a case directory's only tracked file is its
 * manifest), so on a fresh clone it is absent and that is a statement about the tree rather than
 * about the reader. It is RED and it is not a skip. */
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"
#include "Subject.h"

using outshine::Gltf::Document;
using outshine::Gltf::Subject;

namespace {

const char *const kThreeCubes = "test/render/coverage/trs-hierarchy/scene.glb";

bool Present(const char *path) {
  std::ifstream file(path, std::ios::binary);
  return file.good();
}

} // namespace

int main() {
  using namespace outshine::Test;

  if (!Present(kThreeCubes)) {
    Unprepared(kThreeCubes);
    return Report();
  }

  Document document;
  const bool read = document.ReadFile(kThreeCubes);
  CHECK(read, "the three-cube chain reads as a .glb");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

  Subject subject;
  const bool built = subject.Build(document);
  CHECK(built, "the default scene flattens");
  if (!built) {
    std::printf("       %s\n", subject.Error().c_str());
    return Report();
  }

  CHECK(subject.Parts().size() == 3,
        "one part per mesh-bearing node of the chain, three levels deep");
  Note("parts", (double)subject.Parts().size(), "nodes");

  /* THE NAMES ARE THE FILE'S. A generated name here would make the whole keying circular. */
  const char *const wanted[3] = {"level0", "level1", "level2"};
  size_t named = 0;
  for (size_t part = 0; part < subject.Parts().size() && part < 3; ++part) {
    if (subject.Parts()[part].NodeName == wanted[part]) { ++named; }
  }
  CHECK(named == 3, "each part carries the glTF node's own name, in the order the walk visited them");

  /* A PARTITION AND NOT A LIST OF RANGES: consecutive, covering, disjoint. A part that overlapped
   * its neighbour would give one vertex two declared colours and the last writer would win. */
  size_t next = 0;
  bool consecutive = true;
  for (const outshine::Gltf::Part &part : subject.Parts()) {
    if (part.FirstVertex != next || part.VertexCount == 0) { consecutive = false; }
    next = part.FirstVertex + part.VertexCount;
  }
  CHECK(consecutive && next == subject.VertexCount(),
        "the parts partition the vertices: consecutive, none empty, and together every vertex of "
        "the subject exactly once");
  Note("vertices covered by parts", (double)next, "vertices");
  Note("vertices in the subject", (double)subject.VertexCount(), "vertices");

  Covers("I.26.13 a subject's mesh-bearing nodes survive the flattening as named parts, which is "
         "what a per-node declaration resolves against");
  return Report();
}
