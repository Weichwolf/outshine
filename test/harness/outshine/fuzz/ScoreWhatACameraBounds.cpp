#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"
#include "Document.h"

namespace {

// The oracle is what a view frustum IS, and it owes nothing to our design. A perspective camera
// bounds a volume between two planes: the near one at znear and the far one at zfar. For that
// volume to exist,
//
//   znear > 0        a near plane at or behind the eye bounds nothing
//   zfar  > znear    a far plane not beyond the near one encloses no volume
//
// and for an orthographic camera the same is true of its magnification: xmag and ymag scale the
// picture, and a zero collapses it to a line. glTF 2.0 states all three -- "znear" is
// exclusiveMinimum 0, "zfar" is exclusiveMinimum 0 and must be greater than znear, xmag and ymag
// may not be zero -- and Khronos reports CAMERA_ZFAR_LEQUAL_ZNEAR and CAMERA_XMAG_YMAG_ZERO
// against its own fixtures for them.
//
// A reader that accepts any of the three has accepted a camera that cannot see, and the first
// question it is asked -- what does this frame? -- has no answer.
struct Lens {
  const char *What;
  const char *Json;
  bool Stands;
};

[[nodiscard]] std::string Holding(const char *lens) {
  return std::string("{\"asset\":{\"version\":\"2.0\"},\"cameras\":[") + lens + "]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its documents into the runner's nest and was given none");
    return Report();
  }
  const std::string path = std::string(nest) + "/lensed.gltf";

  const Lens asked[] = {
      {"a frustum between 0.1 and 100",
       "{\"type\":\"perspective\",\"perspective\":{\"yfov\":1.0,\"znear\":0.1,\"zfar\":100.0}}",
       true},
      {"a perspective camera with no far plane at all",
       "{\"type\":\"perspective\",\"perspective\":{\"yfov\":1.0,\"znear\":0.1}}", true},
      {"a near plane AT the eye",
       "{\"type\":\"perspective\",\"perspective\":{\"yfov\":1.0,\"znear\":0.0,\"zfar\":100.0}}",
       false},
      {"a near plane BEHIND the eye",
       "{\"type\":\"perspective\",\"perspective\":{\"yfov\":1.0,\"znear\":-1.0,\"zfar\":100.0}}",
       false},
      {"a far plane at the near one",
       "{\"type\":\"perspective\",\"perspective\":{\"yfov\":1.0,\"znear\":5.0,\"zfar\":5.0}}",
       false},
      {"a far plane in FRONT of the near one",
       "{\"type\":\"perspective\",\"perspective\":{\"yfov\":1.0,\"znear\":5.0,\"zfar\":1.0}}",
       false},
      {"an orthographic camera that magnifies",
       "{\"type\":\"orthographic\",\"orthographic\":{\"xmag\":2.0,\"ymag\":2.0,\"znear\":0.1,"
       "\"zfar\":100.0}}",
       true},
      {"an orthographic camera magnifying by zero",
       "{\"type\":\"orthographic\",\"orthographic\":{\"xmag\":0.0,\"ymag\":2.0,\"znear\":0.1,"
       "\"zfar\":100.0}}",
       false},
  };

  size_t agreed = 0;
  for (const Lens &one : asked) {
    if (!Wrote(path, Holding(one.Json))) {
      Unprepared("a document could not be written into the nest");
      return Report();
    }
    outshine::Gltf::Document document;
    const bool stood = document.ReadFile(path);
    std::printf("%-44s %s%s\n", one.What, stood ? "STOOD" : "refused",
                stood == one.Stands ? "" : "   <- and should not have");
    if (stood == one.Stands) { ++agreed; }
  }

  Note("lenses asked about", (double)(sizeof asked / sizeof asked[0]), "cameras");
  Note("of them, answered as the frustum demands", (double)agreed, "cameras");

  CHECK(agreed == sizeof asked / sizeof asked[0],
        "**A CAMERA BOUNDS A VOLUME OR IT SEES NOTHING**: znear must lie in front of the eye and "
        "zfar beyond znear, or the two planes enclose nothing; an orthographic magnification of "
        "zero collapses the picture to a line. A reader that accepts any of the three has "
        "accepted a camera whose first question -- what does this frame? -- has no answer");

  CHECK(asked[0].Stands && asked[1].Stands && asked[6].Stands,
        "and the control is a control: three of the eight are CONFORMANT, including a "
        "perspective camera that declares no far plane at all, which glTF 2.0 permits and an "
        "over-eager reader would refuse");

  Covers("gltf-2.0: a camera's near plane lies in front of the eye and its far plane beyond the "
         "near one, an orthographic magnification is not zero, and a perspective camera may "
         "still decline to name a far plane");
  return Report();
}
