#ifndef OUTSHINE_APP_COMMANDLINE_H
#define OUTSHINE_APP_COMMANDLINE_H

#include <algorithm>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::App {
namespace Says {
inline constexpr std::string_view kNullCommandArgument = "command arguments must not be null";
}

struct CommandLine {
  std::string_view Verb = "help";
  std::span<const char *const> Arguments;
};

[[nodiscard]] inline std::expected<CommandLine, std::string_view>
ReadCommandLine(std::span<const char *const> arguments) noexcept {
  if (std::ranges::any_of(arguments, [](const char *argument) { return argument == nullptr; })) {
    return std::unexpected(Says::kNullCommandArgument);
  }
  CommandLine command;
  if (arguments.size() <= 1) { return command; }
  arguments = arguments.subspan(1);
  command.Verb = arguments.front();
  command.Arguments = arguments.subspan(1);
  return command;
}
}
#endif
