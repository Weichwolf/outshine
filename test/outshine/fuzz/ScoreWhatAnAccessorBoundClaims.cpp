#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"
#include "Document.h"

namespace {

// glTF 2.0, accessor.min / accessor.max: "Minimum value of each component in this attribute" --
// the ACTUAL componentwise extremes of the data the accessor describes, not a box around them
// and not a box inside them. Khronos's own validator reports four separate errors against that
// one sentence, and the reader here answered none of them: ACCESSOR_MIN_MISMATCH,
// ACCESSOR_MAX_MISMATCH, ACCESSOR_ELEMENT_OUT_OF_MIN_BOUND, ACCESSOR_ELEMENT_OUT_OF_MAX_BOUND.
//
// The oracle is the sentence, and the case builds its own documents against it: one triangle
// whose positions are known exactly, with the declared bounds moved by hand. Truth is what the
// arithmetic says, not what our reader says.
//
//   the data       (0,0,0) (1,0,0) (0,1,0)   so the extremes are 0..1, 0..1, 0..0
//   honest         min [0,0,0]  max [1,1,0]     stands
//   too wide       min [-9,0,0] max [1,1,0]     a box AROUND the data, and the spec forbids it
//   too narrow     min [0,0,0]  max [0.5,1,0]   a box INSIDE it, and an element falls outside
constexpr const char *kTriangleBase64 = "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA";

[[nodiscard]] std::string Declaring(const char *least, const char *most) {
  return std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
                     "\"nodes\":[{\"mesh\":0}],"
                     "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
                     "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                     "\"type\":\"VEC3\",\"min\":") +
         least + ",\"max\":" + most +
         "}],"
         "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
         "\"buffers\":[{\"byteLength\":36,\"uri\":\"data:application/octet-stream;base64," +
         kTriangleBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

struct Answer {
  bool Stood = false;
  std::string Why;
};

[[nodiscard]] Answer Read(const std::string &path, const std::string &held) {
  Answer out;
  if (!Wrote(path, held)) { return out; }
  outshine::Gltf::Document document;
  out.Stood = document.ReadFile(path);
  if (!out.Stood) { out.Why = document.Error(); }
  return out;
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
  const std::string path = std::string(nest) + "/bounded.gltf";

  const Answer honest = Read(path, Declaring("[0,0,0]", "[1,1,0]"));
  std::printf("HONEST BOUNDS 0..1: %s%s\n",
              honest.Stood ? "STOOD" : "REFUSED -- ",
              honest.Stood ? "" : honest.Why.c_str());
  CHECK(honest.Stood,
        "an accessor whose declared bounds ARE its data stands -- a check that refused this "
        "would refuse every conformant file in the corpus and prove nothing but its own zeal");

  const Answer wide = Read(path, Declaring("[-9,0,0]", "[1,1,0]"));
  std::printf("A BOX AROUND THE DATA, min x = -9: %s%s\n",
              wide.Stood ? "STOOD" : "REFUSED -- ",
              wide.Stood ? "" : wide.Why.c_str());
  CHECK(!wide.Stood,
        "**AN ACCESSOR'S BOUNDS ARE ITS DATA'S EXTREMES AND NOT A BOX AROUND THEM**: glTF 2.0 "
        "asks for the minimum of each component, and a reader that accepts a wider claim has "
        "accepted a lie it will later use for culling and for framing");
  CHECK(!wide.Stood && !wide.Why.empty(), "and the refusal carries a reason a caller could act on");

  const Answer narrow = Read(path, Declaring("[0,0,0]", "[0.5,1,0]"));
  std::printf("A BOX INSIDE THE DATA, max x = 0.5: %s%s\n",
              narrow.Stood ? "STOOD" : "REFUSED -- ",
              narrow.Stood ? "" : narrow.Why.c_str());
  CHECK(!narrow.Stood,
        "and an element that falls OUTSIDE the declared bounds is refused too -- the same "
        "sentence read from the other side, and the case that a bounding volume actually "
        "breaks on");

  Covers("gltf-2.0: an accessor's declared min and max are the actual componentwise extremes of "
         "its data, refused in both directions -- a box around the data and a box inside it");
  return Report();
}
