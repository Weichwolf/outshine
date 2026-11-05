#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <print>
#include <string>
#include <span>
#include <charconv>
#include <system_error>
#include <vector>

namespace {
struct alignas(8) VertexPoint {
  float X, Y;
};

struct alignas(8) ClusterRoot {
  uint32_t First, Instance;
};

static_assert(sizeof(VertexPoint) == 8 && alignof(VertexPoint) == 8 &&
              offsetof(VertexPoint, Y) == 4);
static_assert(sizeof(ClusterRoot) == 8 && alignof(ClusterRoot) == 8 &&
              offsetof(ClusterRoot, Instance) == 4);

struct DrawArguments {
  uint32_t Count, Instances, First;
  int32_t Base;
  uint32_t Instance;
};

static_assert(sizeof(DrawArguments) == 20 && alignof(DrawArguments) == 4);
static_assert(sizeof(DrawArguments) == sizeof(MTLDrawIndexedPrimitivesIndirectArguments));
static_assert(alignof(DrawArguments) == alignof(MTLDrawIndexedPrimitivesIndirectArguments));
static_assert(offsetof(DrawArguments, Count) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexCount));
static_assert(offsetof(DrawArguments, Instances) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, instanceCount));
static_assert(offsetof(DrawArguments, First) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexStart));
static_assert(offsetof(DrawArguments, Base) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseVertex));
static_assert(offsetof(DrawArguments, Instance) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseInstance));
static_assert(offsetof(DrawArguments, Instances) == 4 && offsetof(DrawArguments, First) == 8);
static_assert(offsetof(DrawArguments, Base) == 12 && offsetof(DrawArguments, Instance) == 16);
constexpr auto kSource = R"(
#include <metal_stdlib>
using namespace metal;
struct DrawArguments { uint count, instances, first; int base; uint instance; };
struct Output { atomic_uint count; uint instances, first; int base; uint instance; };
static_assert(sizeof(DrawArguments) == 20 && alignof(DrawArguments) == 4);
static_assert(sizeof(Output) == 20 && alignof(Output) == 4);
static_assert(__builtin_offsetof(Output, count) == 0 && __builtin_offsetof(Output, instances) == 4);
static_assert(__builtin_offsetof(Output, first) == 8 && __builtin_offsetof(Output, base) == 12);
static_assert(__builtin_offsetof(Output, instance) == 16);
static_assert(sizeof(uint2) == 8 && alignof(uint2) == 8);
struct V { float4 position [[position]]; float4 colour; };
V shade(uint vid, uint instance, device const uint *colours, device const float2 *positions) {
  float2 xy = (float2(instance%128,instance/128) + positions[vid]) / float2(128,64);
  uint colour = colours[instance];
  return {float4(xy*2-1,0,1),float4(float(colour%251)/251,float(colour%127)/127,.5,1)};
}
vertex V original(uint vid [[vertex_id]], uint instance [[instance_id]],
                  device const uint *colours [[buffer(2)]], device const float2 *positions [[buffer(3)]]) {
  return shade(vid,instance,colours,positions);
}
vertex V pulling(uint vid [[vertex_id]], device const uint *colours [[buffer(2)]],
                 device const float2 *positions [[buffer(3)]], device const uint2 *roots [[buffer(4)]]) {
  uint2 root = roots[vid >> 6];
  return shade(root.x + (vid & 63),root.y,colours,positions);
}
fragment float4 fragmentMain(V in [[stage_in]]) { return in.colour; }
kernel void compact(device const DrawArguments *args [[buffer(0)]],
                    device uint *indices [[buffer(1)]], device Output &output [[buffer(2)]],
                    constant uint &count [[buffer(3)]], uint id [[thread_position_in_grid]]) {
  if (id >= count) return;
  if (id == 0) output.instances = 1;
  if (args[id].count == 0 || args[id].instances == 0) return;
  uint first = atomic_fetch_add_explicit(&output.count,3,memory_order_relaxed);
  indices[first] = id*64;
  indices[first+1] = id*64+1;
  indices[first+2] = id*64+2;
}
)";

constexpr uint32_t kRecords = 8192;

struct Programs {
  std::array<id<MTLRenderPipelineState>, 2> Pipelines;
  id<MTLComputePipelineState> Compute;
};

struct Buffers {
  id<MTLBuffer> Arguments, Colours, Positions, Roots, Indices, SelectedIndices, Count;
  id<MTLTexture> Target;
  id<MTLCommandQueue> Queue;
};

