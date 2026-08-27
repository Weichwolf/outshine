#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include <Event.h>
#include <Generate.h>
#include <Geometry.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// ONE VALUE, MANY PRODUCERS, MANY CONSUMERS -- that is the sentence this item is built on, and
// three cases proving three producers SEPARATELY do not prove it. They prove three things that
// happen to work. What the sentence claims is that the value is the same value: hand the same
// geometry in as a FILE, as a CLIENT's call and as a GENERATOR's output, and the same picture comes
// back.
//
// THE ORACLE IS PIXEL IDENTITY and it owes nothing to our design. Three routes into one renderer
// under one declaration must agree exactly, because the only thing that differs is who filled the
// value -- and if who filled it can be read off the picture, the value is not one value.
//
// The file arm goes through the reader, which now writes into a `Geometry` and assembles it. The
// client arm calls `Engine::Stands`. The generator arm registers under a kind and a scenario names
// it. Same nine floats, same material, same frame.
constexpr int kFramePx = 64;

constexpr float kPositions[9] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f};
constexpr float kNormals[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
constexpr uint32_t kIndices[3] = {0, 1, 2};

constexpr const char *kTriangleBase64 =
    "AACAvwAAgL8AAAAAAACAPwAAgL8AAAAAAACAvwAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

[[nodiscard]] std::string Minimal(void) {
  return std::string(
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
      "\"material\":0}]}],"
      "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.7,0.3,0.15,1.0],"
      "\"metallicFactor\":0.0,\"roughnessFactor\":0.85}}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
      "\"min\":[-1,-1,0],\"max\":[1,1,0]},"
      "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
      "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
      "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,") +
      kTriangleBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

void Fills(outshine::Geometry &into) {
  outshine::Material row;
  row.BaseColour[0] = 0.7f;
  row.BaseColour[1] = 0.3f;
  row.BaseColour[2] = 0.15f;
  row.Metalness = 0.0f;
  row.Roughness = 0.85f;
  const int named = into.Surface("clay", row);
  const int part = into.Part("face", named);
  (void)into.Positions(part, std::span<const float>(kPositions, 9));
  (void)into.Normals(part, std::span<const float>(kNormals, 9));
  (void)into.Triangles(part, std::span<const uint32_t>(kIndices, 3));
}

class Makes final : public outshine::Generates {
public:
  [[nodiscard]] std::string_view Kind() const override { return "one-face"; }
  [[nodiscard]] bool Make(const outshine::Ask &, outshine::Geometry &into) const override {
    Fills(into);
    return true;
  }
};

[[nodiscard]] size_t Apart(const std::vector<uint8_t> &was, const std::vector<uint8_t> &is) {
  if (was.size() != is.size() || was.empty()) { return 1u; }
  size_t many = 0;
  for (size_t at = 0; at < was.size(); ++at) {
    if (was[at] != is[at]) { ++many; }
  }
  return many;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subject into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/face.gltf", Minimal())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  const Makes maker;

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;

  outshine::Scenario byFile = stands;
  outshine::Asset shown;
  shown.Uri = "face.gltf";
  shown.Kind = "gltf";
  byFile.Assets.push_back(shown);

  outshine::Scenario byMaker = stands;
  outshine::Asset made;
  made.Uri = "one-face";
  made.Kind = "generated";
  byMaker.Assets.push_back(made);

  // EACH ARM GETS ITS OWN ENGINE, which is not tidiness but the comparison's own requirement: three
  // producers filling one value must be compared from the same starting state, and an engine that
  // has already stood something is not that state. The first version reused one engine and the
  // client and generator arms drew NOTHING -- 0 lit pixels against the file's 169 -- because what
  // stood before them was still standing in a way the re-declaration did not clear.
  const auto stoodBy = [&](const outshine::Scenario &declared, const outshine::Geometry *handed,
                           std::vector<uint8_t> &rgba, std::string &why) {
    outshine::Engine one;
    one.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
    if (!one.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
      why = "the device stood no canvas";
      return false;
    }
    one.Offers(maker);
    if (!one.Declare(declared)) {
      why = one.Error();
      return false;
    }
    if (handed != nullptr && !one.Stands(*handed)) {
      why = one.Error();
      return false;
    }
    if (!one.RenderTo(outshine::Extent{}) || !one.Pixels(rgba)) {
      why = one.Error();
      return false;
    }
    return true;
  };

  const auto meanOf = [](const std::vector<uint8_t> &rgba) {
    const size_t pixels = rgba.size() / 4;
    double sum = 0.0;
    for (size_t at = 0; at < pixels; ++at) { sum += rgba[at * 4]; }
    return pixels == 0 ? 0.0 : sum / (double)pixels;
  };
  const auto litOf = [](const std::vector<uint8_t> &rgba) {
    const size_t pixels = rgba.size() / 4;
    size_t many = 0;
    for (size_t at = 0; at < pixels; ++at) {
      if (rgba[at * 4] > 8 || rgba[at * 4 + 1] > 8 || rgba[at * 4 + 2] > 8) { ++many; }
    }
    return many;
  };
  std::vector<uint8_t> fromFile, fromClient, fromMaker;
  outshine::Geometry byHand;
  Fills(byHand);
  std::string why;
  if (!stoodBy(byFile, nullptr, fromFile, why)) {
    Unprepared(("the file arm did not stand: " + why).c_str());
    return Report();
  }
  if (!stoodBy(stands, &byHand, fromClient, why)) {
    Unprepared(("the client arm did not stand: " + why).c_str());
    return Report();
  }
  if (!stoodBy(byMaker, nullptr, fromMaker, why)) {
    Unprepared(("the generator arm did not stand: " + why).c_str());
    return Report();
  }

  std::printf("A FILE, A CLIENT AND A GENERATOR each stood the same face\n");
  std::printf("MEAN RED  file %6.3f  client %6.3f  maker %6.3f\n", meanOf(fromFile),
              meanOf(fromClient), meanOf(fromMaker));
  std::printf("LIT PIXELS file %5zu  client %5zu  maker %5zu\n", litOf(fromFile),
              litOf(fromClient), litOf(fromMaker));
  std::printf("FILE against CLIENT     %zu subpixel(s) apart of %zu\n",
              Apart(fromFile, fromClient), fromFile.size());
  std::printf("FILE against GENERATOR  %zu subpixel(s) apart of %zu\n",
              Apart(fromFile, fromMaker), fromFile.size());

  CHECK(!fromFile.empty() && fromFile.size() == fromClient.size() &&
            fromFile.size() == fromMaker.size(),
        "all three arms drew a frame of the same size, so the comparisons below are between "
        "pictures rather than between one picture and an absence");
  CHECK(Apart(fromFile, fromClient) == 0,
        "**A FILE AND A CLIENT FILL THE SAME VALUE**: the reader writes into a `Geometry` and "
        "assembles it, and `Engine::Stands` assembles the one a client filled -- so the only "
        "thing that differs is who filled it. If that could be read off the picture, it would not "
        "be one value");
  CHECK(Apart(fromFile, fromMaker) == 0,
        "and so does a GENERATOR, named by a scenario's asset rather than handed in by a call. "
        "Three producers, one value, one picture -- which is the sentence this item is built on, "
        "and three cases proving three producers separately never said it");

  Covers("the door: a file, a client and a generator fill the same geometry value and the frame "
         "cannot tell which of them did -- one value, three producers, proven by pixel identity "
         "rather than by three separate cases that happen to work");
  return Report();
}
