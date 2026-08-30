#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"
#include "Document.h"

namespace {

// The oracle is glTF 2.0's own schema, which gives every one of its top-level arrays
// `"minItems": 1`. An array that is PRESENT says something stands in it; an empty one is a
// statement that contradicts itself, and Khronos's validator reports EMPTY_ENTITY for exactly
// this across eighteen of its own fixtures.
//
// This is not a taste. A reader that accepts `"meshes": []` has accepted a file claiming to
// carry meshes and carrying none, and every later question about mesh 0 is asked of a promise
// that was already broken.
//
// The case walks the arrays the schema names, one document per array, and each must be refused
// with the array's own name in the reason.
constexpr const char *kNamed[] = {"accessors",
                                  "animations",
                                  "buffers",
                                  "bufferViews",
                                  "cameras",
                                  "images",
                                  "materials",
                                  "meshes",
                                  "nodes",
                                  "samplers",
                                  "scenes",
                                  "skins",
                                  "textures"};

[[nodiscard]] std::string Holding(const char *emptied) {
  std::string held = "{\"asset\":{\"version\":\"2.0\"},\"";
  held += emptied;
  held += "\":[]}";
  return held;
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its documents into the runner's nest and was given none");
    return Report();
  }
  const std::string path = std::string(nest) + "/emptied.gltf";

  size_t refused = 0, named = 0;
  for (const char *emptied : kNamed) {
    if (!Wrote(path, Holding(emptied))) {
      Unprepared("a document could not be written into the nest");
      return Report();
    }
    outshine::Gltf::Document document;
    const bool stood = document.ReadFile(path);
    if (!stood) {
      ++refused;
      if (document.Error().find(emptied) != std::string::npos) { ++named; }
    }
    std::printf("%-12s empty: %s\n", emptied, stood ? "STOOD" : "refused");
  }

  Note("arrays the schema gives a minimum of one",
       (double)(sizeof kNamed / sizeof kNamed[0]),
       "arrays");
  Note("of them, refused when emptied", (double)refused, "arrays");
  Note("and refused by NAME", (double)named, "arrays");

  CHECK(refused == sizeof kNamed / sizeof kNamed[0],
        "**AN ARRAY THAT IS PRESENT SAYS SOMETHING STANDS IN IT**: glTF 2.0 gives every one of "
        "its top-level arrays a minimum of one item, and a reader that accepts `\"meshes\": []` "
        "has accepted a file claiming to carry meshes and carrying none -- every later question "
        "about mesh 0 is then asked of a promise already broken");
  CHECK(named == refused,
        "and each refusal carries the array's own name, so a caller is told which promise broke "
        "rather than that one did");

  // The control: a document with the same arrays PRESENT and holding one item each is read, so
  // the rule is minItems and not a reader that refuses arrays.
  const std::string stands =
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{}]}";
  if (!Wrote(path, stands)) {
    Unprepared("the standing document could not be written");
    return Report();
  }
  outshine::Gltf::Document whole;
  const bool read = whole.ReadFile(path);
  std::printf("THE SAME ARRAYS HOLDING ONE ITEM EACH: %s%s\n",
              read ? "STOOD" : "refused -- ",
              read ? "" : whole.Error().c_str());
  CHECK(read,
        "and the control is a control: the same arrays holding one item each are READ, so what "
        "this refuses is emptiness and not the array");

  Covers("gltf-2.0: an array the schema gives a minimum of one item is refused when it is "
         "present and empty, and the refusal names which one");
  return Report();
}
