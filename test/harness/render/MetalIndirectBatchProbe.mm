#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <chrono>
#include <concepts>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <print>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr NSUInteger kSlots = 3305;
constexpr NSUInteger kWidth = 1280;
constexpr NSUInteger kHeight = 720;
constexpr NSUInteger kBytesPerRow = kWidth * 4;
constexpr NSUInteger kImageBytes = kBytesPerRow * kHeight;
constexpr NSUInteger kRounds = 24;
constexpr NSUInteger kCoveredPixelsPerTriangle = 28;
constexpr double kNsPerSecond = 1e9;
constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;
const std::array<NSString *, 4> kNames{@"loop", @"icb", @"compact_icb", @"regular_loop"};

double Nanoseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
}

struct Scene {
  id<MTLDevice> Device;
  id<MTLCommandQueue> Queue;
  id<MTLRenderPipelineState> Pipeline;
  id<MTLRenderPipelineState> RegularPipeline;
  id<MTLBuffer> Indices;
  id<MTLBuffer> Arguments;
  id<MTLBuffer> Readback;
  id<MTLTexture> Target;
};

bool Create(Scene &scene, NSString *shaderPath) {
  if (shaderPath == nil) { return false; }
  scene.Device = MTLCreateSystemDefaultDevice();
  if (scene.Device == nil) { return false; }
  scene.Queue = [scene.Device newCommandQueue];
  NSError *error = nil;
  NSString *const source = [NSString stringWithContentsOfFile:shaderPath
                                                     encoding:NSUTF8StringEncoding
                                                        error:&error];
  if (source == nil) {
    std::println(stderr, "{}", error.localizedDescription.UTF8String);
    return false;
  }
  const id<MTLLibrary> library = [scene.Device newLibraryWithSource:source
                                                            options:nil
                                                              error:&error];
  if (library == nil) {
    std::println(stderr, "{}", error.localizedDescription.UTF8String);
    return false;
  }
  MTLRenderPipelineDescriptor *const descriptor = [MTLRenderPipelineDescriptor new];
  descriptor.vertexFunction = [library newFunctionWithName:@"vertex_main"];
  descriptor.fragmentFunction = [library newFunctionWithName:@"fragment_main"];
  descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
  descriptor.supportIndirectCommandBuffers = YES;
  scene.Pipeline = [scene.Device newRenderPipelineStateWithDescriptor:descriptor error:&error];
  if (scene.Pipeline == nil) {
    std::println(stderr, "{}", error.localizedDescription.UTF8String);
    return false;
  }
  descriptor.supportIndirectCommandBuffers = NO;
  scene.RegularPipeline = [scene.Device newRenderPipelineStateWithDescriptor:descriptor
                                                                       error:&error];
  if (scene.RegularPipeline == nil) { return false; }
  const std::array<uint32_t, 3> indices{0, 1, 2};
  scene.Indices = [scene.Device newBufferWithBytes:indices.data()
                                            length:sizeof(indices)
                                           options:MTLResourceStorageModeShared];
  scene.Arguments =
      [scene.Device newBufferWithLength:kSlots * sizeof(MTLDrawIndexedPrimitivesIndirectArguments)
                                options:MTLResourceStorageModeShared];
  scene.Readback = [scene.Device newBufferWithLength:kImageBytes
                                             options:MTLResourceStorageModeShared];
  MTLTextureDescriptor *const target =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                         width:kWidth
                                                        height:kHeight
                                                     mipmapped:NO];
  target.storageMode = MTLStorageModePrivate;
  target.usage = MTLTextureUsageRenderTarget;
  scene.Target = [scene.Device newTextureWithDescriptor:target];
  return scene.Queue != nil && scene.Indices != nil && scene.Arguments != nil &&
         scene.Readback != nil && scene.Target != nil;
}

struct Commands {
  id<MTLIndirectCommandBuffer> Full;
  id<MTLIndirectCommandBuffer> Compact;
  NSUInteger Active = 0;
  double BuildNs = 0;
};

