#pragma once

#include "Shell.h"
#include <cstdlib>
#include <string>

namespace outshine::Test {

[[nodiscard]] inline std::string ClientCommand() {
  const char *binary = std::getenv("OUTSHINE_CLIENT");
  return ShellArgument(binary != nullptr ? binary : "build/outshine");
}

}
