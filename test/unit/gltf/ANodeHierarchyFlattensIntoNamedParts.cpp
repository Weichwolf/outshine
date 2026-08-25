#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include "Document.h"
#include "Subject.h"

using outshine::Gltf::Document;
using outshine::Gltf::Subject;

namespace {

const std::string kThreeCubes = outshine::Test::PreparedRoot() + "/test-render-outshine-grown-trs-hierarchy/scene.glb";

bool Present(const char *path) {
  std::ifstream file(path, std::ios::binary);
  return file.good();
}

}

int main() {
  using namespace outshine::Test;

  if (!Present(kThreeCubes.c_str())) {
    Unprepared((kThreeCubes + " is not prepared -- run test/harness/shared/corpus/prepare.py").c_str());
    return Report();
  }

  Document document;
  const bool read = document.ReadFile(kThreeCubes.c_str());
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

  const char *const wanted[3] = {"level0", "level1", "level2"};
  size_t named = 0;
  for (size_t part = 0; part < subject.Parts().size() && part < 3; ++part) {
    if (subject.Parts()[part].NodeName == wanted[part]) { ++named; }
  }
  CHECK(named == 3, "each part carries the glTF node's own name, in the order the walk visited them");

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

  Covers("I.70 a subject's mesh-bearing nodes survive the flattening as named parts, which is "
         "what a per-node declaration resolves against");
  return Report();
}
