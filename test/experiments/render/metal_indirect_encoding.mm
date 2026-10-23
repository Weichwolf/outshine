#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct Draw {
  uint32_t Indices, Instances, First;
  int32_t Base;
  uint32_t Instance;
};

static_assert(sizeof(Draw) == sizeof(MTLDrawIndexedPrimitivesIndirectArguments));
static_assert(offsetof(Draw, Indices) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexCount));
static_assert(offsetof(Draw, Instances) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, instanceCount));
static_assert(offsetof(Draw, First) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, indexStart));
static_assert(offsetof(Draw, Base) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseVertex));
static_assert(offsetof(Draw, Instance) ==
              offsetof(MTLDrawIndexedPrimitivesIndirectArguments, baseInstance));
constexpr size_t kCount = 8192, kStride = 32, kOffset = 64, kIndexOffset = 32;
constexpr size_t kWide = 1024, kHigh = 256;
using Clock = std::chrono::steady_clock;

double Ms(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void Require(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL %s\n", message);
    std::exit(1);
  }
}

NSString *const kShader = @R"metal(
#include <metal_stdlib>
using namespace metal;
struct VertexOut { float4 position [[position]]; float4 colour; };
vertex VertexOut vertices(uint v [[vertex_id]], uint instance [[instance_id]], device const float2 *points [[buffer(0)]]) {
  uint2 cell = uint2(instance & 255u, instance >> 8u);
  float2 local = points[v] * 0.7f + 0.15f;
  float2 p = (float2(cell) + local) / float2(256,64);
  return {float4(p*2-1,0.5f,1), float4(float((instance*17u)&255u)/255, float((instance*37u)&255u)/255, float((v*53u)&255u)/255,1)};
}
fragment float4 pixels(VertexOut in [[stage_in]]) { return in.colour; }
kernel void write_arguments(device uint *arguments [[buffer(0)]], constant uint4& params [[buffer(1)]], uint i [[thread_position_in_grid]]) {
 if(i>=params.x)return;
 uint mode=(i+params.y)%7u;
 device uint *one=arguments+params.z+i*params.w;
 one[0]=mode==0?0u:(mode==1?6u:3u);
 one[1]=mode==2?0u:(mode==3?2u:1u);
 one[2]=mode==1||mode==4?3u:0u;
 one[3]=as_type<uint>(mode==1||mode==4?-3:(mode==5?3:0));
 one[4]=i*2u;
}
struct Container { command_buffer commands [[id(0)]]; };
kernel void commands(constant Container& destination [[buffer(0)]],
                     device const uint *arguments [[buffer(1)]],
                     device const uchar *indices [[buffer(2)]],
                     constant uint4& params [[buffer(3)]],
                     uint i [[thread_position_in_grid]]) {
  if(i>=params.x) return;
  device const uint *one=arguments+params.y+i*params.z;
  render_command command(destination.commands,i);
  command.reset();
  if(one[0]==0||one[1]==0) return;
  if(params.w==2) command.draw_indexed_primitives(primitive_type::triangle,one[0],reinterpret_cast<device const ushort *>(indices)+one[2],one[1],as_type<int>(one[3]),one[4]);
  else command.draw_indexed_primitives(primitive_type::triangle,one[0],reinterpret_cast<device const uint *>(indices)+one[2],one[1],as_type<int>(one[3]),one[4]);
}
)metal";

void SaveImage(const std::vector<uint8_t> &pixels) {
  auto colour = CGColorSpaceCreateDeviceRGB();
  auto provider = CGDataProviderCreateWithData(nullptr, pixels.data(), pixels.size(), nullptr);
  auto image = CGImageCreate(kWide,
                             kHigh,
                             8,
                             32,
                             kWide * 4,
                             colour,
                             static_cast<CGBitmapInfo>(kCGImageAlphaLast) |
                                 static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big),
                             provider,
                             nullptr,
                             false,
                             kCGRenderingIntentDefault);
  NSURL *url = [NSURL fileURLWithPath:@"icb-witness.png"];
  auto target =
      CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
  Require(target != nullptr, "PNG destination");
  CGImageDestinationAddImage(target, image, nullptr);
  Require(CGImageDestinationFinalize(target), "PNG write");
  CFRelease(target);
  CGImageRelease(image);
  CGDataProviderRelease(provider);
  CGColorSpaceRelease(colour);
}

struct Lab {
  id<MTLDevice> device;
  id<MTLCommandQueue> queue;
  id<MTLRenderPipelineState> graphics;
  id<MTLComputePipelineState> compute, producer;
  bool GpuArguments;
  id<MTLBuffer> points, arguments, index16, index32, container, readback;
  id<MTLIndirectCommandBuffer> commands;
  id<MTLTexture> target;

