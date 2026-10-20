#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <print>
#include <span>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::byte>;
using Clock = std::chrono::steady_clock;
constexpr uint32_t kWidthPx = 1280, kHeightPx = 720, kPrepareLanes = 64, kPrefixLanes = 256,
                   kMaxRounds = 1000;
constexpr size_t kDrawBytes = 20, kFrameBytes = 80;

struct DrawArguments {
  uint32_t Count, Instances, First;
  int32_t Base;
  uint32_t Model;
};

static_assert(sizeof(DrawArguments) == kDrawBytes && alignof(DrawArguments) == 4 &&
              offsetof(DrawArguments, Count) == 0 && offsetof(DrawArguments, Instances) == 4 &&
              offsetof(DrawArguments, First) == 8 && offsetof(DrawArguments, Base) == 12 &&
              offsetof(DrawArguments, Model) == 16);

struct alignas(16) FrameLayout {
  std::array<float, 16> Clip;
  std::array<float, 4> Shift;
};

static_assert(sizeof(FrameLayout) == kFrameBytes && alignof(FrameLayout) == 16 &&
              offsetof(FrameLayout, Clip) == 0 && offsetof(FrameLayout, Shift) == 64);
static_assert(sizeof(MTLIndirectCommandBufferExecutionRange) == 8 &&
              offsetof(MTLIndirectCommandBufferExecutionRange, location) == 0 &&
              offsetof(MTLIndirectCommandBufferExecutionRange, length) == 4);
enum class Path : uint32_t { Legacy, Compatible, SparseBuild, SparseReuse, DenseBuild, DenseReuse };
constexpr std::array kPaths{Path::Legacy,
                            Path::Compatible,
                            Path::SparseBuild,
                            Path::SparseReuse,
                            Path::DenseBuild,
                            Path::DenseReuse};

[[noreturn]] void Fail(const std::string &message) {
  std::println(stderr, "{}", message);
  std::_Exit(EXIT_FAILURE);
}

std::string Error(NSError *error, const char *fallback) {
  const auto *message = error.localizedDescription.UTF8String;
  return message != nullptr ? message : fallback;
}

Bytes Read(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) { Fail(path); }
  const auto size = file.tellg();
  if (size < 0) { Fail(path); }
  Bytes data(static_cast<size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));
  if (!file) { Fail(path); }
  return data;
}

void Finish(id<MTLCommandBuffer> command) {
  [command commit];
  [command waitUntilCompleted];
  if (command.status != MTLCommandBufferStatusCompleted) {
    Fail(Error(command.error, "Metal command failed"));
  }
}

id<MTLBuffer> Upload(id<MTLDevice> device, id<MTLCommandQueue> queue, const Bytes &bytes) {
  const auto staging = [device newBufferWithBytes:bytes.data()
                                           length:bytes.size()
                                          options:MTLResourceStorageModeShared];
  const auto result = [device newBufferWithLength:bytes.size()
                                          options:MTLResourceStorageModePrivate];
  if (staging == nil || result == nil) { Fail("buffer allocation failed"); }
  const auto command = [queue commandBuffer];
  const auto copy = [command blitCommandEncoder];
  [copy copyFromBuffer:staging
           sourceOffset:0
               toBuffer:result
      destinationOffset:0
                   size:bytes.size()];
  [copy endEncoding];
  Finish(command);
  return result;
}

struct Probe {
  id<MTLDevice> Device;
  id<MTLCommandQueue> Queue;
  id<MTLBuffer> Arguments, OriginalArguments, Indices, Positions, Models, ArgumentTable;
  id<MTLIndirectCommandBuffer> Commands, DenseCommands;
  id<MTLBuffer> DenseTable, Slots, ExecutionRange;
  id<MTLComputePipelineState> Prepare, Clear, Prefix, Dense;
  id<MTLRenderPipelineState> Plain, Compatible;
  id<MTLDepthStencilState> DepthState;
  id<MTLTexture> Depth;
  Bytes Frame;
  uint32_t Rows = 0;
};

id<MTLFunction> _Nonnull Function(id<MTLLibrary> library, NSString *name) {
  const auto function = [library newFunctionWithName:name];
  if (function == nil) { Fail("Metal function missing: " + std::string(name.UTF8String)); }
  return function;
}