id<MTLCommandBuffer> Encode(const Programs &programs, const Buffers &buffers, uint32_t mode) {
  const auto &[pipelines, compute] = programs;
  const auto &[arguments,
               colourBuffer,
               positionBuffer,
               rootBuffer,
               originalIndices,
               compactIndices,
               draw,
               target,
               queue] = buffers;
  constexpr uint32_t count = kRecords;
  const auto commands = [queue commandBuffer];
  if (mode == 1) {
    const auto clear = [commands blitCommandEncoder];
    [clear fillBuffer:draw range:NSMakeRange(0, sizeof(DrawArguments)) value:0];
    [clear endEncoding];
    const auto encoder = [commands computeCommandEncoder];
    [encoder setComputePipelineState:compute];
    [encoder setBuffer:arguments offset:0 atIndex:0];
    [encoder setBuffer:compactIndices offset:0 atIndex:1];
    [encoder setBuffer:draw offset:0 atIndex:2];
    [encoder setBytes:&count length:sizeof(count) atIndex:3];
    [encoder dispatchThreads:MTLSizeMake(count, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
    [encoder endEncoding];
  }
  const auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = target;
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  const auto render = [commands renderCommandEncoderWithDescriptor:pass];
  [render setRenderPipelineState:pipelines[mode]];
  [render setVertexBuffer:colourBuffer offset:0 atIndex:2];
  [render setVertexBuffer:positionBuffer offset:0 atIndex:3];
  if (mode == 1) { [render setVertexBuffer:rootBuffer offset:0 atIndex:4]; }
  if (mode == 0) {
    for (uint32_t at = 0; at < count; ++at) {
      [render drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                          indexType:MTLIndexTypeUInt32
                        indexBuffer:originalIndices
                  indexBufferOffset:4
                     indirectBuffer:arguments
               indirectBufferOffset:at * sizeof(DrawArguments)];
    }
  } else {
    [render drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexType:MTLIndexTypeUInt32
                      indexBuffer:compactIndices
                indexBufferOffset:0
                   indirectBuffer:draw
             indirectBufferOffset:0];
  }
  [render endEncoding];
  return commands;
}

const char *Message(NSError *error) {
  const char *const message = error.localizedDescription.UTF8String;
  return message != nullptr ? message : "Metal operation failed";
}

bool HasColour(std::span<const uint8_t> pixels) {
  for (size_t at = 0; at < pixels.size(); at += 4) {
    if (pixels[at] != 0 || pixels[at + 1] != 0 || pixels[at + 2] != 0) { return true; }
  }
  return false;
}

int Measure(const Programs &programs, const Buffers &buffers, uint32_t divisor) {
  std::array<double, 2> cpu{};
  std::array<double, 2> gpu{};
  std::vector<uint8_t> reference(size_t{256} * 128 * 4);
  std::vector<uint8_t> actual(reference.size());
  const auto &target = buffers.Target;
  for (uint32_t round = 0; round < 14; ++round) {
    for (uint32_t one = 0; one < 2; ++one) {
      const uint32_t mode = (one + round) % 2;
      const auto start = std::chrono::steady_clock::now();
      const auto commands = Encode(programs, buffers, mode);
      const auto encoded = std::chrono::steady_clock::now();
      [commands commit];
      [commands waitUntilCompleted];
      if (commands.status != MTLCommandBufferStatusCompleted) {
        std::println(stderr, "{}", Message(commands.error));
        return 5;
      }
      const double elapsed = (commands.GPUEndTime - commands.GPUStartTime) * 1000;
      if (!std::isfinite(elapsed) || elapsed <= 0) { return 8; }
      if (round >= 2) {
        cpu[mode] += std::chrono::duration<double, std::milli>(encoded - start).count();
        gpu[mode] += elapsed;
      }
      [target getBytes:actual.data()
           bytesPerRow:NSUInteger{256} * 4
            fromRegion:MTLRegionMake2D(0, 0, 256, 128)
           mipmapLevel:0];
      if (round == 0 && mode == 0) {
        reference = actual;
      } else if (actual != reference) {
        std::println(stderr, "pixel mismatch mode={} round={}", mode, round);
        return 6;
      }
    }
  }
  if ((divisor != 0) != HasColour(reference)) {
    std::println(stderr, "nonempty visibility must produce actual pixels");
    return 7;
  }
  for (uint32_t mode = 0; mode < 2; ++mode) {
    std::println("mode={} records={} divisor={} cpu_ms={:.6f} gpu_ms={:.6f} equal=1",
                 mode == 0 ? "loop" : "pull",
                 kRecords,
                 divisor,
                 cpu[mode] / 12,
                 gpu[mode] / 12);
  }
  return 0;
}
}

int main(int argc, char *const *argv) {
  @autoreleasepool {
    uint32_t divisor = 64;
    if (argc > 2) { return 1; }
    if (argc == 2) {
      const std::string argument(argv[1]);
      const auto parsed =
          std::from_chars(argument.data(), argument.data() + argument.size(), divisor);
      if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size()) { return 1; }
    }
    auto device = MTLCreateSystemDefaultDevice();
    NSError *error = nil;
    NSString *const source = [NSString stringWithUTF8String:kSource];
    if (source == nil) { return 2; }
    auto library = [device newLibraryWithSource:source options:nil error:&error];
    if (library == nullptr) {
      std::println(stderr, "{}", Message(error));
      return 2;
    }
    const auto pipeline = [&](NSString *entry) {
      const auto desc = [MTLRenderPipelineDescriptor new];
      desc.vertexFunction = [library newFunctionWithName:entry];
      desc.fragmentFunction = [library newFunctionWithName:@"fragmentMain"];
      desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
      return [device newRenderPipelineStateWithDescriptor:desc error:&error];
    };
    std::array<id<MTLRenderPipelineState>, 2> pipelines{pipeline(@"original"),
                                                        pipeline(@"pulling")};
    const id<MTLFunction> function = [library newFunctionWithName:@"compact"];
    if (function == nil) { return 3; }
    const auto compute = [device newComputePipelineStateWithFunction:function error:&error];
    if ((pipelines[0] == nullptr) || (pipelines[1] == nullptr) || (compute == nullptr)) {
      std::println(stderr, "{}", Message(error));
      return 3;
    }
    constexpr uint32_t count = kRecords;
    std::vector<DrawArguments> args(count);
    std::vector<uint32_t> colours(count);
    std::vector<VertexPoint> positions(size_t{count} * 3);
    std::vector<ClusterRoot> roots(count);
    std::vector<uint32_t> rawIndices(size_t{count} * 3 + 1);
    rawIndices[0] = 0xdeadbeef;
    for (uint32_t at = 0; at < count; ++at) {
      args[at] = {.Count = divisor != 0 && at % divisor == 0 ? 3u : 0u,
                  .Instances = 1,
                  .First = at * 3,
                  .Base = -1,
                  .Instance = (at * 73 + 19) % count};
      colours[at] = at ^ 0x5a;
      roots[at] = {.First = ((at * 37 + 11) % count) * 3, .Instance = args[at].Instance};
      positions[size_t{at} * 3] =
          at % 2 == 0 ? VertexPoint{.X = 0, .Y = 0} : VertexPoint{.X = 1, .Y = 1};
      positions[size_t{at} * 3 + 1] =
          at % 2 == 0 ? VertexPoint{.X = 1, .Y = 0} : VertexPoint{.X = 0, .Y = 1};
      positions[size_t{at} * 3 + 2] =
          at % 2 == 0 ? VertexPoint{.X = 0, .Y = 1} : VertexPoint{.X = 1, .Y = 0};
      for (uint32_t v = 0; v < 3; ++v) {
        rawIndices[1 + size_t{at} * 3 + v] = 1 + roots[at].First + v;
      }
    }
    const auto arguments = [device newBufferWithBytes:args.data()
                                               length:args.size() * sizeof(DrawArguments)
                                              options:MTLResourceStorageModeShared];
    const auto colourBuffer = [device newBufferWithBytes:colours.data()
                                                  length:colours.size() * 4
                                                 options:MTLResourceStorageModeShared];
    const auto originalIndices = [device newBufferWithBytes:rawIndices.data()
                                                     length:rawIndices.size() * sizeof(uint32_t)
                                                    options:MTLResourceStorageModeShared];
    const auto positionBuffer = [device newBufferWithBytes:positions.data()
                                                    length:positions.size() * sizeof(VertexPoint)
                                                   options:MTLResourceStorageModeShared];
    const auto rootBuffer = [device newBufferWithBytes:roots.data()
                                                length:roots.size() * sizeof(ClusterRoot)
                                               options:MTLResourceStorageModeShared];
    const auto compactIndices = [device newBufferWithLength:uint64_t{count} * 3 * 4
                                                    options:MTLResourceStorageModePrivate];
    const auto draw = [device newBufferWithLength:sizeof(DrawArguments)
                                          options:MTLResourceStorageModePrivate];
    const auto texture =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:256
                                                          height:128
                                                       mipmapped:NO];
    texture.storageMode = MTLStorageModeShared;
    texture.usage = MTLTextureUsageRenderTarget;
    const auto target = [device newTextureWithDescriptor:texture];
    const auto queue = [device newCommandQueue];
    if ((arguments == nullptr) || (colourBuffer == nullptr) || (positionBuffer == nullptr) ||
        (rootBuffer == nullptr) || (originalIndices == nullptr) || (compactIndices == nullptr) ||
        (draw == nullptr) || (target == nullptr) || (queue == nullptr)) {
      return 4;
    }

    std::println("device={} records={} divisor={} rounds=14 warmup=2",
                 device.name.UTF8String,
                 kRecords,
                 divisor);
    return Measure({.Pipelines = pipelines, .Compute = compute},
                   {.Arguments = arguments,
                    .Colours = colourBuffer,
                    .Positions = positionBuffer,
                    .Roots = rootBuffer,
                    .Indices = originalIndices,
                    .SelectedIndices = compactIndices,
                    .Count = draw,
                    .Target = target,
                    .Queue = queue},
                   divisor);
  }
}