Commands Prepare(const Scene &scene, NSUInteger percent) {
  const auto start = Clock::now();
  MTLIndirectCommandBufferDescriptor *const descriptor = [MTLIndirectCommandBufferDescriptor new];
  descriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
  descriptor.inheritBuffers = YES;
  descriptor.inheritPipelineState = YES;
  Commands result;
  result.Full = [scene.Device newIndirectCommandBufferWithDescriptor:descriptor
                                                     maxCommandCount:kSlots
                                                             options:0];
  result.Compact = [scene.Device newIndirectCommandBufferWithDescriptor:descriptor
                                                        maxCommandCount:kSlots
                                                                options:0];
  auto *const arguments =
      static_cast<MTLDrawIndexedPrimitivesIndirectArguments *>(scene.Arguments.contents);
  if (result.Full == nil || result.Compact == nil || arguments == nullptr) { return result; }
  for (NSUInteger slot = 0; slot < kSlots; ++slot) {
    const uint32_t active = slot % 100 < percent ? 1 : 0;
    arguments[slot] = {.indexCount = 3,
                       .instanceCount = active,
                       .indexStart = 0,
                       .baseVertex = 0,
                       .baseInstance = static_cast<uint32_t>(slot)};
    if (active == 0) {
      [[result.Full indirectRenderCommandAtIndex:slot] reset];
      continue;
    }
    const id<MTLIndirectRenderCommand> command = [result.Full indirectRenderCommandAtIndex:slot];
    [command drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:3
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:scene.Indices
                 indexBufferOffset:0
                     instanceCount:active
                        baseVertex:0
                      baseInstance:slot];
    const id<MTLIndirectRenderCommand> compact =
        [result.Compact indirectRenderCommandAtIndex:result.Active++];
    [compact drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:3
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:scene.Indices
                 indexBufferOffset:0
                     instanceCount:1
                        baseVertex:0
                      baseInstance:slot];
  }
  result.BuildNs = Nanoseconds(start);
  return result;
}

struct Sample {
  double EncodeNs = 0;
  double CommitNs = 0;
  double HostNs = 0;
  double GpuNs = 0;
  std::vector<uint8_t> Image;
};

bool Draw(const Scene &scene, const Commands &prepared, NSUInteger mode, Sample &sample) {
  const auto start = Clock::now();
  const id<MTLCommandBuffer> commands = [scene.Queue commandBuffer];
  MTLRenderPassDescriptor *const pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = scene.Target;
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
  const id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
  [encoder setRenderPipelineState:mode == 3 ? scene.RegularPipeline : scene.Pipeline];
  [encoder setCullMode:MTLCullModeNone];
  [encoder useResource:scene.Indices usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
  if (mode == 0 || mode == 3) {
    for (NSUInteger slot = 0; slot < kSlots; ++slot) {
      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                           indexType:MTLIndexTypeUInt32
                         indexBuffer:scene.Indices
                   indexBufferOffset:0
                      indirectBuffer:scene.Arguments
                indirectBufferOffset:slot * sizeof(MTLDrawIndexedPrimitivesIndirectArguments)];
    }
  } else if (mode == 1) {
    [encoder executeCommandsInBuffer:prepared.Full withRange:NSMakeRange(0, kSlots)];
  } else if (prepared.Active != 0) {
    [encoder executeCommandsInBuffer:prepared.Compact withRange:NSMakeRange(0, prepared.Active)];
  }
  [encoder endEncoding];
  sample.EncodeNs = Nanoseconds(start);
  const auto commit = Clock::now();
  [commands commit];
  sample.CommitNs = Nanoseconds(commit);
  [commands waitUntilCompleted];
  sample.HostNs = Nanoseconds(start);
  if (commands.status != MTLCommandBufferStatusCompleted) {
    std::println(stderr, "{}", commands.error.localizedDescription.UTF8String);
    return false;
  }
  if (!std::isfinite(commands.GPUStartTime) || commands.GPUStartTime <= 0 ||
      !std::isfinite(commands.GPUEndTime) || commands.GPUEndTime <= commands.GPUStartTime) {
    return false;
  }
  sample.GpuNs = (commands.GPUEndTime - commands.GPUStartTime) * kNsPerSecond;
  const id<MTLCommandBuffer> readback = [scene.Queue commandBuffer];
  const id<MTLBlitCommandEncoder> blit = [readback blitCommandEncoder];
  [blit copyFromTexture:scene.Target
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(kWidth, kHeight, 1)
                      toBuffer:scene.Readback
             destinationOffset:0
        destinationBytesPerRow:kBytesPerRow
      destinationBytesPerImage:kImageBytes];
  [blit endEncoding];
  [readback commit];
  [readback waitUntilCompleted];
  if (readback.status != MTLCommandBufferStatusCompleted) { return false; }
  const auto *const bytes = static_cast<const uint8_t *>(scene.Readback.contents);
  sample.Image.assign(bytes, bytes + kImageBytes);
  return true;
}