id<MTLRenderPipelineState> Pipeline(id<MTLDevice> device, id<MTLFunction> vertex, bool indirect) {
  const auto descriptor = [MTLRenderPipelineDescriptor new];
  descriptor.vertexFunction = vertex;
  descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  descriptor.supportIndirectCommandBuffers = indirect;
  NSError *error = nil;
  const auto pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
  if (pipeline == nil) { Fail(Error(error, "graphics pipeline failed")); }
  return pipeline;
}

Probe Prepare(const std::string &folder, const char *shader) {
  Probe p;
  p.Device = MTLCreateSystemDefaultDevice();
  if (p.Device == nil) { Fail("no Metal device"); }
  p.Queue = [p.Device newCommandQueue];
  const auto input = Read(folder + "/input.json");
  const auto manifest =
      nlohmann::json::parse(reinterpret_cast<const char *>(input.data()),
                            reinterpret_cast<const char *>(input.data() + input.size()),
                            nullptr,
                            false);
  if (manifest.is_discarded()) { Fail("invalid input manifest"); }
  p.Rows = manifest["rows"].get<uint32_t>();
  p.Positions = Upload(p.Device, p.Queue, Read(manifest["vertices_path"].get<std::string>()));
  p.Indices = Upload(p.Device, p.Queue, Read(folder + "/draw-indices.bin"));
  p.Models = Upload(p.Device, p.Queue, Read(folder + "/models.bin"));
  p.Frame = Read(folder + "/frame.bin");
  if (p.Frame.size() != sizeof(FrameLayout)) { Fail("frame layout"); }
  const auto arguments = Read(folder + "/arguments.bin");
  if (arguments.size() != p.Rows * sizeof(DrawArguments)) { Fail("argument layout"); }
  p.Arguments = Upload(p.Device, p.Queue, arguments);
  p.OriginalArguments = Upload(p.Device, p.Queue, arguments);
  const auto source = Read(shader);
  NSString *const text = [[NSString alloc] initWithBytes:source.data()
                                                  length:source.size()
                                                encoding:NSUTF8StringEncoding];
  NSError *error = nil;
  const auto library = [p.Device newLibraryWithSource:text options:nil error:&error];
  if (library == nil) { Fail(Error(error, "Metal shader compilation failed")); }
  const auto prepare = Function(library, @"prepareCommands");
  p.Prepare = [p.Device newComputePipelineStateWithFunction:prepare error:&error];
  if (p.Prepare == nil) { Fail(Error(error, "compute pipeline failed")); }
  p.Clear = [p.Device newComputePipelineStateWithFunction:Function(library, @"clearCounts")
                                                    error:&error];
  if (p.Clear == nil) { Fail(Error(error, "clear pipeline failed")); }
  const auto dense = Function(library, @"prepareDenseCommands");
  p.Dense = [p.Device newComputePipelineStateWithFunction:dense error:&error];
  if (p.Dense == nil) { Fail(Error(error, "dense pipeline failed")); }
  p.Prefix = [p.Device newComputePipelineStateWithFunction:Function(library, @"commandPrefix")
                                                     error:&error];
  if (p.Prefix == nil) { Fail(Error(error, "prefix pipeline failed")); }
  if (p.Prefix.maxTotalThreadsPerThreadgroup < kPrefixLanes || p.Prefix.threadExecutionWidth < 8 ||
      p.Prefix.threadExecutionWidth > kPrefixLanes ||
      kPrefixLanes % p.Prefix.threadExecutionWidth != 0) {
    Fail("unsupported prefix group shape");
  }
  const auto vertex = Function(library, @"positionOnly");
  p.Plain = Pipeline(p.Device, vertex, false);
  p.Compatible = Pipeline(p.Device, vertex, true);
  const auto descriptor = [MTLIndirectCommandBufferDescriptor new];
  descriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
  descriptor.inheritBuffers = YES;
  descriptor.inheritPipelineState = YES;
  descriptor.maxVertexBufferBindCount = 0;
  descriptor.maxFragmentBufferBindCount = 0;
  p.Commands = [p.Device newIndirectCommandBufferWithDescriptor:descriptor
                                                maxCommandCount:p.Rows
                                                        options:MTLResourceStorageModePrivate];
  if (p.Commands == nil) { Fail("indirect command buffer allocation failed"); }
  const auto encoder = [prepare newArgumentEncoderWithBufferIndex:2];
  p.ArgumentTable = [p.Device newBufferWithLength:encoder.encodedLength
                                          options:MTLResourceStorageModeShared];
  [encoder setArgumentBuffer:p.ArgumentTable offset:0];
  [encoder setIndirectCommandBuffer:p.Commands atIndex:0];
  p.DenseCommands = [p.Device newIndirectCommandBufferWithDescriptor:descriptor
                                                     maxCommandCount:p.Rows
                                                             options:MTLResourceStorageModePrivate];
  if (p.DenseCommands == nil) { Fail("dense command allocation failed"); }
  const auto denseEncoder = [dense newArgumentEncoderWithBufferIndex:2];
  p.DenseTable = [p.Device newBufferWithLength:denseEncoder.encodedLength
                                       options:MTLResourceStorageModeShared];
  [denseEncoder setArgumentBuffer:p.DenseTable offset:0];
  [denseEncoder setIndirectCommandBuffer:p.DenseCommands atIndex:0];
  p.Slots = [p.Device newBufferWithLength:uint64_t{p.Rows} * sizeof(uint32_t)
                                  options:MTLResourceStorageModePrivate];
  p.ExecutionRange = [p.Device newBufferWithLength:8 options:MTLResourceStorageModePrivate];
  const auto depthDescriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                         width:kWidthPx
                                                        height:kHeightPx
                                                     mipmapped:NO];
  depthDescriptor.storageMode = MTLStorageModePrivate;
  depthDescriptor.usage = MTLTextureUsageRenderTarget;
  p.Depth = [p.Device newTextureWithDescriptor:depthDescriptor];
  if (p.Depth == nil) { Fail("depth texture allocation failed"); }
  const auto state = [MTLDepthStencilDescriptor new];
  state.depthCompareFunction = MTLCompareFunctionGreaterEqual;
  state.depthWriteEnabled = YES;
  p.DepthState = [p.Device newDepthStencilStateWithDescriptor:state];
  return p;
}

