#ifndef OUTSHINE_CLIENT_COMMANDLINE_H
#define OUTSHINE_CLIENT_COMMANDLINE_H

#include <algorithm>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Client {
namespace Says {
inline constexpr std::string_view kNullCommandArgument = "command arguments must not be null";
inline constexpr std::string_view kMissingPlaceCommand =
    "--places requires a directory and command";
}

struct CommandLine {
  std::string_view Directory = "src/assets/places";
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
  if (std::string_view(arguments.front()) == "--places") {
    if (arguments.size() < 3) { return std::unexpected(Says::kMissingPlaceCommand); }
    command.Directory = arguments[1];
    arguments = arguments.subspan(2);
  }
  command.Verb = arguments.front();
  command.Arguments = arguments.subspan(1);
  return command;
}
}
#endif
