#ifndef OUTSHINE_TEST_PREPAREDSUBJECT_H
#define OUTSHINE_TEST_PREPAREDSUBJECT_H

#include <cstdio>
#include <string>
#include <vector>

// board:1786: a case probed ONE file -- the gltf -- and needed six. The textures the gltf names
// are part of the same corpus, were never probed, and a half-placed asset walked past the
// Unprepared gate to die at a CHECK as a red verdict about the ENGINE.
//
// What a case needs is not a list somebody keeps beside the asset; it is the list the ASSET
// ITSELF names. A glTF declares its images as `"uri": "textures/..."`, and every one of them
// has to be readable before the case can say anything about the engine that reads them.
namespace outshine::Test {

[[nodiscard]] inline bool Readable(const std::string &path) {
  std::FILE *const probe = std::fopen(path.c_str(), "rb");
  if (probe == nullptr) { return false; }
  std::fclose(probe);
  return true;
}

// every file the glTF at `gltfPath` needs beside it, taken from its own uri fields -- images
// and buffers alike -- with the gltf itself first. A path that does not read at all yields
// just that path, which is the caller's first missing file.
[[nodiscard]] inline std::vector<std::string> WhatTheSubjectNeeds(const std::string &gltfPath) {
  std::vector<std::string> needed{gltfPath};
  std::FILE *const reading = std::fopen(gltfPath.c_str(), "rb");
  if (reading == nullptr) { return needed; }
  std::string text;
  char block[65536];
  size_t got = 0;
  while ((got = std::fread(block, 1, sizeof block, reading)) > 0) { text.append(block, got); }
  std::fclose(reading);

  const size_t slash = gltfPath.find_last_of('/');
  const std::string beside =
      slash == std::string::npos ? std::string() : gltfPath.substr(0, slash + 1);
  for (size_t at = text.find("\"uri\""); at != std::string::npos;
       at = text.find("\"uri\"", at + 1)) {
    const size_t opens = text.find('"', text.find(':', at) + 1);
    if (opens == std::string::npos) { break; }
    const size_t closes = text.find('"', opens + 1);
    if (closes == std::string::npos) { break; }
    const std::string uri = text.substr(opens + 1, closes - opens - 1);
    // a data: uri carries its own bytes and needs no file beside it
    if (uri.compare(0, 5, "data:") == 0) { continue; }
    needed.push_back(beside + uri);
  }
  return needed;
}

[[nodiscard]] inline std::vector<std::string> WhatIsMissing(const std::string &gltfPath) {
  std::vector<std::string> missing;
  for (const std::string &one : WhatTheSubjectNeeds(gltfPath)) {
    if (!Readable(one)) { missing.push_back(one); }
  }
  return missing;
}

} // namespace outshine::Test
#endif