  explicit Lab(bool gpuArguments = false) : GpuArguments(gpuArguments) {
    device = MTLCreateSystemDefaultDevice();
    Require(device != nil, "Metal device");
    queue = [device newCommandQueue];
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:kShader options:nil error:&error];
    if (!library) { std::fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); }
    Require(library != nil, "compile MSL");
    auto render = [MTLRenderPipelineDescriptor new];
    render.vertexFunction = [library newFunctionWithName:@"vertices"];
    render.fragmentFunction = [library newFunctionWithName:@"pixels"];
    render.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    render.supportIndirectCommandBuffers = YES;
    graphics = [device newRenderPipelineStateWithDescriptor:render error:&error];
    Require(graphics != nil, "render pipeline");
    auto function = [library newFunctionWithName:@"commands"];
    compute = [device newComputePipelineStateWithFunction:function error:&error];
    Require(compute != nil, "compute pipeline");
    producer =
        [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"write_arguments"]
                                              error:&error];
    Require(producer != nil, "argument producer");
    constexpr std::array<float, 12> vertices{0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1};
    points = [device newBufferWithBytes:vertices.data()
                                 length:sizeof(vertices)
                                options:MTLResourceStorageModeShared];
    arguments = [device newBufferWithLength:kOffset + kStride * kCount
                                    options:gpuArguments ? MTLResourceStorageModePrivate
                                                         : MTLResourceStorageModeShared];
    std::array<uint16_t, 25> small{};
    std::array<uint32_t, 17> large{};
    constexpr std::array<uint32_t, 9> indices{0, 1, 2, 3, 4, 5, 3, 5, 4};
    std::copy(indices.begin(), indices.end(), small.begin() + kIndexOffset / 2);
    std::copy(indices.begin(), indices.end(), large.begin() + kIndexOffset / 4);
    index16 = [device newBufferWithBytes:small.data()
                                  length:sizeof(small)
                                 options:MTLResourceStorageModeShared];
    index32 = [device newBufferWithBytes:large.data()
                                  length:sizeof(large)
                                 options:MTLResourceStorageModeShared];
    auto descriptor = [MTLIndirectCommandBufferDescriptor new];
    descriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
    descriptor.inheritPipelineState = YES;
    descriptor.inheritBuffers = YES;
    commands = [device newIndirectCommandBufferWithDescriptor:descriptor
                                              maxCommandCount:kCount
                                                      options:MTLResourceStorageModePrivate];
    Require(commands != nil, "ICB allocation");
    auto encoder = [function newArgumentEncoderWithBufferIndex:0];
    container = [device newBufferWithLength:encoder.encodedLength
                                    options:MTLResourceStorageModeShared];
    [encoder setArgumentBuffer:container offset:0];
    [encoder setIndirectCommandBuffer:commands atIndex:0];
    auto texture = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                      width:kWide
                                                                     height:kHigh
                                                                  mipmapped:NO];
    texture.usage = MTLTextureUsageRenderTarget;
    texture.storageMode = MTLStorageModePrivate;
    target = [device newTextureWithDescriptor:texture];
    readback = [device newBufferWithLength:kWide * kHigh * 4 options:MTLResourceStorageModeShared];
    std::printf("DEVICE %s ICB_BYTES %lu\n",
                device.name.UTF8String,
                static_cast<unsigned long>(commands.allocatedSize));
  }

  struct Sample {
    double Cpu, DrawCpu, Gpu;
    std::vector<uint8_t> Image;
  };

  Sample Frame(bool icb, size_t count, bool shortIndices, uint32_t frame) {
    if (!GpuArguments) { std::memset(arguments.contents, 0xa5, arguments.length); }
    for (size_t i = 0; !GpuArguments && i < count; ++i) {
      const uint32_t mode = (i + frame) % 7;
      Draw draw{.Indices = mode == 0 ? 0u : (mode == 1 ? 6u : 3u),
                .Instances = mode == 2 ? 0u : (mode == 3 ? 2u : 1u),
                .First = mode == 1 || mode == 4 ? 3u : 0u,
                .Base = mode == 1 || mode == 4 ? -3 : (mode == 5 ? 3 : 0),
                .Instance = static_cast<uint32_t>(i * 2)};
      std::memcpy(
          static_cast<uint8_t *>(arguments.contents) + kOffset + i * kStride, &draw, sizeof(draw));
    }
    id<MTLBuffer> indices = shortIndices ? index16 : index32;
    const auto began = Clock::now();
    auto buffer = [queue commandBuffer];
    if (GpuArguments) {
      auto fill = [buffer blitCommandEncoder];
      [fill fillBuffer:arguments range:NSMakeRange(0, arguments.length) value:0xa5];
      [fill endEncoding];
      auto generate = [buffer computeCommandEncoder];
      [generate setComputePipelineState:producer];
      [generate setBuffer:arguments offset:0 atIndex:0];
      const std::array<uint32_t, 4> params{
          static_cast<uint32_t>(count), frame, kOffset / 4, kStride / 4};
      [generate setBytes:params.data() length:sizeof(params) atIndex:1];
      if (count) {
        [generate dispatchThreads:MTLSizeMake(count, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
      }
      [generate endEncoding];
    }
    if (icb) {
      auto encoder = [buffer computeCommandEncoder];
      [encoder setComputePipelineState:compute];
      [encoder setBuffer:container offset:0 atIndex:0];
      [encoder setBuffer:arguments offset:0 atIndex:1];
      [encoder setBuffer:indices offset:kIndexOffset atIndex:2];
      const std::array<uint32_t, 4> params{
          static_cast<uint32_t>(count), kOffset / 4, kStride / 4, shortIndices ? 2u : 4u};
      [encoder setBytes:params.data() length:sizeof(params) atIndex:3];
      [encoder useResource:commands usage:MTLResourceUsageWrite];
      [encoder useResource:indices usage:MTLResourceUsageRead];
      if (count) {
        [encoder dispatchThreads:MTLSizeMake(count, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
      }
      [encoder endEncoding];
    }
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    auto encoder = [buffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:graphics];
    [encoder setVertexBuffer:points offset:0 atIndex:0];
    [encoder setCullMode:MTLCullModeNone];
    const auto drawBegan = Clock::now();
    if (icb) {
      [encoder useResource:indices usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [encoder useResource:points usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      if (count) { [encoder executeCommandsInBuffer:commands withRange:NSMakeRange(0, count)]; }
    } else {
      for (size_t i = 0; i < count; ++i) {
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                             indexType:shortIndices ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
                           indexBuffer:indices
                     indexBufferOffset:kIndexOffset
                        indirectBuffer:arguments
                  indirectBufferOffset:kOffset + i * kStride];
      }
    }
    const double drawMs = Ms(drawBegan);
    [encoder endEncoding];
    auto blit = [buffer blitCommandEncoder];
    [blit copyFromTexture:target
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(kWide, kHigh, 1)
                        toBuffer:readback
               destinationOffset:0
          destinationBytesPerRow:kWide * 4
        destinationBytesPerImage:kWide * kHigh * 4];
    [blit endEncoding];
    const double cpu = Ms(began);
    [buffer commit];
    [buffer waitUntilCompleted];
    if (buffer.status != MTLCommandBufferStatusCompleted) {
      std::fprintf(stderr, "%s\n", buffer.error.localizedDescription.UTF8String);
    }
    Require(buffer.status == MTLCommandBufferStatusCompleted, "GPU command completion");
    const auto *bytes = static_cast<const uint8_t *>(readback.contents);
    return {.Cpu = cpu,
            .DrawCpu = drawMs,
            .Gpu = (buffer.GPUEndTime - buffer.GPUStartTime) * 1000,
            .Image = std::vector<uint8_t>(bytes, bytes + kWide * kHigh * 4)};
  }
};

int main(int argc, const char **argv) {
  @autoreleasepool {
    Require(argc == 1 || (argc == 2 && std::strcmp(argv[1], "--gpu-arguments") == 0),
            "usage: lab [--gpu-arguments]");
    const bool gpu = argc > 1;
    Lab lab(gpu), reference;
    std::printf("ARGUMENTS %s\n", gpu ? "GPU_PRIVATE" : "CPU_SHARED");
    for (const size_t count : std::array<size_t, 6>{0, 1, 7, 128, 1024, kCount}) {
      for (const bool shortIndices : {false, true}) {
        std::vector<double> ordinary, indirect, ordinaryGpu, indirectGpu;
        for (uint32_t frame = 0; frame < 20; ++frame) {
          @autoreleasepool {
            Lab::Sample a, b;
            if ((frame & 1u) == 0) {
              a = lab.Frame(false, count, shortIndices, frame);
              b = lab.Frame(true, count, shortIndices, frame);
            } else {
              b = lab.Frame(true, count, shortIndices, frame);
              a = lab.Frame(false, count, shortIndices, frame);
            }
            Require(a.Image == b.Image, "exact ICB and ordinary indexed image");
            if (gpu) {
              Require(a.Image == reference.Frame(false, count, shortIndices, frame).Image,
                      "GPU arguments agree with independent CPU arguments");
            }
            if (count == kCount) {
              size_t lit = 0;
              for (size_t at = 0; at < a.Image.size(); at += 4) {
                lit += (a.Image[at] | a.Image[at + 1] | a.Image[at + 2]) != 0;
              }
              Require(lit > 1024, "nonempty rendered colour coverage");
            }
            if (frame >= 4) {
              ordinary.push_back(a.Cpu);
              indirect.push_back(b.Cpu);
              ordinaryGpu.push_back(a.Gpu);
              indirectGpu.push_back(b.Gpu);
            }
            if (count == kCount && shortIndices && frame == 19) { SaveImage(b.Image); }
          }
        }
        const auto median = [](std::vector<double> x) {
          std::sort(x.begin(), x.end());
          return (x[x.size() / 2 - 1] + x[x.size() / 2]) / 2;
        };
        std::printf("COUNT %zu INDEX %d FRAMES 20 EXACT 20 CPU_LOOP_MS %.6f "
                    "CPU_ICB_MS %.6f GPU_LOOP_MS %.6f GPU_ICB_MS %.6f\n",
                    count,
                    shortIndices ? 16 : 32,
                    median(ordinary),
                    median(indirect),
                    median(ordinaryGpu),
                    median(indirectGpu));
        std::fflush(stdout);
      }
    }
  }
}
