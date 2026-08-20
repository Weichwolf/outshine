#ifndef SHADERPRELUDE_H
#define SHADERPRELUDE_H

namespace outshine::Render {

static const char *kMslPrelude = R"(
#include <metal_stdlib>
using namespace metal;
)";

}
#endif
