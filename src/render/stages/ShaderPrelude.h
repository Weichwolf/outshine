#ifndef SHADERPRELUDE_H
#define SHADERPRELUDE_H

#include <cstdio>
#include <numbers>
#include <string>

namespace outshine::Render {

static const char *kMslPrelude = R"(
#include <metal_stdlib>
using namespace metal;
)";

[[nodiscard]] inline std::string MslPrelude() {
  char pi[48];
  std::snprintf(pi, sizeof pi, "#define OUTSHINE_PI %.17g\n", std::numbers::pi);
  return std::string(kMslPrelude) + pi;
}

}
#endif
