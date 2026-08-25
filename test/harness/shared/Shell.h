#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Test {

inline int Run(const std::string &command, std::string &said) {
  std::FILE *const pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) { return -1; }
  char block[4096];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  return pclose(pipe);
}

// A command's answer is what it said, not what it said plus the separator that ended it.
// board:1857: dropping this trim turned two walks anchored on `Ask(...).size() == 40` into
// walks over an empty window -- and a guard that stops guarding goes GREEN, so the suite that
// was supposed to control the change could not see it.
[[nodiscard]] inline std::string Ask(const std::string &command, int *verdict = nullptr) {
  std::string said;
  const int closed = Run(command, said);
  if (verdict != nullptr) { *verdict = closed; }
  while (!said.empty() && (said.back() == '\n' || said.back() == ' ')) { said.pop_back(); }
  return said;
}

[[nodiscard]] inline std::vector<std::string> Lines(std::string_view block) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < block.size()) {
    const size_t stop = block.find('\n', at);
    const size_t end = stop == std::string_view::npos ? block.size() : stop;
    if (end > at) { out.emplace_back(block.substr(at, end - at)); }
    if (stop == std::string_view::npos) { break; }
    at = stop + 1;
  }
  return out;
}

} // namespace outshine::Test
