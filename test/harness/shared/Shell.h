#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Test {

[[nodiscard]] inline std::string Ask(const std::string &command, int *verdict = nullptr) {
  std::string said;
  std::FILE *const pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    if (verdict != nullptr) { *verdict = -1; }
    return said;
  }
  char block[4096];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  const int closed = pclose(pipe);
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
