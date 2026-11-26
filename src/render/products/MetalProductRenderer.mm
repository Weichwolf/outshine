#include "MetalProductRenderer.h"
#include "Units.h"

#import <Metal/Metal.h>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>

namespace outshine::Render {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t kMatrixWords = 16;
constexpr auto kClearColor = std::array{0.18, 0.25, 0.34};
constexpr size_t kChannels = 4;
using ClipMatrix = std::array<float, kMatrixWords>;

static_assert(sizeof(SurfaceVertex) == 6 * sizeof(float) &&
              alignof(SurfaceVertex) == alignof(float) &&
              offsetof(SurfaceVertex, Normal) == 3 * sizeof(float));
static_assert(sizeof(ClipMatrix) == 64 && sizeof(uint32_t) == 4);

NSString *const kShader = @R"(
#include <metal_stdlib>
using namespace metal;
struct SurfaceVertex {
  packed_float3 position;
  packed_float3 normal;
};
struct Varyings {
  float4 position [[position]];
  float3 normal;
};
vertex Varyings tile_vertex(uint index [[vertex_id]],
                            const device SurfaceVertex *vertices [[buffer(0)]],
                            constant float4x4 &clip [[buffer(1)]]) {
  SurfaceVertex point = vertices[index];
  return {clip * float4(point.position, 1.0), point.normal};
}
fragment float4 tile_fragment(Varyings in [[stage_in]]) {
  float3 normal = normalize(in.normal);
  const float3 light = normalize(float3(-0.55, 0.65, 0.45));
  float diffuse = 0.35 + 0.65 * max(0.0, dot(normal, light));
  float3 reflectance = mix(float3(0.26, 0.22, 0.17), float3(0.13, 0.20, 0.10),
                           smoothstep(0.45, 0.92, normal.y));
  return float4(diffuse * reflectance, 1.0);
}
struct PanoramaUniform {
  float4 forward;
  float4 rightSpan;
  float4 upSpan;
  float4 display;
};
struct PanoramaRaster {
  float4 position [[position]];
  float2 ndc;
};
vertex PanoramaRaster panorama_vertex(uint index [[vertex_id]]) {
  constexpr float2 triangle[] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
  float2 ndc = triangle[index];
  return {float4(ndc, 0, 1), ndc};
}
struct PanoramaFragment {
  float4 colour [[color(0)]];
  float depth [[depth(any)]];
};
fragment PanoramaFragment panorama_fragment(PanoramaRaster in [[stage_in]],
                                             constant PanoramaUniform &view [[buffer(0)]],
                                             texturecube<half> colour [[texture(0)]],
                                             texturecube<float> distanceM [[texture(1)]]) {
  constexpr sampler linearColour(coord::normalized, filter::linear);
  constexpr sampler pointDepth(coord::normalized, filter::nearest);
  float3 ray = normalize(view.forward.xyz + in.ndc.x * view.rightSpan.xyz +
                         in.ndc.y * view.upSpan.xyz);
  float radius = distanceM.sample(pointDepth, ray).r;
  float forwardM = radius * dot(view.forward.xyz, ray);
  float depth = forwardM > 0.0 ? saturate(view.display.x / forwardM - view.display.y) : 0.0;
  float3 x = float3(colour.sample(linearColour, ray).rgb) * view.display.z;
  float3 filmic = saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
  return {float4(filmic, 1.0), depth};
}
)";

double ElapsedMs(Clock::time_point since) {
  return std::chrono::duration<double, std::milli>(Clock::now() - since).count();
}

std::expected<ClipMatrix, std::string> MetalClip(Mat4 clip) {
  ClipMatrix packed{};
  for (size_t column = 0; column < kMatrixWords; column += 4) {
    clip[column + 2] = (clip[column + 3] - clip[column + 2]) * 0.5;
  }
  for (size_t word = 0; word < packed.size(); ++word) {
    packed[word] = static_cast<float>(clip[word]);
    if (!std::isfinite(packed[word])) { return std::unexpected("nonfinite Metal clip matrix"); }
  }
  return packed;
}

MTLTextureDescriptor *Texture(MTLPixelFormat format, MTLStorageMode storage) {
  auto *const desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                         width:MetalProductFrameWidth
                                                        height:MetalProductFrameHeight
                                                     mipmapped:NO];
  desc.storageMode = storage;
  desc.usage = MTLTextureUsageRenderTarget;
  return desc;
}

