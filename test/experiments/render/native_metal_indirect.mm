#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <print>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr uint32_t kWidth = 1280, kHeight = 720;
NSString *const kShader = @R"MSL(
#include <metal_stdlib>
#include <metal_command_buffer>
using namespace metal;
struct Args { uint count, instances, first; int base; uint instance; };
struct Settings { uint count, phase; };
struct Container { command_buffer commands [[id(0)]]; };
kernel void arguments(device Args *args [[buffer(0)]], constant Settings &settings [[buffer(1)]], uint i [[thread_position_in_grid]]) {
 if(i>=settings.count)return;
 const bool active = settings.phase==0 || (settings.phase==1 && i%4==0) || (settings.phase==3 && i%2==0);
 args[i] = {active?3u:0u, active?(i%3==0?2u:1u):0u, i%2==0?0u:3u, i%2==0?0:-3, i*2+settings.phase};
}
kernel void commands(device Container &container [[buffer(0)]], const device Args *args [[buffer(1)]], const device uint *indices [[buffer(2)]], constant Settings &settings [[buffer(3)]], uint i [[thread_position_in_grid]]) {
 if(i>=settings.count)return;
 render_command command(container.commands, i);
 const Args a=args[i];
 if(a.count==0 || a.instances==0) {
#if !OMIT_RESET
 command.reset();
#endif
}
 else command.draw_indexed_primitives(primitive_type::triangle, a.count, indices+a.first, a.instances, a.base, a.instance);
}
struct Raster { float4 position [[position]]; float4 colour; };
vertex Raster vertexMain(const device float4 *vertices [[buffer(0)]], uint vertexId [[vertex_id]], uint instance [[instance_id]]) {
 const uint x=instance%128, y=(instance/128)%64;
 Raster out;
 out.position=float4(vertices[vertexId].xy*float2(1.0/128,1.0/64)+float2((float(x)+.5)/64-1,(float(y)+.5)/32-1),.5,1);
 out.colour=float4(float((instance>>1)&1),float((instance>>2)&1),float((instance>>3)&1),1);
 return out;
}
fragment float4 fragmentMain(Raster in [[stage_in]], constant float4 &tint [[buffer(1)]]) { return in.colour*tint; }
)MSL";

[[noreturn]] void Fail(const char *message) {
  std::println(stderr, "{}", message);
  std::_Exit(1);
}

void Require(bool condition, const char *message) {
  if (!condition) { Fail(message); }
}

struct Settings {
  uint32_t Count, Phase;
};

struct Args {
  uint32_t Count, Instances, First;
  int32_t Base;
  uint32_t Instance;
};

static_assert(sizeof(Settings) == 8);
static_assert(alignof(Settings) == alignof(uint32_t));
static_assert(offsetof(Settings, Count) == 0);
static_assert(offsetof(Settings, Phase) == 4);
static_assert(sizeof(Args) == sizeof(MTLDrawIndexedPrimitivesIndirectArguments));
static_assert(alignof(Args) == alignof(MTLDrawIndexedPrimitivesIndirectArguments));
static_assert(offsetof(Args, Count) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexCount));
static_assert(offsetof(Args, Instances) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, instanceCount));
static_assert(offsetof(Args, First) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexStart));
static_assert(offsetof(Args, Base) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseVertex));
static_assert(offsetof(Args, Instance) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseInstance));

struct MetalIndirectProbe {
  id<MTLDevice> Device;
  id<MTLCommandQueue> Queue;
  id<MTLLibrary> Library;
  id<MTLComputePipelineState> Arguments, Commands;
  std::array<id<MTLRenderPipelineState>, 2> Pipelines;
  id<MTLArgumentEncoder> Encoder;
  std::array<id<MTLBuffer>, 2> Vertices;
  id<MTLBuffer> Indices;
  id<MTLTexture> Target;

