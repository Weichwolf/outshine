#ifndef SOURCEDIGEST_H
#define SOURCEDIGEST_H

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Sha256.h"

namespace outshine::Test {

struct SourceIdentity {
  std::string Digest;
  long Files = 0;
  long Bytes = 0;

  long long NewestModified = 0;
};

inline void DigestTree(const std::filesystem::path &root, const std::filesystem::path &relativeTo,
                       std::vector<std::string> &lines, long &bytes, long long &newest) {
  std::error_code failed;
  for (std::filesystem::recursive_directory_iterator at(root, failed), end; at != end;
       at.increment(failed)) {
    if (failed) { return; }
    if (!at->is_regular_file(failed) || failed) { continue; }
    const auto written = std::filesystem::last_write_time(at->path(), failed);
    if (!failed) {
      newest = std::max(newest, (long long)std::chrono::duration_cast<std::chrono::seconds>(
                                    written.time_since_epoch())
                                    .count());
    }
    std::ifstream file(at->path(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    bytes += (long)content.size();
    lines.push_back(std::filesystem::relative(at->path(), relativeTo).generic_string() + " " +
                    Sha256Hex(content) + "\n");
  }
}

[[nodiscard]] inline SourceIdentity SourcesUnderTest(void) {
  SourceIdentity out;
  std::vector<std::string> lines;
  DigestTree("src", ".", lines, out.Bytes, out.NewestModified);
  DigestTree("test/frame", ".", lines, out.Bytes, out.NewestModified);
  std::sort(lines.begin(), lines.end());
  std::string material;
  for (const std::string &line : lines) { material += line; }
  out.Files = (long)lines.size();
  out.Digest = Sha256Hex(material);
  return out;
}

}
#endif