struct ProductPass {
  id<MTLCommandQueue> Queue;
  id<MTLRenderPipelineState> Pipeline;
  id<MTLDepthStencilState> DepthState;
  MTLRenderPassDescriptor *Pass;
  id<MTLTexture> Color;
};

std::string MetalError(NSError *error, const char *fallback) {
  const char *const reason = error.localizedDescription.UTF8String;
  return reason != nullptr ? reason : fallback;
}

std::expected<ProductPass, std::string> MakePass(id<MTLDevice> device, bool panorama = false) {
  NSError *error = nil;
  const id<MTLLibrary> library = [device newLibraryWithSource:kShader options:nil error:&error];
  if (library == nil) {
    return std::unexpected(MetalError(error, "Metal failed to create a tile shader or pipeline"));
  }
  auto *const descriptor = [[MTLRenderPipelineDescriptor alloc] init];
  descriptor.vertexFunction =
      [library newFunctionWithName:panorama ? @"panorama_vertex" : @"tile_vertex"];
  descriptor.fragmentFunction =
      [library newFunctionWithName:panorama ? @"panorama_fragment" : @"tile_fragment"];
  descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm_sRGB;
  descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  const id<MTLRenderPipelineState> pipeline =
      [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
  if (pipeline == nil) {
    return std::unexpected(MetalError(error, "Metal failed to create a tile shader or pipeline"));
  }
  auto *const depthSpec = [[MTLDepthStencilDescriptor alloc] init];
  depthSpec.depthCompareFunction =
      panorama ? MTLCompareFunctionGreaterEqual : MTLCompareFunctionGreater;
  depthSpec.depthWriteEnabled = YES;
  auto *const pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = [device
      newTextureWithDescriptor:Texture(MTLPixelFormatRGBA8Unorm_sRGB, MTLStorageModeShared)];
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].clearColor =
      MTLClearColorMake(kClearColor[0], kClearColor[1], kClearColor[2], 1.0);
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  pass.depthAttachment.texture = [device
      newTextureWithDescriptor:Texture(MTLPixelFormatDepth32Float, MTLStorageModeMemoryless)];
  pass.depthAttachment.loadAction = MTLLoadActionClear;
  pass.depthAttachment.clearDepth = 0.0;
  pass.depthAttachment.storeAction = MTLStoreActionDontCare;
  ProductPass result{.Queue = [device newCommandQueue],
                     .Pipeline = pipeline,
                     .DepthState = [device newDepthStencilStateWithDescriptor:depthSpec],
                     .Pass = pass,
                     .Color = pass.colorAttachments[0].texture};
  if (result.Queue == nil || result.DepthState == nil || result.Color == nil ||
      pass.depthAttachment.texture == nil) {
    return std::unexpected("Metal could not allocate the tile pass");
  }
  return result;
}

struct TileMesh {
  id<MTLBuffer> Vertices;
  id<MTLBuffer> Indices;
  size_t IndexCount;
};

std::expected<id<MTLCommandBuffer>, std::string>
SubmitTile(const ProductPass &pass, const TileMesh &mesh, const ClipMatrix &clip) {
  @autoreleasepool {
    const id<MTLCommandBuffer> command = [pass.Queue commandBuffer];
    const id<MTLRenderCommandEncoder> draw = [command renderCommandEncoderWithDescriptor:pass.Pass];
    if (draw == nil) { return std::unexpected("Metal could not encode the tile frame"); }
    [draw setRenderPipelineState:pass.Pipeline];
    [draw setDepthStencilState:pass.DepthState];
    [draw setVertexBuffer:mesh.Vertices offset:0 atIndex:0];
    [draw setVertexBytes:clip.data() length:sizeof(clip) atIndex:1];
    [draw drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                     indexCount:mesh.IndexCount
                      indexType:MTLIndexTypeUInt32
                    indexBuffer:mesh.Indices
              indexBufferOffset:0];
    [draw endEncoding];
    [command commit];
    return command;
  }
}

std::expected<std::optional<double>, std::string> RetireTile(id<MTLCommandBuffer> command) {
  [command waitUntilCompleted];
  if (command.status != MTLCommandBufferStatusCompleted) {
    return std::unexpected("Metal tile command failed");
  }
  const double began = command.GPUStartTime;
  const double end = command.GPUEndTime;
  if (!std::isfinite(began) || began <= 0 || !std::isfinite(end) || end <= began) {
    return std::optional<double>{};
  }
  return (end - began) * kMsPerS;
}

