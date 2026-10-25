#include "UiAction.h"
#include "ActionHostAdapter.h"
#include "Script.h"
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace outshine {
namespace {
constexpr Script::Budget kUiScriptBudget{.InputBytes = size_t{64} * 1024,
                                         .MemoryBytes = size_t{512} * 1024,
                                         .StackBytes = size_t{128} * 1024,
                                         .HostBytes = size_t{16} * 1024};
}

std::expected<bool, std::string>
RunUiAction(Host *receiver, std::string_view programme, std::string_view action) {
  if (receiver == nullptr) { return std::unexpected("UI action requires an offered host"); }
  constexpr size_t kSeparators = 3;
  constexpr size_t kPayload = kUiScriptBudget.InputBytes - kSeparators;
  if (programme.size() > kPayload || action.size() > kPayload - programme.size()) {
    return std::unexpected("UI action exceeds its source budget");
  }
  ActionHostAdapter bindings(receiver);
  Script::Program program(kUiScriptBudget);
  std::string source;
  source.reserve(programme.size() + action.size() + kSeparators);
  source.append(programme).push_back('\n');
  source.append(action).append(";\n");
  std::string error;
  if (!program.Read(source, error) || !program.Run(bindings, error)) {
    return std::unexpected(error);
  }
  return bindings.Fired();
}

}
