#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"
#include "Document.h"

namespace {

// The oracle is the GLB container's own definition, and it leaves no room: a glTF binary file
// carries EXACTLY ONE JSON chunk and it comes FIRST, followed by AT MOST ONE binary chunk. The
// reason is not ceremony. A reader meeting a second JSON chunk has two structures and no way to
// say which describes the file; a buffer view naming chunk 0 in a file with two binary chunks
// names both. Khronos reports GLB_DUPLICATE_CHUNK against its own fixtures for it.
//
// Silently taking the first and ignoring the rest -- which is what this reader did -- answers a
// question the file did not ask, and answers it differently from any other reader that picks the
// last.
constexpr uint32_t kGlbMagic = 0x46546C67;
constexpr uint32_t kChunkJson = 0x4E4F534A;
constexpr uint32_t kChunkBinary = 0x004E4942;

void Word(std::vector<uint8_t> &into, uint32_t value) {
  for (int at = 0; at < 4; ++at) { into.push_back((uint8_t)((value >> (8 * at)) & 0xFFu)); }
}

void Chunk(std::vector<uint8_t> &into, uint32_t kind, const std::string &body) {
  std::string padded = body;
  while (padded.size() % 4 != 0) { padded.push_back(kind == kChunkJson ? ' ' : '\0'); }
  Word(into, (uint32_t)padded.size());
  Word(into, kind);
  into.insert(into.end(), padded.begin(), padded.end());
}

[[nodiscard]] std::vector<uint8_t> Container(bool twiceJson, bool twiceBinary, bool jsonFirst) {
  const std::string structure = "{\"asset\":{\"version\":\"2.0\"}}";
  std::vector<uint8_t> held;
  Word(held, kGlbMagic);
  Word(held, 2);
  Word(held, 0);
  if (!jsonFirst) { Chunk(held, kChunkBinary, std::string(4, '\0')); }
  Chunk(held, kChunkJson, structure);
  if (twiceJson) { Chunk(held, kChunkJson, structure); }
  Chunk(held, kChunkBinary, std::string(4, '\0'));
  if (twiceBinary) { Chunk(held, kChunkBinary, std::string(4, '\0')); }
  const uint32_t whole = (uint32_t)held.size();
  for (int at = 0; at < 4; ++at) { held[8 + (size_t)at] = (uint8_t)((whole >> (8 * at)) & 0xFFu); }
  return held;
}

[[nodiscard]] bool Wrote(const std::string &path, const std::vector<uint8_t> &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

struct Shape {
  const char *What;
  bool TwiceJson, TwiceBinary, JsonFirst, Stands;
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its containers into the runner's nest and was given none");
    return Report();
  }
  const std::string path = std::string(nest) + "/carried.glb";

  const Shape asked[] = {
      {"one JSON chunk first, then one binary", false, false, true, true},
      {"a second JSON chunk", true, false, true, false},
      {"a second binary chunk", false, true, true, false},
      {"a binary chunk BEFORE the JSON", false, false, false, false},
  };

  size_t agreed = 0;
  for (const Shape &one : asked) {
    if (!Wrote(path, Container(one.TwiceJson, one.TwiceBinary, one.JsonFirst))) {
      Unprepared("a container could not be written into the nest");
      return Report();
    }
    outshine::Gltf::Document document;
    const bool stood = document.ReadFile(path);
    std::printf("%-40s %s%s\n", one.What, stood ? "STOOD" : "refused",
                stood == one.Stands ? "" : "   <- and should not have");
    if (stood == one.Stands) { ++agreed; }
  }

  Note("containers asked about", (double)(sizeof asked / sizeof asked[0]), "files");
  Note("of them, answered as the container defines", (double)agreed, "files");

  CHECK(agreed == sizeof asked / sizeof asked[0],
        "**A GLB CARRIES ONE STRUCTURE AND IT COMES FIRST**: exactly one JSON chunk, ahead of "
        "what it describes, and at most one binary chunk after it. A reader meeting a second "
        "JSON chunk has two structures and no way to say which is the file; a buffer view "
        "naming chunk 0 in a file with two binary chunks names both. Taking the first and "
        "ignoring the rest answers a question the file did not ask, and answers it differently "
        "from a reader that takes the last");

  CHECK(asked[0].Stands,
        "and the control is a control: the conformant container STANDS, so what this refuses is "
        "the duplicate and the disorder rather than the container");

  Covers("gltf-2.0: a GLB carries exactly one JSON chunk, first, and at most one binary chunk, "
         "and a second of either is refused rather than ignored");
  return Report();
}
