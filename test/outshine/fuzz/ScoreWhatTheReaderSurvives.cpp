#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"
#include "Document.h"
#include "Subject.h"

namespace {

// This case is INPUT grade and claims nothing more: nothing is supplied, so all it can prove is
// that the reader SURVIVES. It never says a document was read correctly -- only that a corrupt
// one did not take the process with it.
//
// It is DETERMINISTIC on purpose. A random fuzzer belongs outside a gate that must not go silent
// (board:1869): it finds a different thing every run, so a green run means nothing and a red one
// cannot be repeated. This walks a fixed schedule instead -- every mutation is a pure function of
// (seed byte, position, kind) and the same schedule runs on every machine forever. A long random
// soak is a separate job whose OUTPUT is a reduced case committed here, not a green tick.
//
// The seeds are the smallest possible conformant glTF and deliberate near-misses of it. The
// mutations are the four ways a byte stream goes wrong at a boundary: a byte flips, a byte is
// dropped, the stream is cut short, and a length is made to lie.
constexpr size_t kPositions = 64;

[[nodiscard]] std::string Conformant(void) {
  return "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
         "\"nodes\":[{\"mesh\":0}],"
         "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
         "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
         "\"min\":[0,0,0],\"max\":[1,1,0]}],"
         "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
         "\"buffers\":[{\"byteLength\":36,\"uri\":\"data:application/octet-stream;base64,"
         "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\"}]}";
}

[[nodiscard]] std::vector<std::string> Seeds(void) {
  return {
      Conformant(),
      "{\"asset\":{\"version\":\"2.0\"}}",
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[]}",
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":7,\"scenes\":[{\"nodes\":[]}]}",
      "{\"asset\":{\"version\":\"2.0\"},\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":9}]}",
      "{}",
      "",
  };
}

[[nodiscard]] std::string Mutated(const std::string &seed, size_t position, int kind) {
  std::string held = seed;
  if (held.empty()) { return held; }
  const size_t at = position % held.size();
  switch (kind) {
    case 0: held[at] = (char)(held[at] ^ 0x20); break;
    case 1: held.erase(at, 1); break;
    case 2: held.resize(at); break;
    default: held[at] = (char)('0' + (char)(position % 10)); break;
  }
  return held;
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = held.empty() || std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its mutants into the runner's nest and was given none");
    return Report();
  }
  const std::string path = std::string(nest) + "/mutant.gltf";

  const std::vector<std::string> seeds = Seeds();
  size_t tried = 0, stood = 0, refused = 0;
  size_t worstBytes = 0;

  for (size_t which = 0; which < seeds.size(); ++which) {
    for (size_t position = 0; position < kPositions; ++position) {
      for (int kind = 0; kind < 4; ++kind) {
        const std::string mutant = Mutated(seeds[which], position, kind);
        if (!Wrote(path, mutant)) {
          Unprepared("a mutant could not be written into the nest");
          return Report();
        }
        ++tried;
        if (mutant.size() > worstBytes) { worstBytes = mutant.size(); }

        outshine::Gltf::Document document;
        if (!document.ReadFile(path)) {
          ++refused;
          continue;
        }
        outshine::Gltf::Subject subject;
        if (subject.Build(document)) {
          ++stood;
        } else {
          ++refused;
        }
      }
    }
  }

  Note("mutants read", (double)tried, "documents");
  Note("of them, stood", (double)stood, "documents");
  Note("of them, refused with a reason", (double)refused, "documents");
  Note("the largest mutant", (double)worstBytes, "bytes");

  CHECK(tried == seeds.size() * kPositions * 4,
        "the whole schedule ran -- a case that stopped early proves only what it reached");
  CHECK(stood + refused == tried,
        "**EVERY MUTANT EITHER STANDS OR IS REFUSED, AND NOTHING ELSE**: reaching this line at "
        "all is the proof, because the failure this case exists to catch does not return a "
        "value -- it takes the process. A reader at a system boundary is fed by content the "
        "engine does not control, and a document that is wrong is a document that arrives");
  CHECK(refused > 0,
        "and the control is a control: the schedule DOES produce documents the reader refuses, "
        "so a reader that accepted everything would not pass here by silence");
  CHECK(stood > 0,
        "and it produces documents that still stand, so the mutations are not all fatal and the "
        "walk reaches the code behind the parse");

  Covers("INPUT grade: the glTF reader survives a fixed schedule of corrupted documents -- every "
         "one either stands or is refused with a reason, and none of them takes the process");
  return Report();
}