template <typename Submit>
std::expected<void, std::string>
MeasureCapture(size_t count, const Submit &submitAt, MetalProductFrames &frames) {
  std::array<id<MTLCommandBuffer>, MetalProductFramesInFlight> pending{};
  frames.Samples.resize(count);
  const auto finish = [&](size_t frame) -> std::expected<void, std::string> {
    auto &command = pending[frame % pending.size()];
    const auto elapsed = RetireTile(command);
    if (!elapsed) { return std::unexpected(elapsed.error()); }
    frames.Samples[frame].GpuMs = *elapsed;
    command = nil;
    return {};
  };
  const auto windowAt = Clock::now();
  for (size_t frame = 0; frame < count; ++frame) {
    const auto frameAt = Clock::now();
    auto &sample = frames.Samples[frame];
    const auto renderAt = Clock::now();
    if (frame >= pending.size()) {
      const auto retired = finish(frame - pending.size());
      if (!retired) { return retired; }
      sample.SyncWaitMs = ElapsedMs(renderAt);
    }
    const auto submitted = submitAt(frame);
    if (!submitted) { return std::unexpected(submitted.error()); }
    pending[frame % pending.size()] = *submitted;
    sample.RenderMs = ElapsedMs(renderAt);
    sample.WallMs = ElapsedMs(frameAt);
  }
  frames.WindowMs = ElapsedMs(windowAt);
  const auto flushAt = Clock::now();
  for (size_t frame = count > pending.size() ? count - pending.size() : 0; frame < count; ++frame) {
    if (const auto retired = finish(frame); !retired) { return retired; }
  }
  frames.FlushMs = ElapsedMs(flushAt);
  return {};
}

void FinishCapture(id<MTLDevice> device,
                   const ProductPass &pass,
                   MetalProductFrames &frames,
                   bool image) {
  frames.DeviceId = device.registryID;
  frames.ResourceBytes = device.currentAllocatedSize;
  frames.RecommendedResourceBytes = device.recommendedMaxWorkingSetSize;
  frames.UnifiedMemory = device.hasUnifiedMemory;
  if (image) {
    const size_t row = MetalProductFrameWidth * kChannels;
    frames.Rgba.resize(row * MetalProductFrameHeight);
    [pass.Color getBytes:frames.Rgba.data()
             bytesPerRow:row
              fromRegion:MTLRegionMake2D(0, 0, MetalProductFrameWidth, MetalProductFrameHeight)
             mipmapLevel:0];
  }
}

id<MTLTexture> UploadCube(id<MTLDevice> device,
                          MTLPixelFormat format,
                          size_t side,
                          size_t pixelBytes,
                          const std::string &pixels) {
  auto *const desc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:format
                                                                           size:side
                                                                      mipmapped:NO];
  desc.storageMode = MTLStorageModeShared;
  desc.usage = MTLTextureUsageShaderRead;
  const id<MTLTexture> cube = [device newTextureWithDescriptor:desc];
  if (cube == nil) { return nil; }
  const size_t rowBytes = side * pixelBytes;
  const size_t sliceBytes = rowBytes * side;
  for (size_t face = 0; face < FarPanorama::FaceCount; ++face) {
    [cube replaceRegion:MTLRegionMake2D(0, 0, side, side)
            mipmapLevel:0
                  slice:face
              withBytes:pixels.data() + face * sliceBytes
            bytesPerRow:rowBytes
          bytesPerImage:sliceBytes];
  }
  return cube;
}

struct PanoramaTextures {
  id<MTLTexture> Colour;
  id<MTLTexture> Depth;
};

using PanoramaUniform = std::array<std::array<float, 4>, 4>;
static_assert(sizeof(PanoramaUniform) == sizeof(ClipMatrix));

PanoramaUniform PackView(const PanoramaView &view) {
  const auto vector = [](const Vec3 &axis) {
    return std::array{static_cast<float>(axis[0]),
                      static_cast<float>(axis[1]),
                      static_cast<float>(axis[2]),
                      0.0f};
  };
  return {vector(view.Forward),
          vector(view.RightSpan),
          vector(view.UpSpan),
          {static_cast<float>(view.DepthScaleM),
           static_cast<float>(view.DepthBias),
           static_cast<float>(view.Exposure),
           0.0f}};
}

