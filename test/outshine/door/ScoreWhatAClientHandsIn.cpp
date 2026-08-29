#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <Event.h>
#include <Geometry.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// ONE VALUE, TWO PRODUCERS, AND THE PICTURE CANNOT TELL THEM APART. A glTF reader fills the
// engine's geometry; so, now, does a client with no file anywhere. If the two paths disagree, one
// of them is not the interchange value -- it is a format's private shape wearing the name.
//
// THE ORACLE IS THAT THE TWO PICTURES ARE THE SAME PICTURE, and this case makes that oracle
// airtight by refusing to spell the triangle twice: the arrays below are the ONLY statement of it.
// The glTF is written by base64-encoding those same bytes, and the handed-in geometry points at
// the same arrays. A case that typed the triangle into a JSON string and again into a C++ array
// would prove the two paths agree about two different triangles.
//
// Why it matters that a client can hand geometry in at all: a generator returns a representation
// rather than a file (board:1948), and a representation nobody can hand to the engine is a
// representation with one consumer. This is the door that makes it a value.
constexpr int kFramePx = 96;

constexpr float kPositions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
constexpr float kNormals[9] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
constexpr uint32_t kIndices[3] = {0, 1, 2};

[[nodiscard]] std::string Base64(const void *from, size_t bytes) {
  static const char *kAlphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const unsigned char *at = static_cast<const unsigned char *>(from);
  std::string out;
  for (size_t which = 0; which < bytes; which += 3) {
    const unsigned a = at[which];
    const unsigned b = which + 1 < bytes ? at[which + 1] : 0u;
    const unsigned c = which + 2 < bytes ? at[which + 2] : 0u;
    const unsigned held = (a << 16) | (b << 8) | c;
    out += kAlphabet[(held >> 18) & 63u];
    out += kAlphabet[(held >> 12) & 63u];
    out += which + 1 < bytes ? kAlphabet[(held >> 6) & 63u] : '=';
    out += which + 2 < bytes ? kAlphabet[held & 63u] : '=';
  }
  return out;
}

[[nodiscard]] std::string Document(void) {
  std::vector<unsigned char> buffer(sizeof kPositions + sizeof kNormals);
  std::memcpy(buffer.data(), kPositions, sizeof kPositions);
  std::memcpy(buffer.data() + sizeof kPositions, kNormals, sizeof kNormals);
  return std::string(
             "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
             "\"nodes\":[{\"mesh\":0}],"
             "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
             "\"material\":0}]}],"
             "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
             "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":"
             "\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
             "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
             "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
             "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
             "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,") +
         Base64(buffer.data(), buffer.size()) + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] size_t Differing(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
  if (a.size() != b.size()) { return a.size() + b.size(); }
  size_t apart = 0;
  for (size_t at = 0; at + 3 < a.size(); at += 4) {
    for (int channel = 0; channel < 3; ++channel) {
      if (a[at + channel] != b[at + channel]) { ++apart; break; }
    }
  }
  return apart;
}

[[nodiscard]] double Mean(const std::vector<uint8_t> &rgba) {
  const size_t pixels = rgba.size() / 4;
  if (pixels == 0) { return 0.0; }
  double sum = 0.0;
  for (size_t at = 0; at < pixels; ++at) {
    const int r = rgba[at * 4], g = rgba[at * 4 + 1], b = rgba[at * 4 + 2];
    sum += r > g ? (r > b ? r : b) : (g > b ? g : b);
  }
  return sum / (double)pixels;
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
  if (!Wrote(under + "/handed.gltf", Document())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;
  outshine::Asset shown;
  shown.Uri = "handed.gltf";
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);

  std::vector<uint8_t> fromFile;
  if (!engine.declare(stands) || !engine.advance() ||
      !engine.renderer().render(outshine::Extent{}) || !engine.renderer().readPixels(fromFile)) {
    Unprepared(("the file arm did not stand: " + engine.error()).c_str());
    return Report();
  }

  outshine::Geometry geometry;
  const int part = geometry.addPart("handed", outshine::MaterialInstance(0));
  const bool filled = geometry.setPositions(part, std::span<const float>(kPositions, 9)) &&
                      geometry.setNormals(part, std::span<const float>(kNormals, 9)) &&
                      geometry.setTriangles(part, std::span<const uint32_t>(kIndices, 3));
  if (!filled) {
    Unprepared("the builder refused the fixture, so there is nothing to hand in");
    return Report();
  }

  // A REFUSAL FROM THE THING UNDER TEST IS A FAILURE, NOT UNPREPAREDNESS. The first version of
  // this case reported UNPREPARED when `Stands` refused, so the negative control below came back
  // as a red with the wrong NAME -- it read as a missing fixture rather than as the door being
  // broken. Only the file arm's input is a fixture; everything after it is the claim.
  const bool handed = engine.setGeometry(geometry).has_value();
  std::vector<uint8_t> fromMemory;
  if (handed && (!engine.renderer().render(outshine::Extent{}) || !engine.renderer().readPixels(fromMemory))) {
    Unprepared(("the device would not draw the handed subject: " + engine.error()).c_str());
    return Report();
  }
  if (!handed) { std::printf("STANDS REFUSED  %s\n", engine.error().c_str()); }

  const size_t apart = Differing(fromFile, fromMemory);
  std::printf("FROM FILE    mean max(RGB) %7.3f\n", Mean(fromFile));
  std::printf("FROM MEMORY  mean max(RGB) %7.3f\n", Mean(fromMemory));
  std::printf("PIXELS THAT DIFFER %zu of %zu\n", apart, fromFile.size() / 4);

  CHECK(Mean(fromFile) > 0.0,
        "the file arm drew something, so the comparison below has a picture to be the same as -- "
        "two black frames agree perfectly and prove nothing");
  CHECK(handed,
        "**A CLIENT HANDS GEOMETRY TO THE ENGINE WITH NO FILE ANYWHERE**: `outshine::Geometry` is "
        "a span of parts a caller fills, and it names no format. A generator returns a "
        "representation rather than a file, and a representation nobody can hand in has one "
        "consumer");
  CHECK(apart == 0,
        "**AND THE PICTURE CANNOT TELL THE TWO PRODUCERS APART**: the same nine floats reach the "
        "same frame through the glTF reader and through the door, pixel for pixel. This case "
        "spells the triangle ONCE -- the document is base64 of the same arrays the span points at "
        "-- because two spellings would prove the paths agree about two different triangles");

  Covers("the door: one geometry value with two producers, a glTF reader and a client that hands "
         "it in directly, and a picture that cannot tell them apart");
  return Report();
}