  explicit MetalIndirectProbe(bool omitReset) {
    Device = MTLCreateSystemDefaultDevice();
    Require(Device != nil, "Metal device unavailable");
    Queue = [Device newCommandQueue];
    Require(Queue != nil, "queue unavailable");
    NSError *error = nil;
    MTLCompileOptions *const options = [MTLCompileOptions new];
    options.languageVersion = MTLLanguageVersion2_3;
    options.preprocessorMacros = @{@"OMIT_RESET" : [NSNumber numberWithBool:omitReset]};
    Library = [Device newLibraryWithSource:kShader options:options error:&error];
    if (Library == nil) { Fail(error.localizedDescription.UTF8String); }
    auto function = [Library newFunctionWithName:@"arguments"];
    if (function == nil) { Fail("argument producer function unavailable"); }
    Arguments = [Device newComputePipelineStateWithFunction:function error:&error];
    if (Arguments == nil) { Fail(error.localizedDescription.UTF8String); }
    function = [Library newFunctionWithName:@"commands"];
    if (function == nil) { Fail("command producer function unavailable"); }
    Commands = [Device newComputePipelineStateWithFunction:function error:&error];
    if (Commands == nil) { Fail(error.localizedDescription.UTF8String); }
    Encoder = [function newArgumentEncoderWithBufferIndex:0];
    Require(Encoder != nil, "ICB argument encoder unavailable");
    for (size_t mode = 0; mode < 2; ++mode) {
      const auto descriptor = [MTLRenderPipelineDescriptor new];
      descriptor.vertexFunction = [Library newFunctionWithName:@"vertexMain"];
      descriptor.fragmentFunction = [Library newFunctionWithName:@"fragmentMain"];
      descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
      descriptor.supportIndirectCommandBuffers = mode != 0;
      Pipelines[mode] = [Device newRenderPipelineStateWithDescriptor:descriptor error:&error];
      if (Pipelines[mode] == nil) { Fail(error.localizedDescription.UTF8String); }
    }
    std::array<float, 24> vertices{-.8f, -.8f, 0, 1, .8f, -.8f, 0, 1, 0, .8f, 0, 1,
                                   -.1f, -.1f, 0, 1, .1f, -.1f, 0, 1, 0, .1f, 0, 1};
    constexpr std::array<uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
    Vertices[0] = [Device newBufferWithBytes:vertices.data()
                                      length:sizeof(vertices)
                                     options:MTLResourceStorageModeShared];
    for (size_t i = 0; i < vertices.size(); ++i) {
      if (i % 4 < 2) { vertices[i] *= .75f; }
    }
    Vertices[1] = [Device newBufferWithBytes:vertices.data()
                                      length:sizeof(vertices)
                                     options:MTLResourceStorageModeShared];
    Indices = [Device newBufferWithBytes:indices.data()
                                  length:sizeof(indices)
                                 options:MTLResourceStorageModeShared];
    const auto texture =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:kWidth
                                                          height:kHeight
                                                       mipmapped:NO];
    texture.usage = MTLTextureUsageRenderTarget;
    texture.storageMode = MTLStorageModeShared;
    Target = [Device newTextureWithDescriptor:texture];
    Require(Target != nil, "target unavailable");
  }

  Json Sample(id<MTLBuffer> arguments,
              id<MTLIndirectCommandBuffer> icb,
              id<MTLBuffer> container,
              Settings settings,
              uint32_t mode,
              uint32_t sections,
              std::vector<uint8_t> &pixels) {
    @autoreleasepool {
      const auto begin = Clock::now();
      const auto buffer = [Queue commandBuffer];
      auto compute = [buffer computeCommandEncoder];
      [compute setComputePipelineState:Arguments];
      [compute setBuffer:arguments offset:0 atIndex:0];
      [compute setBytes:&settings length:sizeof(settings) atIndex:1];
      [compute dispatchThreads:MTLSizeMake(settings.Count, 1, 1)
          threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
      [compute endEncoding];
      if (mode >= 2) {
        compute = [buffer computeCommandEncoder];
        [compute setComputePipelineState:Commands];
        [compute setBuffer:container offset:0 atIndex:0];
        [compute setBuffer:arguments offset:0 atIndex:1];
        [compute setBuffer:Indices offset:0 atIndex:2];
        [compute setBytes:&settings length:sizeof(settings) atIndex:3];
        [compute useResource:icb usage:MTLResourceUsageWrite];
        [compute dispatchThreads:MTLSizeMake(settings.Count, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        [compute endEncoding];
      }
      if (mode == 3) {
        const auto blit = [buffer blitCommandEncoder];
        [blit optimizeIndirectCommandBuffer:icb withRange:NSMakeRange(0, settings.Count)];
        [blit endEncoding];
      }
      const auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture = Target;
      pass.colorAttachments[0].loadAction = MTLLoadActionClear;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
      const auto render = [buffer renderCommandEncoderWithDescriptor:pass];
      const uint32_t perSection = (settings.Count + sections - 1) / sections;
      for (uint32_t first = 0; first < settings.Count; first += perSection) {
        const uint32_t count = std::min(perSection, settings.Count - first);
        const uint32_t group = first / perSection;
        const std::array<float, 4> tint{
            group % 2 == 0 ? 1.0f : .5f, group % 3 == 0 ? 1.0f : .5f, 1.0f, 1.0f};
        [render setRenderPipelineState:Pipelines[mode == 0 ? 0 : 1]];
        [render setVertexBuffer:Vertices[group % 2] offset:0 atIndex:0];
        [render setFragmentBytes:tint.data() length:sizeof(tint) atIndex:1];
        if (mode >= 2) {
          [render useResource:Indices usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
          [render executeCommandsInBuffer:icb withRange:NSMakeRange(first, count)];
        } else {
          for (uint32_t draw = first; draw < first + count; ++draw) {
            [render drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexType:MTLIndexTypeUInt32
                              indexBuffer:Indices
                        indexBufferOffset:0
                           indirectBuffer:arguments
                     indirectBufferOffset:draw * sizeof(Args)];
          }
        }
      }
      [render endEncoding];
      double encodeMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
      [buffer commit];
      [buffer waitUntilCompleted];
      if (buffer.status != MTLCommandBufferStatusCompleted) {
        Fail(buffer.error.localizedDescription.UTF8String);
      }
      double gpuMs = 1000 * (buffer.GPUEndTime - buffer.GPUStartTime);
      Require(gpuMs > 0, "missing GPU timestamp");
      pixels.resize(size_t(kWidth) * kHeight * 4);
      [Target getBytes:pixels.data()
           bytesPerRow:size_t(kWidth) * 4
            fromRegion:MTLRegionMake2D(0, 0, kWidth, kHeight)
           mipmapLevel:0];
      return {{"count", settings.Count},
              {"sections", sections},
              {"phase", settings.Phase},
              {"mode", mode},
              {"encode_ms", encodeMs},
              {"gpu_ms", gpuMs}};
    }
  }

  static void CheckOutputs(const std::array<std::vector<uint8_t>, 4> &outputs, uint32_t phase) {
    for (size_t mode = 1; mode < outputs.size(); ++mode) {
      Require(outputs[0] == outputs[mode], "direct/ICB pixels differ");
    }
    if (phase == 2) {
      for (const uint8_t value : outputs[0]) {
        Require(value == 0, "empty commands retain stale colour");
      }
    } else {
      bool visible = false;
      for (size_t i = 3; i < outputs[0].size(); i += 4) { visible |= outputs[0][i] != 0; }
      Require(visible, "active commands produced no covered pixels");
    }
  }

  Json Run() {
    Json samples = Json::array();
    Json resources = Json::array();
    uint32_t controls = 0;
    for (const uint32_t count : {1u, 1024u, 8192u}) {
      const auto setupBegin = Clock::now();
      const auto arguments = [Device newBufferWithLength:count * sizeof(Args)
                                                 options:MTLResourceStorageModePrivate];
      const auto descriptor = [MTLIndirectCommandBufferDescriptor new];
      descriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
      descriptor.inheritBuffers = YES;
      descriptor.inheritPipelineState = YES;
      descriptor.maxVertexBufferBindCount = 0;
      descriptor.maxFragmentBufferBindCount = 0;
      const auto icb =
          [Device newIndirectCommandBufferWithDescriptor:descriptor
                                         maxCommandCount:count
                                                 options:MTLResourceStorageModePrivate];
      Require(icb != nil, "indirect commands unavailable");
      const auto container = [Device newBufferWithLength:Encoder.encodedLength
                                                 options:MTLResourceStorageModeShared];
      [Encoder setArgumentBuffer:container offset:0];
      [Encoder setIndirectCommandBuffer:icb atIndex:0];
      resources.push_back(
          {{"count", count},
           {"icb_allocated_bytes", icb.allocatedSize},
           {"argument_bytes", arguments.allocatedSize},
           {"container_bytes", container.allocatedSize},
           {"setup_ms",
            std::chrono::duration<double, std::milli>(Clock::now() - setupBegin).count()}});
      for (const uint32_t sections : {1u, 145u}) {
        for (uint32_t round = 0; round < 7; ++round) {
          for (uint32_t phase = 0; phase < 4; ++phase) {
            std::array<std::vector<uint8_t>, 4> outputs;
            for (uint32_t step = 0; step < 4; ++step) {
              const uint32_t mode = (step + round) % 4;
              auto sample = Sample(arguments,
                                   icb,
                                   container,
                                   {.Count = count, .Phase = phase},
                                   mode,
                                   sections,
                                   outputs[mode]);
              sample["round"] = round;
              samples.push_back(sample);
            }
            CheckOutputs(outputs, phase);
            ++controls;
          }
        }
      }
    }
    const Class kind = [Device class];
    if (kind == Nil) { Fail("device class unavailable"); }
    const char *const name = Device.name.UTF8String;
    const char *const className = NSStringFromClass(kind).UTF8String;
    if (name == nullptr || className == nullptr) { Fail("device metadata unavailable"); }
    return {{"device", std::string(name)},
            {"device_class", std::string(className)},
            {"resources", resources},
            {"width", kWidth},
            {"height", kHeight},
            {"pixel_controls", controls},
            {"samples", samples},
            {"scope",
             "synthetic native command overhead; GPU-generated arguments; excludes "
             "World/PBR/p99/residency"}};
  }
};

}

int main(int argc, const char *const *argv) {
  @autoreleasepool {
    Require(argc == 2 || (argc == 3 && std::string_view(argv[2]) == "--omit-reset"),
            "output json path and optional --omit-reset required");
    MetalIndirectProbe probe(argc == 3);
    auto result = probe.Run();
    std::ofstream file(argv[1]);
    file << result.dump(2) << '\n';
    Require(bool(file), "result write failed");
    std::println("controls={} samples={}",
                 result["pixel_controls"].get<uint32_t>(),
                 result["samples"].size());
    return 0;
  }
}
