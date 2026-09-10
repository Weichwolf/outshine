#include "ReadTextFile.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <expected>
#include <string>
#include <string_view>

namespace outshine {
namespace Says {
constexpr std::string_view InvalidPath = ": file path contains NUL";
constexpr std::string_view OpenFailed = ": file could not be opened";
constexpr std::string_view ReadFailed = ": file read failed";
constexpr std::string_view TooLarge = ": file exceeds byte budget";
constexpr std::string_view CloseFailed = ": file close failed";
}

std::expected<std::string, std::string> ReadTextFile(std::string_view path, size_t byteLimit) {
  const auto error = [path](std::string_view reason) {
    return std::unexpected(std::string(path) + std::string(reason));
  };
  if (path.contains('\0')) { return error(Says::InvalidPath); }
  const std::string ownedPath(path);
  std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(ownedPath.c_str(), "rb"),
                                                          &std::fclose);
  if (!file) { return error(Says::OpenFailed); }
  std::string text;
  std::array<char, 4096> block{};
  for (;;) {
    const size_t remaining = byteLimit - text.size();
    if (remaining == 0) {
      const int next = std::fgetc(file.get());
      if (std::ferror(file.get()) != 0) { return error(Says::ReadFailed); }
      if (next != EOF) { return error(Says::TooLarge); }
      break;
    }
    const size_t wanted = std::min(remaining, block.size());
    const size_t read = std::fread(block.data(), 1, wanted, file.get());
    if (std::ferror(file.get()) != 0) { return error(Says::ReadFailed); }
    text.append(block.data(), read);
    if (std::feof(file.get()) != 0) { break; }
    if (read == 0) { return error(Says::ReadFailed); }
  }
  if (std::fclose(file.release()) != 0) { return error(Says::CloseFailed); }
  return text;
}
}