void EncodePrepare(const Probe &p, id<MTLCommandBuffer> command, bool clear, uint32_t mask = 0) {
  const auto encoder = [command computeCommandEncoder];
  [encoder setComputePipelineState:clear ? p.Clear : p.Prepare];
  [encoder setBuffer:p.Arguments offset:0 atIndex:0];
  if (clear) {
    [encoder setBytes:&p.Rows length:sizeof(p.Rows) atIndex:1];
    [encoder setBytes:&mask length:sizeof(mask) atIndex:2];
  } else {
    [encoder setBuffer:p.Indices offset:0 atIndex:1];
    [encoder setBuffer:p.ArgumentTable offset:0 atIndex:2];
    [encoder setBytes:&p.Rows length:sizeof(p.Rows) atIndex:3];
    [encoder useResource:p.Commands usage:MTLResourceUsageWrite];
  }
  [encoder dispatchThreadgroups:MTLSizeMake((p.Rows + kPrepareLanes - 1) / kPrepareLanes, 1, 1)
          threadsPerThreadgroup:MTLSizeMake(kPrepareLanes, 1, 1)];
  [encoder endEncoding];
}

void EncodeDense(const Probe &p, id<MTLCommandBuffer> command) {
  const auto prefix = [command computeCommandEncoder];
  [prefix setComputePipelineState:p.Prefix];
  [prefix setBuffer:p.Arguments offset:0 atIndex:0];
  [prefix setBuffer:p.Slots offset:0 atIndex:1];
  [prefix setBuffer:p.ExecutionRange offset:0 atIndex:2];
  [prefix setBytes:&p.Rows length:sizeof(p.Rows) atIndex:3];
  [prefix dispatchThreadgroups:MTLSizeMake(1, 1, 1)
         threadsPerThreadgroup:MTLSizeMake(kPrefixLanes, 1, 1)];
  [prefix endEncoding];
  const auto encoder = [command computeCommandEncoder];
  [encoder setComputePipelineState:p.Dense];
  [encoder setBuffer:p.Arguments offset:0 atIndex:0];
  [encoder setBuffer:p.Indices offset:0 atIndex:1];
  [encoder setBuffer:p.DenseTable offset:0 atIndex:2];
  [encoder setBytes:&p.Rows length:sizeof(p.Rows) atIndex:3];
  [encoder setBuffer:p.Slots offset:0 atIndex:4];
  [encoder useResource:p.DenseCommands usage:MTLResourceUsageWrite];
  [encoder dispatchThreadgroups:MTLSizeMake((p.Rows + kPrepareLanes - 1) / kPrepareLanes, 1, 1)
          threadsPerThreadgroup:MTLSizeMake(kPrepareLanes, 1, 1)];
  [encoder endEncoding];
}

