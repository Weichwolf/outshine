#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"

using outshine::Gltf::Document;

namespace {

// A minimal triangle whose buffer is a data: URI. board:1396: this is the ordinary shape of a
// small asset -- a .gltf that carries its own bytes rather than naming a file beside it.
constexpr const char *kEmbedded = R"({
  "asset":{"version":"2.0"},
  "scene":0,
  "scenes":[{"nodes":[0]}],
  "nodes":[{"mesh":0}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
  "accessors":[
    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
     "min":[0.0,0.0,0.0],"max":[1.0,1.0,0.0]},
    {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
  ],
  "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":36},
    {"buffer":0,"byteOffset":36,"byteLength":6}
  ],
  "buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"}],
  "images":[{"mimeType":"image/png","uri":"data:image/png;base64,iVBORw0KGgo="}]
})";

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Document document;
  const std::string text = kEmbedded;
  const bool read = document.Read({(const uint8_t *)text.data(), text.size()}, "embedded.gltf");
  if (!read) { std::printf("REFUSED %s\n", document.Error().c_str()); }
  CHECK(read, "a .gltf whose buffer is a data: URI reads");
  if (!read) { return Report(); }

  Note("buffer views the document holds", (double)document.BufferViews().size(), "views");
  Note("images it holds", (double)document.Images().size(), "images");

  CHECK(document.BufferViews().size() == 3,
        "**AN EMBEDDED IMAGE BECOMES A BUFFER VIEW AT LOAD**: the two the file declares plus one "
        "the reader made for the image's own bytes, so nothing downstream sees a URI and the "
        "decode cannot reach the frame path (board:1396)");
  CHECK(!document.Images().empty() && document.Images().front().View >= 0 &&
            document.Images().front().Uri.empty(),
        "and the image names that view rather than the data: URI it arrived as");

  const size_t made = (size_t)document.Images().front().View;
  CHECK(made < document.BufferViews().size() &&
            document.BufferViews()[made].ByteLength == 8,
        "**AND THE BYTES ARE THE BYTES**: 'iVBORw0KGgo=' is the eight-byte PNG signature, "
        "decoded whole rather than resized to fit");

  CHECK(document.Meshes().size() == 1 && !document.Meshes().front().Primitives.empty(),
        "and the geometry the embedded buffer carries reads as a mesh");

  Covers("I.2.9 a data: URI is base64 and the reader decodes it, for buffers and for images, "
         "bounded at load: a declared length that disagrees with its payload is a refusal and "
         "an image becomes a buffer view rather than a URI anything downstream must handle "
         "(board:1396)");
  return Report();
}
