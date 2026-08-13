/* WHAT EVERY SHADER THIS ENGINE EMITS BEGINS WITH. The shaders are generated -- spliced from the
 * emitters beside this file and compiled by the device at bring-up -- so the two lines that open a
 * Metal translation unit are stated here once rather than at the head of every emitter.
 *
 * THE TARGET LANGUAGE IS MSL AND THE DEVICE IS THE COMPILER. SDL_GPU takes MSL source directly
 * (`SDL_GPU_SHADERFORMAT_MSL`) and hands it to Metal, so the engine needs no shader toolchain, no
 * offline step and no third-party translator in the tree; what a stage emits is what the driver
 * reads. A second shader format is a second thing that can disagree with this one. */
#ifndef SHADERPRELUDE_H
#define SHADERPRELUDE_H

namespace outshine::Render {

static const char *kMslPrelude = R"(
#include <metal_stdlib>
using namespace metal;
)";

} // namespace outshine::Render
#endif