void EncodeRaster(const Probe &p, id<MTLCommandBuffer> command, Path mode) {
  const auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.depthAttachment.texture = p.Depth;
  pass.depthAttachment.loadAction = MTLLoadActionClear;
  pass.depthAttachment.storeAction = MTLStoreActionStore;
  pass.depthAttachment.clearDepth = 0;
  const auto encoder = [command renderCommandEncoderWithDescriptor:pass];
  [encoder setRenderPipelineState:mode == Path::Legacy ? p.Plain : p.Compatible];
  [encoder setDepthStencilState:p.DepthState];
  [encoder setCullMode:MTLCullModeNone];
  [encoder setViewport:MTLViewport{.originX = 0,
                                   .originY = 0,
                                   .width = kWidthPx,
                                   .height = kHeightPx,
                                   .znear = 0,
                                   .zfar = 1}];
  [encoder setScissorRect:MTLScissorRect{.x = 0, .y = 0, .width = kWidthPx, .height = kHeightPx}];
  [encoder setVertexBuffer:p.Positions offset:0 atIndex:0];
  [encoder setVertexBuffer:p.Models offset:0 atIndex:1];
  [encoder setVertexBytes:p.Frame.data() length:p.Frame.size() atIndex:2];
  if (mode == Path::Legacy || mode == Path::Compatible) {
    for (uint32_t row = 0; row < p.Rows; ++row) {
      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                           indexType:MTLIndexTypeUInt32
                         indexBuffer:p.Indices
                   indexBufferOffset:0
                      indirectBuffer:p.Arguments
                indirectBufferOffset:row * sizeof(DrawArguments)];
    }
  } else {
    [encoder useResource:p.Indices usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
    if (mode == Path::SparseBuild || mode == Path::SparseReuse) {
      [encoder executeCommandsInBuffer:p.Commands withRange:NSMakeRange(0, p.Rows)];
    } else {
      [encoder executeCommandsInBuffer:p.DenseCommands
                        indirectBuffer:p.ExecutionRange
                  indirectBufferOffset:0];
    }
  }
  [encoder endEncoding];
}

Bytes Depth(const Probe &p) {
  constexpr size_t size = uint64_t{kWidthPx} * kHeightPx * sizeof(float);
  const auto target = [p.Device newBufferWithLength:size options:MTLResourceStorageModeShared];
  if (target == nil) { Fail("depth readback allocation failed"); }
  const auto command = [p.Queue commandBuffer];
  const auto copy = [command blitCommandEncoder];
  [copy copyFromTexture:p.Depth
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(kWidthPx, kHeightPx, 1)
                      toBuffer:target
             destinationOffset:0
        destinationBytesPerRow:uint64_t{kWidthPx} * sizeof(float)
      destinationBytesPerImage:size];
  [copy endEncoding];
  Finish(command);
  Bytes result(size);
  std::memcpy(result.data(), target.contents, size);
  return result;
}

Bytes BufferPixels(const Probe &p, id<MTLBuffer> source) {
  const auto target = [p.Device newBufferWithLength:source.length
                                            options:MTLResourceStorageModeShared];
  if (target == nil) { Fail("buffer readback allocation failed"); }
  const auto command = [p.Queue commandBuffer];
  const auto copy = [command blitCommandEncoder];
  [copy copyFromBuffer:source
           sourceOffset:0
               toBuffer:target
      destinationOffset:0
                   size:source.length];
  [copy endEncoding];
  Finish(command);
  Bytes out(source.length);
  std::memcpy(out.data(), target.contents, out.size());
  return out;
}

void CheckPrefix(const Probe &p) {
  const auto raw = BufferPixels(p, p.Arguments);
  const auto slots = BufferPixels(p, p.Slots);
  const auto range = BufferPixels(p, p.ExecutionRange);
  const auto *args = reinterpret_cast<const DrawArguments *>(raw.data());
  const auto *prefix = reinterpret_cast<const uint32_t *>(slots.data());
  uint32_t carry = 0;
  for (uint32_t row = 0; row < p.Rows; ++row) {
    if (prefix[row] != carry) { Fail("GPU dense order/prefix differs"); }
    if (args[row].Count > 0 && args[row].Instances > 0) { ++carry; }
  }
  const auto *span = reinterpret_cast<const uint32_t *>(range.data());
  if (span[0] != 0 || span[1] != carry) { Fail("GPU execution range differs"); }
}

