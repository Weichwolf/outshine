#ifndef OUTSHINE_TEST_SHELL_H
#define OUTSHINE_TEST_SHELL_H

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace outshine::Test {

[[nodiscard]] inline std::string ShellArgument(std::string_view value) {
  std::string quoted = "'";
  for (const char byte : value) {
    if (byte == '\'') {
      quoted += "'\\''";
    } else {
      quoted += byte;
    }
  }
  return quoted + "'";
}

// Test-owned shell scripts retain their environment and raw POSIX child status.
inline int Run(const std::string &command, std::string &said) {
  const std::unique_ptr<std::FILE, decltype(&std::fclose)> output(std::tmpfile(), std::fclose);
  if (!output) { return -1; }
  posix_spawn_file_actions_t actions{};
  if (posix_spawn_file_actions_init(&actions) != 0) { return -1; }
  const int descriptor = fileno(output.get());
  const bool redirected =
      descriptor >= 0 &&
      posix_spawn_file_actions_adddup2(&actions, descriptor, STDOUT_FILENO) == 0 &&
      (descriptor == STDOUT_FILENO || posix_spawn_file_actions_addclose(&actions, descriptor) == 0);
  if (!redirected) {
    posix_spawn_file_actions_destroy(&actions);
    return -1;
  }
  std::string program = "sh";
  std::string option = "-c";
  std::string separator = "--";
  std::string script = command;
  const std::array arguments{
      program.data(), option.data(), separator.data(), script.data(), static_cast<char *>(nullptr)};
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  const int started =
      posix_spawn(&child, "/bin/sh", &actions, nullptr, arguments.data(), environment);
  posix_spawn_file_actions_destroy(&actions);
  if (started != 0) { return -1; }
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) { return -1; }
  }
  if (std::fseek(output.get(), 0, SEEK_SET) != 0) { return -1; }
  constexpr size_t kOutputChunkBytes = 4096;
  std::array<char, kOutputChunkBytes> block{};
  while (std::fgets(block.data(), static_cast<int>(block.size()), output.get()) != nullptr) {
    said += block.data();
  }
  return std::ferror(output.get()) == 0 ? status : -1;
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

#endif
