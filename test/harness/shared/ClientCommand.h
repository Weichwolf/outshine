#ifndef OUTSHINE_TEST_CLIENTCOMMAND_H
#define OUTSHINE_TEST_CLIENTCOMMAND_H

#include "Shell.h"
#include <SDL3/SDL_stdinc.h>
#include <string>

namespace outshine::Test {

[[nodiscard]] inline std::string ClientCommand() {
  const char *binary = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "OUTSHINE_CLIENT");
  return ShellArgument(binary != nullptr ? binary : "build/outshine");
}

}

#endif
