#include "SpirvFile.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <unistd.h>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-spirv-XXXXXX").string();
  if (mkdtemp(directory.data()) == nullptr) {
    CHECK(false, "temporary shader directory created");
    return Report();
  }
  const std::string path = directory + "/shader";
  const std::array<uint32_t, 5> words{0x07230203, 0x00010000, 0, 1, 0};
  const auto write = [&](std::span<const uint32_t> input, bool trailing = false) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(input.data()), input.size_bytes());
    if (trailing) { file.put('x'); }
  };
  CHECK(!ReadSpirv(path), "missing shader is refused");
  CHECK(!ReadSpirv(directory), "directory is not a shader byte stream");
  write({});
  CHECK(!ReadSpirv(path), "empty shader is refused");
  write(std::span(words).first(4));
  CHECK(!ReadSpirv(path), "incomplete header is refused");
  write(words, true);
  CHECK(!ReadSpirv(path), "incomplete trailing word is refused");
  auto badMagic = words;
  badMagic[0] = 0;
  write(badMagic);
  CHECK(!ReadSpirv(path), "wrong format magic is refused");
  write(words);
  const auto read = ReadSpirv(path);
  CHECK(read && std::ranges::equal(*read, words), "complete framed words retain their exact bits");
  CHECK(!ReadSpirv(path + std::string("\0ignored", 8)), "embedded NUL cannot alias a valid shader");
  std::filesystem::remove_all(directory);
  return Report();
}