uint64_t Hash(const std::vector<uint8_t> &image) {
  uint64_t result = kFnvOffset;
  for (const auto byte : image) { result = (result ^ byte) * kFnvPrime; }
  return result;
}

NSUInteger Colored(const std::vector<uint8_t> &image) {
  NSUInteger result = 0;
  for (NSUInteger offset = 0; offset < image.size(); offset += 4) {
    if (image[offset] != 0 || image[offset + 1] != 0 || image[offset + 2] != 0) { ++result; }
  }
  return result;
}

template <std::integral T> NSNumber *Number(T value) {
  return [NSNumber numberWithUnsignedLongLong:value];
}

NSNumber *Number(double value) {
  return [NSNumber numberWithDouble:value];
}

bool Record(const Scene &scene,
            const Commands &prepared,
            NSUInteger mode,
            NSUInteger percent,
            NSUInteger round,
            std::vector<uint8_t> &reference,
            NSMutableArray *rows) {
  Sample sample;
  if (!Draw(scene, prepared, mode, sample)) { return false; }
  if (reference.empty()) { reference = sample.Image; }
  if (sample.Image != reference ||
      Colored(sample.Image) != prepared.Active * kCoveredPixelsPerTriangle) {
    std::println(stderr, "image mismatch or incorrect coverage");
    return false;
  }
  [rows addObject:@{
    @"percent" : Number(percent),
    @"active" : Number(prepared.Active),
    @"slots" : Number(kSlots),
    @"mode" : kNames[mode],
    @"round" : Number(round),
    @"encode_ns" : Number(sample.EncodeNs),
    @"commit_ns" : Number(sample.CommitNs),
    @"host_ns" : Number(sample.HostNs),
    @"gpu_ns" : Number(sample.GpuNs),
    @"build_ns" : Number(prepared.BuildNs),
    @"full_icb_bytes" : Number(prepared.Full.allocatedSize),
    @"compact_icb_bytes" : Number(prepared.Compact.allocatedSize),
    @"argument_bytes" : Number(scene.Arguments.length),
    @"argument_allocated_bytes" : Number(scene.Arguments.allocatedSize),
    @"fnv1a64" : Number(Hash(sample.Image)),
    @"colored_pixels" : Number(Colored(sample.Image))
  }];
  return true;
}

bool Measure(const Scene &scene, NSMutableArray *rows) {
  for (const NSUInteger percent : std::array<NSUInteger, 4>{0, 1, 10, 100}) {
    const auto prepared = Prepare(scene, percent);
    if (prepared.Full == nil || prepared.Compact == nil) { return false; }
    std::vector<uint8_t> reference;
    for (NSUInteger round = 0; round < kRounds; ++round) {
      for (NSUInteger at = 0; at < kNames.size(); ++at) {
        @autoreleasepool {
          const NSUInteger mode = round % 2 == 0 ? at : kNames.size() - 1 - at;
          if (!Record(scene, prepared, mode, percent, round, reference, rows)) { return false; }
        }
      }
    }
  }
  return true;
}
}

int main(int argc, const char *const *argv) {
  if (argc != 3) { return 2; }
  @autoreleasepool {
    NSString *const output = [NSString stringWithUTF8String:argv[2]];
    if (output == nil) { return 2; }
    Scene scene;
    if (!Create(scene, [NSString stringWithUTF8String:argv[1]])) { return 3; }
    NSMutableArray *const rows = [NSMutableArray array];
    if (!Measure(scene, rows)) { return 4; }
    NSError *error = nil;
    NSData *const json = [NSJSONSerialization dataWithJSONObject:@{
      @"device" : scene.Device.name,
      @"rounds" : Number(kRounds),
      @"rows" : rows
    }
                                                         options:NSJSONWritingPrettyPrinted
                                                           error:&error];
    if (json == nil || ![json writeToFile:output atomically:YES]) { return 5; }
    NSData *const image = [NSData dataWithBytes:scene.Readback.contents length:kImageBytes];
    if (![image writeToFile:[output stringByAppendingString:@".rgba"] atomically:YES]) { return 5; }
    std::println("{} matched renders on {}", rows.count, scene.Device.name.UTF8String);
  }
  return 0;
}