nlohmann::json Sample(const Probe &p, Path mode) {
  const auto started = Clock::now();
  const auto command = [p.Queue commandBuffer];
  if (mode == Path::SparseBuild) { EncodePrepare(p, command, false); }
  if (mode == Path::DenseBuild) { EncodeDense(p, command); }
  EncodeRaster(p, command, mode);
  [command commit];
  const auto encoded = Clock::now();
  [command waitUntilCompleted];
  if (command.status != MTLCommandBufferStatusCompleted) { Fail("sample did not complete"); }
  const double elapsed = (command.GPUEndTime - command.GPUStartTime) * 1e9;
  if (!(elapsed > 0)) { Fail("GPU timing missing"); }
  return {{"cpu_encode_ns",
           std::chrono::duration_cast<std::chrono::nanoseconds>(encoded - started).count()},
          {"gpu_ns", elapsed}};
}

Bytes VerifyPaths(const Probe &p, nlohmann::json &report) {
  Bytes baseline;
  for (const auto mode : kPaths) {
    const auto draw = [p.Queue commandBuffer];
    if (mode == Path::SparseBuild) { EncodePrepare(p, draw, false); }
    if (mode == Path::DenseBuild) { EncodeDense(p, draw); }
    EncodeRaster(p, draw, mode);
    Finish(draw);
    const auto pixels = Depth(p);
    if (mode == Path::Legacy) {
      baseline = pixels;
    } else if (pixels != baseline) {
      Fail("depth differs between command paths");
    }
    report["modes"][static_cast<size_t>(mode)]["depth_exact"] = true;
  }
  const auto nonzero =
      std::count_if(reinterpret_cast<const float *>(baseline.data()),
                    reinterpret_cast<const float *>(baseline.data() + baseline.size()),
                    [](float value) { return value > 0; });
  if (nonzero == 0) { Fail("empty baseline depth"); }
  report["visible_depth_pixels"] = nonzero;
  return baseline;
}

void VerifyChanges(const Probe &p,
                   const Bytes &baseline,
                   nlohmann::json &report,
                   const std::string &folder) {
  id<MTLCommandBuffer> command = nil;
  command = [p.Queue commandBuffer];
  EncodePrepare(p, command, true);
  EncodeRaster(p, command, Path::SparseReuse);
  Finish(command);
  if (Depth(p) != baseline) { Fail("stale-command negative control"); }
  command = [p.Queue commandBuffer];
  EncodePrepare(p, command, false);
  EncodeRaster(p, command, Path::SparseReuse);
  Finish(command);
  const auto cleared = Depth(p);
  if (std::ranges::any_of(cleared, [](std::byte value) { return value != std::byte{}; })) {
    Fail("cleared GPU counts retained commands");
  }
  command = [p.Queue commandBuffer];
  const auto copy = [command blitCommandEncoder];
  [copy copyFromBuffer:p.OriginalArguments
           sourceOffset:0
               toBuffer:p.Arguments
      destinationOffset:0
                   size:p.Arguments.length];
  [copy endEncoding];
  EncodePrepare(p, command, false);
  EncodeRaster(p, command, Path::SparseReuse);
  Finish(command);
  if (Depth(p) != baseline) { Fail("GPU count restore did not restore commands"); }
  report["gpu_count_change_reset_restore"] = true;
  command = [p.Queue commandBuffer];
  EncodePrepare(p, command, true);
  EncodeRaster(p, command, Path::DenseReuse);
  Finish(command);
  if (Depth(p) != baseline) { Fail("dense stale-command negative control"); }
  command = [p.Queue commandBuffer];
  EncodeDense(p, command);
  EncodeRaster(p, command, Path::DenseReuse);
  Finish(command);
  if (std::ranges::any_of(Depth(p), [](std::byte value) { return value != std::byte{}; })) {
    Fail("dense zero counts retained commands");
  }
  command = [p.Queue commandBuffer];
  const auto restore = [command blitCommandEncoder];
  [restore copyFromBuffer:p.OriginalArguments
             sourceOffset:0
                 toBuffer:p.Arguments
        destinationOffset:0
                     size:p.Arguments.length];
  [restore endEncoding];
  EncodeDense(p, command);
  EncodeRaster(p, command, Path::DenseReuse);
  Finish(command);
  if (Depth(p) != baseline) { Fail("dense restore changed depth"); }
  report["dense_gpu_count_change_reset_restore"] = true;
  CheckPrefix(p);
  command = [p.Queue commandBuffer];
  EncodePrepare(p, command, true, 1);
  EncodeRaster(p, command, Path::Legacy);
  Finish(command);
  const auto partial = Depth(p);
  if (partial == baseline ||
      std::ranges::all_of(partial, [](std::byte value) { return value == std::byte{}; })) {
    Fail("partial-count control is vacuous");
  }
  command = [p.Queue commandBuffer];
  EncodePrepare(p, command, false);
  EncodeRaster(p, command, Path::SparseReuse);
  Finish(command);
  if (Depth(p) != partial) { Fail("sparse partial-count update differs"); }
  command = [p.Queue commandBuffer];
  EncodeDense(p, command);
  EncodeRaster(p, command, Path::DenseReuse);
  Finish(command);
  if (Depth(p) != partial) { Fail("dense partial-count update differs"); }
  CheckPrefix(p);
  report["partial_count_depth_order_exact"] = true;
  std::ofstream fullFile(folder + "/baseline-depth.bin", std::ios::binary);
  fullFile.write(reinterpret_cast<const char *>(baseline.data()),
                 static_cast<std::streamsize>(baseline.size()));
  std::ofstream partialFile(folder + "/partial-depth.bin", std::ios::binary);
  partialFile.write(reinterpret_cast<const char *>(partial.data()),
                    static_cast<std::streamsize>(partial.size()));
  if (!fullFile || !partialFile) { Fail("depth control write failed"); }
}
}

