#include "MetalShader.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <print>

namespace {

constexpr uint32_t kWidth = 35, kHeight = 19, kTargets = 5, kRowBytes = 768;
constexpr size_t kTextureBytes = size_t{kHeight} * kRowBytes;
constexpr uint32_t kValues = 246;
constexpr size_t kAtOffset = 48;
constexpr float kDepthRange = 256.0f;
constexpr double kMillisecondsPerSecond = 1000;

struct alignas(16) Reducing {
  std::array<uint32_t, 4> Source{kWidth, kHeight, 0, 0};
  std::array<uint32_t, 4> Wide{18, 9, 5, 3}, High{10, 5, 3, 2}, At{0, 180, 225, 240};
};

static_assert(sizeof(Reducing) == 64 && alignof(Reducing) == 16 &&
              offsetof(Reducing, At) == kAtOffset);

struct Resources {
  std::array<id<MTLTexture>, kTargets> Colours;
  id<MTLTexture> Depth;
  id<MTLBuffer> Pyramid, Readback;
};

double Milliseconds(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}

id<MTLFunction> Function(id<MTLDevice> device, const char *path, MetalShader &shader) {
  const auto translated = TranslateMetal(path);
  if (!translated) {
    std::println(stderr, "{}: {}", path, translated.error());
    return nil;
  }
  shader = *translated;
  std::ofstream(std::string(path) + ".metal") << shader.Source;
  NSError *error = nil;
  MTLCompileOptions *const options = [MTLCompileOptions new];
  options.mathMode = MTLMathModeSafe;
  const id<MTLLibrary> library =
      [device newLibraryWithSource:[NSString stringWithUTF8String:shader.Source.c_str()]
                           options:options
                             error:&error];
  if (library == nil) {
    std::println(stderr, "{}: {}", path, error.localizedDescription.UTF8String);
    return nil;
  }
  return [library newFunctionWithName:[NSString stringWithUTF8String:shader.Entry.c_str()]];
}

id<MTLTexture> Texture(id<MTLDevice> device, MTLPixelFormat format) {
  MTLTextureDescriptor *const info =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                         width:kWidth
                                                        height:kHeight
                                                     mipmapped:NO];
  info.storageMode = MTLStorageModePrivate;
  info.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  return [device newTextureWithDescriptor:info];
}

Resources Allocate(id<MTLDevice> device) {
  Resources resources;
  for (auto &colour : resources.Colours) { colour = Texture(device, MTLPixelFormatRGBA32Float); }
  resources.Depth = Texture(device, MTLPixelFormatDepth32Float);
  resources.Pyramid = [device newBufferWithLength:kValues * sizeof(float)
                                          options:MTLResourceStorageModePrivate];
  resources.Readback =
      [device newBufferWithLength:kTargets * kTextureBytes + kValues * sizeof(float)
                          options:MTLResourceStorageModeShared];
  return resources;
}

id<MTLRenderPipelineState> RenderPipeline(id<MTLDevice> device,
                                          const std::array<id<MTLFunction>, 2> &functions) {
  MTLRenderPipelineDescriptor *const info = [MTLRenderPipelineDescriptor new];
  info.vertexFunction = functions[0];
  info.fragmentFunction = functions[1];
  info.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  for (uint32_t i = 0; i < kTargets; ++i) {
    info.colorAttachments[i].pixelFormat = MTLPixelFormatRGBA32Float;
  }
  NSError *error = nil;
  const id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:info
                                                                                     error:&error];
  if (pipeline == nil) {
    std::println(stderr, "render: {}", error.localizedDescription.UTF8String);
  }
  return pipeline;
}

