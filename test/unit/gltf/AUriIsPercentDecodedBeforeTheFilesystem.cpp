/* A URI IS NOT A PATH. glTF requires the writer to percent-encode reserved characters
 * (`Specification.adoc:550`), which makes decoding them this reader's obligation and not a
 * convenience -- and the reader handed the string to `std::ifstream` verbatim.
 *
 * KHRONOS SHIPS THE ASSET FOR IT AND ITS SHAPE IS THE TRAP: `Box With Spaces` carries its BUFFER
 * uri with literal spaces and its IMAGE uris percent-encoded, so a reader that concatenates opens
 * the buffer, draws the box, and fails at the first texture -- which reads as a texture bug. Both
 * spellings are exercised below in one document for exactly that reason.
 *
 * The subject is written into the system temp directory rather than committed: a file whose NAME is
 * the thing under test cannot be recomputed from anything, and the name is one line of code. */
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"

using outshine::Gltf::Document;

namespace {

void Write(const std::filesystem::path &path, const std::vector<uint8_t> &bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<long>(bytes.size()));
}

} // namespace

int main() {
  using namespace outshine::Test;

  std::error_code failed;
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path(failed) / "outshine-uri-decoding";
  std::filesystem::remove_all(directory, failed);
  std::filesystem::create_directories(directory, failed);
  CHECK(!failed, "the subject directory can be made");
  if (failed) { return Report(); }

  /* Four float32 zeros, which is what the accessor below reads; the bytes do not matter and the
   * NAME does. */
  const std::vector<uint8_t> buffer(16, 0);
  Write(directory / "Box With Spaces.bin", buffer);
  /* A 1x1 PNG this test does not decode -- what is judged here is whether the file was FOUND. */
  const std::vector<uint8_t> image{0x89, 'P', 'N', 'G', 1, 2, 3, 4};
  Write(directory / "Wall With Spaces.png", image);

  const std::string json = R"({"asset":{"version":"2.0"},
      "buffers":[{"byteLength":16,"uri":"Box With Spaces.bin"}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":16}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"VEC4"}],
      "images":[{"uri":"Wall%20With%20Spaces.png"}],
      "textures":[{"source":0}],
      "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}]})";
  const std::filesystem::path scene = directory / "scene.gltf";
  {
    std::ofstream out(scene);
    out << json;
  }

  Document document;
  const bool read = document.ReadFile(scene.string());
  CHECK(read, "a document whose buffer uri carries literal spaces reads");
  if (!read) {
    Note(("refusal: " + document.Error()).c_str());
    return Report();
  }

  CHECK(document.Images().size() == 1u, "the document carries its one image");
  CHECK(document.Images()[0].Uri == "Wall With Spaces.png",
        "and the image's uri arrived percent-DECODED, so it names a path and not an escape");

  std::vector<uint8_t> bytes;
  const bool found = document.ImageBytes(0, bytes);
  CHECK(found, "the percent-encoded image uri resolves to the file beside the document");
  CHECK(bytes == image, "and hands back exactly the bytes that were written");

  /* THE OTHER HALF, because a decoder that decoded too eagerly would be a different defect: a
   * percent sign that is not an escape is not one. */
  {
    const std::vector<uint8_t> odd{7, 7, 7, 7};
    Write(directory / "100% linen.bin", odd);
    const std::string oddJson = R"({"asset":{"version":"2.0"},
        "buffers":[{"byteLength":4,"uri":"100% linen.bin"}]})";
    const std::filesystem::path oddScene = directory / "odd.gltf";
    {
      std::ofstream out(oddScene);
      out << oddJson;
    }
    Document literal;
    CHECK(literal.ReadFile(oddScene.string()),
          "a per-cent sign that begins no valid escape is left where it stands");
    if (!literal.Error().empty()) { Note(("refusal: " + literal.Error()).c_str()); }
  }

  std::filesystem::remove_all(directory, failed);

  Covers("I.26.12 khronos:Box With Spaces -- every declared uri resolves to its file, percent "
         "escapes decoded before the string reaches the filesystem");
  return Report();
}