std::expected<id<MTLCommandBuffer>, std::string> SubmitPanorama(const ProductPass &pass,
                                                                const PanoramaTextures &textures,
                                                                const PanoramaView &view) {
  @autoreleasepool {
    const id<MTLCommandBuffer> command = [pass.Queue commandBuffer];
    const id<MTLRenderCommandEncoder> draw = [command renderCommandEncoderWithDescriptor:pass.Pass];
    if (draw == nil) { return std::unexpected("Metal could not encode the panorama frame"); }
    const auto packed = PackView(view);
    [draw setRenderPipelineState:pass.Pipeline];
    [draw setDepthStencilState:pass.DepthState];
    [draw setFragmentTexture:textures.Colour atIndex:0];
    [draw setFragmentTexture:textures.Depth atIndex:1];
    [draw setFragmentBytes:packed.data() length:sizeof(packed) atIndex:0];
    [draw drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [draw endEncoding];
    [command commit];
    return command;
  }
}

}

std::expected<MetalProductFrames, std::string>
RenderMetalTile(const SurfaceTile &tile, size_t count, const TileClipAt &clipAt, bool readImage) {
  @autoreleasepool {
    const auto began = Clock::now();
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return std::unexpected("Metal device unavailable"); }
    const auto pass = MakePass(device);
    if (!pass) { return std::unexpected(pass.error()); }
    MetalProductFrames frames;
    const char *const name = device.name.UTF8String;
    frames.Device = name != nullptr ? name : "Metal";
    frames.SetupMs = ElapsedMs(began);
    const auto upload = Clock::now();
    const auto vertexBytes = std::as_bytes(std::span(tile.Vertices));
    const auto indexBytes = std::as_bytes(std::span(tile.Indices));
    const TileMesh mesh{.Vertices = [device newBufferWithBytes:vertexBytes.data()
                                                        length:vertexBytes.size()
                                                       options:MTLResourceStorageModeShared],
                        .Indices = [device newBufferWithBytes:indexBytes.data()
                                                       length:indexBytes.size()
                                                      options:MTLResourceStorageModeShared],
                        .IndexCount = tile.Indices.size()};
    if (mesh.Vertices == nil || mesh.Indices == nil) {
      return std::unexpected("Metal tile upload failed");
    }
    frames.UploadMs = ElapsedMs(upload);
    frames.UploadedBytes = vertexBytes.size() + indexBytes.size();
    const auto submit = [&](size_t frame) -> std::expected<id<MTLCommandBuffer>, std::string> {
      const auto matrix = clipAt(frame);
      if (!matrix) { return std::unexpected(matrix.error()); }
      const auto clip = MetalClip(*matrix);
      if (!clip) { return std::unexpected(clip.error()); }
      return SubmitTile(*pass, mesh, *clip);
    };
    if (const auto measured = MeasureCapture(count, submit, frames); !measured) {
      return std::unexpected(measured.error());
    }
    FinishCapture(device, *pass, frames, readImage);
    return frames;
  }
}

std::expected<MetalProductFrames, std::string> RenderMetalPanorama(const FarPanorama &panorama,
                                                                   size_t count,
                                                                   const PanoramaViewAt &viewAt,
                                                                   bool readImage) {
  @autoreleasepool {
    const auto began = Clock::now();
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return std::unexpected("Metal device unavailable"); }
    const auto pass = MakePass(device, true);
    if (!pass) { return std::unexpected(pass.error()); }
    MetalProductFrames frames;
    const char *const name = device.name.UTF8String;
    frames.Device = name != nullptr ? name : "Metal";
    frames.SetupMs = ElapsedMs(began);
    const auto upload = Clock::now();
    const PanoramaTextures textures{.Colour = UploadCube(device,
                                                         MTLPixelFormatRGBA16Float,
                                                         panorama.Side,
                                                         FarPanorama::ColourBytesPerTexel,
                                                         panorama.Colour),
                                    .Depth = UploadCube(device,
                                                        MTLPixelFormatR32Float,
                                                        panorama.Side,
                                                        FarPanorama::DepthBytesPerTexel,
                                                        panorama.Depth)};
    if (textures.Colour == nil || textures.Depth == nil) {
      return std::unexpected("Metal panorama upload failed");
    }
    frames.UploadMs = ElapsedMs(upload);
    frames.UploadedBytes = panorama.Colour.size() + panorama.Depth.size();
    const auto submit = [&](size_t frame) -> std::expected<id<MTLCommandBuffer>, std::string> {
      const auto view = viewAt(frame);
      if (!view) { return std::unexpected(view.error()); }
      return SubmitPanorama(*pass, textures, *view);
    };
    if (const auto measured = MeasureCapture(count, submit, frames); !measured) {
      return std::unexpected(measured.error());
    }
    FinishCapture(device, *pass, frames, readImage);
    return frames;
  }
}
}
