#include "StagePipelines.h"
#include "SpirvFile.h"
#include "Check.h"
#include <array>
#include <print>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  auto device = Native::CreateDevice();
  CHECK(device, "native stage compilation needs no SDL video or window");
  if (!device) {
    std::println(stderr, "{}", device.error().Message);
    return Report();
  }
  for (const auto &shader : ComputeShaders()) {
    auto pipeline = CreateComputePipeline(**device, shader.Id);
    CHECK(pipeline && *pipeline, "each declared stage compiles through the actual native device");
    if (!pipeline) { std::println(stderr, "{}", pipeline.error().Message); }
  }
  for (const auto id : {ComputeShaderId::Count, static_cast<ComputeShaderId>(~uint32_t{0})}) {
    const auto rejected = CreateComputePipeline(**device, id);
    CHECK(!rejected && rejected.error().Kind == Native::ErrorKind::Invalid,
          "unknown stages cannot publish an empty or unrelated pipeline");
  }
  const auto depth = *FindComputeShader(ComputeShaderId::DepthPyramid);
  for (auto field : {&ComputeShape::Samplers,
                     &ComputeShape::ReadOnlyTextures,
                     &ComputeShape::ReadWriteTextures,
                     &ComputeShape::ReadOnlyBuffers,
                     &ComputeShape::ReadWriteBuffers,
                     &ComputeShape::UniformBuffers,
                     &ComputeShape::GroupX,
                     &ComputeShape::GroupY,
                     &ComputeShape::GroupZ}) {
    auto wrong = depth;
    ++(wrong.Shape.*field);
    const auto rejected = CompileComputePipeline(**device, wrong);
    CHECK(!rejected && rejected.error().Kind == Native::ErrorKind::Invalid &&
              rejected.error().Message.find("declared compute bindings") != std::string::npos,
          "native SPIR-V reflection rejects each incorrect binding count or group dimension");
  }
  auto missing = depth;
  missing.Path = "build/shaders/no-such-stage.comp.spv";
  const auto absent = CompileComputePipeline(**device, missing);
  CHECK(!absent && absent.error().Kind == Native::ErrorKind::Invalid,
        "a missing shader artifact is an explicit error");
  const auto words = ReadSpirv(depth.Path);
  CHECK(words && words->size() >= 5 && words->front() == 0x07230203u,
        "both stage paths read a complete SPIR-V word stream");
  return Report();
}
