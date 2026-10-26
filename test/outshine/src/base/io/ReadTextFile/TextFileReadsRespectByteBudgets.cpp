#include "io/ReadTextFile.h"
#include "Check.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unistd.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto path = (std::filesystem::temp_directory_path() / "outshine-read-XXXXXX").string();
  const int descriptor = mkstemp(path.data());
  if (descriptor < 0) {
    CHECK(false, "temporary file created");
    return Report();
  }
  close(descriptor);
  for (const size_t size : {size_t{0}, size_t{1}, size_t{4095}, size_t{4096}, size_t{4097}}) {
    std::string content(size, 'x');
    if (size > 1) { content[1] = '\0'; }
    {
      std::ofstream output(path, std::ios::binary);
      output.write(content.data(), content.size());
    }
    const auto exact = ReadTextFile(path, size);
    CHECK(exact && *exact == content, "exact budget preserves every byte, including embedded NUL");
    const auto unlimited = ReadTextFile(path, std::numeric_limits<size_t>::max());
    CHECK(unlimited && *unlimited == content,
          "large limit does not overflow or allocate the limit");
    if (size > 0) {
      const auto shortBudget = ReadTextFile(path, size - 1);
      CHECK(!shortBudget && shortBudget.error().ends_with("file exceeds byte budget"),
            "one byte beyond budget is rejected without a partial result");
    }
  }
  CHECK(!ReadTextFile(path + std::string("\0suffix", 7), 8192), "NUL path is rejected");
  CHECK(!ReadTextFile(std::filesystem::temp_directory_path().string(), 8192),
        "directory read never succeeds as an empty file");
  std::filesystem::remove(path);
  CHECK(!ReadTextFile(path, 8192), "missing file is an explicit failure");
  return Report();
}