void Draw(id<MTLDevice> device,
          id<MTLCommandBuffer> commands,
          id<MTLRenderPipelineState> pipeline,
          const Resources &resources) {
  MTLRenderPassDescriptor *const pass = [MTLRenderPassDescriptor renderPassDescriptor];
  for (uint32_t i = 0; i < kTargets; ++i) {
    pass.colorAttachments[i].texture = resources.Colours[i];
    pass.colorAttachments[i].loadAction = MTLLoadActionClear;
    pass.colorAttachments[i].storeAction = MTLStoreActionStore;
  }
  pass.depthAttachment.texture = resources.Depth;
  pass.depthAttachment.loadAction = MTLLoadActionClear;
  pass.depthAttachment.storeAction = MTLStoreActionStore;
  pass.depthAttachment.clearDepth = 1.0;
  MTLDepthStencilDescriptor *const depth = [MTLDepthStencilDescriptor new];
  depth.depthCompareFunction = MTLCompareFunctionAlways;
  depth.depthWriteEnabled = YES;
  const id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
  [encoder setRenderPipelineState:pipeline];
  [encoder setDepthStencilState:[device newDepthStencilStateWithDescriptor:depth]];
  [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
  [encoder endEncoding];
}

bool Reduce(id<MTLCommandBuffer> commands,
            id<MTLComputePipelineState> pipeline,
            const MetalShader &shader,
            const Resources &resources) {
  const id<MTLComputeCommandEncoder> encoder = [commands computeCommandEncoder];
  [encoder setComputePipelineState:pipeline];
  const Reducing reducing;
  uint32_t matched = 0;
  for (const auto &buffer : shader.Buffers) {
    if (buffer.Set == 1 && buffer.Binding == 0) {
      [encoder setBuffer:resources.Pyramid offset:0 atIndex:buffer.Slot];
      ++matched;
    } else if (buffer.Set == 2 && buffer.Binding == 0) {
      [encoder setBytes:&reducing length:sizeof(reducing) atIndex:buffer.Slot];
      ++matched;
    }
  }
  for (const auto &texture : shader.Textures) {
    if (texture.Set == 0 && texture.Binding == 0) {
      [encoder setTexture:resources.Depth atIndex:texture.Slot];
      ++matched;
    }
  }
  [encoder dispatchThreadgroups:MTLSizeMake((kWidth + 15) / 16, (kHeight + 15) / 16, 1)
          threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
  [encoder endEncoding];
  return matched == 3;
}

void Download(id<MTLCommandBuffer> commands, const Resources &resources) {
  const id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
  for (uint32_t i = 0; i < kTargets; ++i) {
    [blit copyFromTexture:resources.Colours[i]
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(kWidth, kHeight, 1)
                        toBuffer:resources.Readback
               destinationOffset:i * kTextureBytes
          destinationBytesPerRow:kRowBytes
        destinationBytesPerImage:kTextureBytes];
  }
  [blit copyFromBuffer:resources.Pyramid
           sourceOffset:0
               toBuffer:resources.Readback
      destinationOffset:kTargets * kTextureBytes
                   size:kValues * sizeof(float)];
  [blit endEncoding];
}

float Expected(uint32_t x, uint32_t y) {
  return static_cast<float>((x * 7 + y * 13) % 256) / kDepthRange;
}

bool VerifyColours(const std::byte *bytes) {
  for (uint32_t target = 0; target < kTargets; ++target) {
    for (uint32_t y = 0; y < kHeight; ++y) {
      const auto *row =
          reinterpret_cast<const float *>(bytes + target * kTextureBytes + size_t{y} * kRowBytes);
      for (uint32_t x = 0; x < kWidth; ++x) {
        const std::array expected{
            Expected(x, y), static_cast<float>(target) / 4.0f, static_cast<float>(x) / 64.0f, 1.0f};
        for (uint32_t channel = 0; channel < 4; ++channel) {
          if (row[x * 4 + channel] != expected[channel]) { return false; }
        }
      }
    }
  }
  return true;
}

float ExpectedMinimum(uint32_t x, uint32_t y, uint32_t side) {
  float expected = 1.0f;
  for (uint32_t dy = 0; dy < side; ++dy) {
    for (uint32_t dx = 0; dx < side; ++dx) {
      expected = std::min(
          expected,
          Expected(std::min(x * side + dx, kWidth - 1), std::min(y * side + dy, kHeight - 1)));
    }
  }
  return expected;
}

bool VerifyPyramid(const float *values) {
  const Reducing reducing;
  for (uint32_t level = 0; level < 4; ++level) {
    const uint32_t side = 2u << level;
    for (uint32_t y = 0; y < reducing.High[level]; ++y) {
      for (uint32_t x = 0; x < reducing.Wide[level]; ++x) {
        const float expected = ExpectedMinimum(x, y, side);
        if (values[reducing.At[level] + y * reducing.Wide[level] + x] != expected) { return false; }
      }
    }
  }
  return true;
}

int Run(char *const *paths) {
  const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (device == nil) { return 2; }
  MetalShader vertexShader;
  MetalShader fragmentShader;
  MetalShader computeShader;
  const id<MTLFunction> vertex = Function(device, paths[0], vertexShader);
  const id<MTLFunction> fragment = Function(device, paths[1], fragmentShader);
  const id<MTLFunction> compute = Function(device, paths[2], computeShader);
  if (vertex == nil || fragment == nil || compute == nil) { return 3; }
  const auto pipeline = RenderPipeline(device, {vertex, fragment});
  NSError *error = nil;
  const id<MTLComputePipelineState> reduction = [device newComputePipelineStateWithFunction:compute
                                                                                      error:&error];
  if (pipeline == nil || reduction == nil) {
    std::println(stderr, "pipeline: {}", error.localizedDescription.UTF8String);
    return 4;
  }
  const auto resources = Allocate(device);
  for (const auto &texture : resources.Colours) {
    if (texture == nil) { return 5; }
  }
  if (resources.Depth == nil || resources.Pyramid == nil || resources.Readback == nil) { return 5; }
  const id<MTLCommandQueue> queue = [device newCommandQueue];
  const id<MTLCommandBuffer> commands = [queue commandBuffer];
  if (commands == nil) { return 6; }
  const auto encoding = std::chrono::steady_clock::now();
  Draw(device, commands, pipeline, resources);
  if (!Reduce(commands, reduction, computeShader, resources)) { return 7; }
  Download(commands, resources);
  const double encodeMs = Milliseconds(encoding);
  [commands commit];
  const auto waiting = std::chrono::steady_clock::now();
  [commands waitUntilCompleted];
  const double waitMs = Milliseconds(waiting);
  if (commands.status != MTLCommandBufferStatusCompleted) {
    std::println(stderr, "submission: {}", commands.error.localizedDescription.UTF8String);
    return 8;
  }
  const auto *bytes = static_cast<const std::byte *>(resources.Readback.contents);
  if (!VerifyColours(bytes) ||
      !VerifyPyramid(reinterpret_cast<const float *>(bytes + kTargets * kTextureBytes))) {
    std::println(stderr, "GPU readback differs from CPU reference");
    return 9;
  }
  const bool timed = commands.GPUStartTime > 0 && commands.GPUEndTime >= commands.GPUStartTime;
  std::println("{{\"device\":\"{}\",\"mrt\":5,\"depth\":true,\"pyramid_values\":246,\"colour_"
               "channels\":13300,\"cpu_encode_ms\":{:.6f},\"cpu_wait_ms\":{:.6f},\"gpu_time_"
               "available\":{},\"gpu_submission_ms\":{:.6f},\"device_allocated_bytes\":{}}}",
               device.name.UTF8String,
               encodeMs,
               waitMs,
               timed,
               timed ? (commands.GPUEndTime - commands.GPUStartTime) * kMillisecondsPerSecond
                     : -1.0,
               static_cast<unsigned long long>(device.currentAllocatedSize));
  return 0;
}
}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 1; }
  @autoreleasepool {
    return Run(argv + 1);
  }
}