int main(int argc, char *const *argv) {
  if (argc != 5) { Fail("metal-indirect <inputs> <shader.metal> <rounds> <report.json>"); }
  @autoreleasepool {
    const auto p = Prepare(argv[1], argv[2]);
    const auto rounds = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
    if (rounds == 0 || rounds > kMaxRounds) { Fail("invalid rounds"); }
    const auto command = [p.Queue commandBuffer];
    EncodePrepare(p, command, false);
    EncodeDense(p, command);
    Finish(command);
    const std::array names{"legacy",
                           "legacy_icb_pipeline",
                           "build_icb_raster",
                           "reuse_icb_raster",
                           "build_dense_icb_raster",
                           "reuse_dense_icb_raster"};
    nlohmann::json report{
        {"device", p.Device.name.UTF8String},
        {"rows", p.Rows},
        {"icb_allocated_bytes", p.Commands.allocatedSize},
        {"argument_table_bytes", p.ArgumentTable.allocatedSize},
        {"dense_icb_allocated_bytes", p.DenseCommands.allocatedSize},
        {"dense_slots_bytes", p.Slots.allocatedSize},
        {"execution_range_bytes", p.ExecutionRange.allocatedSize},
        {"scope",
         "Standalone supplied native indices, model matrices and clip; identical position-only "
         "depth; GPU ICB rebuild charged in build mode; no PBR/cull/scan/frame/motion admission"},
        {"modes", nlohmann::json::array()}};
    for (const auto *name : names) {
      report["modes"].push_back({{"name", name}, {"samples", nlohmann::json::array()}});
    }
    for (uint32_t round = 0; round < rounds; ++round) {
      @autoreleasepool {
        for (uint32_t one = 0; one < names.size(); ++one) {
          const auto mode = round % 2 == 0 ? one : static_cast<uint32_t>(names.size() - 1) - one;
          report["modes"][mode]["samples"].push_back(Sample(p, kPaths[mode]));
        }
      }
    }
    const auto baseline = VerifyPaths(p, report);
    VerifyChanges(p, baseline, report, argv[1]);
    std::ofstream file(argv[4]);
    file << report.dump(2) << '\n';
    if (!file) { Fail("report write failed"); }
    std::println("{} rows; {} depth pixels; all paths exact",
                 p.Rows,
                 report["visible_depth_pixels"].get<uint64_t>());
  }
}
