#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cstdlib>
#include <print>

@interface OutshineProfilePass : NSObject
@property NSMutableSet<NSString *> *pipelines;
@property NSString *currentPipeline;
@property NSMutableDictionary<NSString *, NSNumber *> *work;
@property uint64_t directIndices;
@property uint64_t calls;
@property uint64_t indirectCalls;
@property uint64_t indirectIndices;
@property uint64_t unavailableIndirect;
@end
@implementation OutshineProfilePass

- (instancetype)init {
  self = [super init];
  if (self != nullptr) {
    _pipelines = [NSMutableSet new];
    _work = [NSMutableDictionary new];
    _currentPipeline = @"unnamed";
  }
  return self;
}

@end
@interface OutshineProfileBuffer : NSObject
@property id<MTLBuffer> source;
@property id<MTLBuffer> readback;
@property NSUInteger bytes;
@end
@implementation OutshineProfileBuffer
@end
@interface OutshineProfileIndirect : NSObject
@property OutshineProfileBuffer *buffer;
@property OutshineProfilePass *pass;
@property NSString *pipeline;
@property NSUInteger offset;
@end
@implementation OutshineProfileIndirect
@end
@interface OutshineProfileCommand : NSObject
@property id<MTLCounterSampleBuffer> samples;
@property NSMutableArray<OutshineProfilePass *> *passes;
@property NSMutableDictionary<NSValue *, OutshineProfileBuffer *> *buffers;
@property NSMutableArray<OutshineProfileIndirect *> *indirect;
@property uint64_t serial;
@property uint64_t omittedPasses;
@end
@implementation OutshineProfileCommand

- (instancetype)init {
  self = [super init];
  if (self != nullptr) {
    _passes = [NSMutableArray new];
    _buffers = [NSMutableDictionary new];
    _indirect = [NSMutableArray new];
  }
  return self;
}

@end

namespace {
char traceKey, passKey, encoderTraceKey;
std::atomic<uint64_t> serial{0};
id<MTLCounterSet> counterSet;
IMP renderOriginal, commitOriginal, pipelineOriginal, indexedOriginal, indirectOriginal,
    drawOriginal;
bool encoderPatched = false;

struct IndirectArguments {
  uint32_t count, instances, first;
  int32_t vertex;
  uint32_t instance;
};

constexpr size_t kIndirectArgumentBytes = 20;
static_assert(sizeof(IndirectArguments) == kIndirectArgumentBytes);
static_assert(alignof(IndirectArguments) == alignof(uint32_t));
static_assert(offsetof(IndirectArguments, count) == 0);
static_assert(offsetof(IndirectArguments, instances) == 4);
static_assert(offsetof(IndirectArguments, first) == 8);
static_assert(offsetof(IndirectArguments, vertex) == 12);
static_assert(offsetof(IndirectArguments, instance) == 16);
static_assert(sizeof(MTLCounterResultTimestamp) == sizeof(uint64_t));
static_assert(offsetof(MTLCounterResultTimestamp, timestamp) == 0);

void Pipeline(id self, SEL sel, id<MTLRenderPipelineState> state) {
  OutshineProfilePass *pass = objc_getAssociatedObject(self, &passKey);
  if (pass != nullptr) {
    pass.currentPipeline = state.label != nil ? state.label : @"unnamed";
    [pass.pipelines addObject:pass.currentPipeline];
  }
  reinterpret_cast<void (*)(id, SEL, id<MTLRenderPipelineState>)>(pipelineOriginal)(
      self, sel, state);
}

void Indexed(id self,
             SEL sel,
             MTLPrimitiveType type,
             NSUInteger count,
             MTLIndexType indexType,
             id<MTLBuffer> buffer,
             NSUInteger offset,
             NSUInteger instances,
             NSInteger base,
             NSUInteger first) {
  OutshineProfilePass *pass = objc_getAssociatedObject(self, &passKey);
  if (pass != nullptr) {
    ++pass.calls;
    pass.directIndices += uint64_t(count) * instances;
    pass.work[pass.currentPipeline] =
        @(pass.work[pass.currentPipeline].unsignedLongLongValue + uint64_t(count) * instances);
  }
  reinterpret_cast<void (*)(id,
                            SEL,
                            MTLPrimitiveType,
                            NSUInteger,
                            MTLIndexType,
                            id<MTLBuffer>,
                            NSUInteger,
                            NSUInteger,
                            NSInteger,
                            NSUInteger)>(indexedOriginal)(
      self, sel, type, count, indexType, buffer, offset, instances, base, first);
}

void Draw(id self,
          SEL sel,
          MTLPrimitiveType type,
          NSUInteger first,
          NSUInteger count,
          NSUInteger instances,
          NSUInteger firstInstance) {
  OutshineProfilePass *pass = objc_getAssociatedObject(self, &passKey);
  if (pass != nullptr) {
    ++pass.calls;
    pass.directIndices += uint64_t(count) * instances;
    pass.work[pass.currentPipeline] =
        @(pass.work[pass.currentPipeline].unsignedLongLongValue + uint64_t(count) * instances);
  }
  reinterpret_cast<void (*)(
      id, SEL, MTLPrimitiveType, NSUInteger, NSUInteger, NSUInteger, NSUInteger)>(drawOriginal)(
      self, sel, type, first, count, instances, firstInstance);
}

void Indirect(id self,
              SEL sel,
              MTLPrimitiveType type,
              MTLIndexType indexType,
              id<MTLBuffer> index,
              NSUInteger indexOffset,
              id<MTLBuffer> args,
              NSUInteger offset) {
  OutshineProfilePass *pass = objc_getAssociatedObject(self, &passKey);
  OutshineProfileCommand *trace = objc_getAssociatedObject(self, &encoderTraceKey);
  if (pass != nullptr) {
    ++pass.calls;
    ++pass.indirectCalls;
  }
  if ((trace != nullptr) && trace.indirect.count < 65536 &&
      offset <= size_t{1024} * 1024 - sizeof(IndirectArguments) &&
      offset + sizeof(IndirectArguments) <= args.length) {
    NSValue *key = [NSValue valueWithPointer:(__bridge void *)args];
    OutshineProfileBuffer *buffer = trace.buffers[key];
    if ((buffer == nullptr) && trace.buffers.count < 8) {
      buffer = [OutshineProfileBuffer new];
      buffer.source = args;
      trace.buffers[key] = buffer;
    }
    if (buffer != nullptr) {
      buffer.bytes = MAX(buffer.bytes, offset + sizeof(IndirectArguments));
      const auto call = [OutshineProfileIndirect new];
      call.pass = pass;
      call.pipeline = pass.currentPipeline;
      call.buffer = buffer;
      call.offset = offset;
      [trace.indirect addObject:call];
    } else {
      ++pass.unavailableIndirect;
    }
  } else if (pass != nullptr) {
    ++pass.unavailableIndirect;
  }
  reinterpret_cast<void (*)(id,
                            SEL,
                            MTLPrimitiveType,
                            MTLIndexType,
                            id<MTLBuffer>,
                            NSUInteger,
                            id<MTLBuffer>,
                            NSUInteger)>(indirectOriginal)(
      self, sel, type, indexType, index, indexOffset, args, offset);
}

IMP Patch(Class cls, SEL sel, IMP hook) {
  Method m = class_getInstanceMethod(cls, sel);
  if (m == nullptr) {
    std::println(stderr, "METAL_PASS missing {}", sel_getName(sel));
    std::abort();
  }
  IMP old = method_getImplementation(m);
  class_addMethod(cls, sel, old, method_getTypeEncoding(m));
  return method_setImplementation(class_getInstanceMethod(cls, sel), hook);
}

id<MTLRenderCommandEncoder> Render(id self, SEL sel, MTLRenderPassDescriptor *descriptor) {
  OutshineProfileCommand *trace = objc_getAssociatedObject(self, &traceKey);
  if (trace == nullptr) {
    trace = [OutshineProfileCommand new];
    trace.serial = ++serial;
    const auto d = [MTLCounterSampleBufferDescriptor new];
    d.counterSet = counterSet;
    d.storageMode = MTLStorageModeShared;
    d.sampleCount = 256;
    NSError *error = nil;
    trace.samples =
        [((id<MTLCommandBuffer>)self).device newCounterSampleBufferWithDescriptor:d error:&error];
    objc_setAssociatedObject(self, &traceKey, trace, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  const NSUInteger ordinal = trace.passes.count;
  if (ordinal >= 64) {
    ++trace.omittedPasses;
    const auto encoder =
        reinterpret_cast<id<MTLRenderCommandEncoder> (*)(id, SEL, MTLRenderPassDescriptor *)>(
            renderOriginal)(self, sel, descriptor);
    if (encoder != nullptr) {
      objc_setAssociatedObject(encoder, &passKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      objc_setAssociatedObject(encoder, &encoderTraceKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return encoder;
  }
  OutshineProfilePass *pass = [OutshineProfilePass new];
  [trace.passes addObject:pass];
  if ((trace.samples != nullptr) && ordinal < 64) {
    descriptor = [descriptor copy];
    const auto s = descriptor.sampleBufferAttachments[0];
    s.sampleBuffer = trace.samples;
    s.startOfVertexSampleIndex = ordinal * 4;
    s.endOfVertexSampleIndex = ordinal * 4 + 1;
    s.startOfFragmentSampleIndex = ordinal * 4 + 2;
    s.endOfFragmentSampleIndex = ordinal * 4 + 3;
  }
  const auto encoder =
      reinterpret_cast<id<MTLRenderCommandEncoder> (*)(id, SEL, MTLRenderPassDescriptor *)>(
          renderOriginal)(self, sel, descriptor);
  if ((encoder != nullptr) && !encoderPatched) {
    const Class cls = object_getClass(encoder);
    pipelineOriginal = Patch(cls, @selector(setRenderPipelineState:), (IMP)Pipeline);
    indexedOriginal = Patch(cls,
                            @selector(drawIndexedPrimitives:indexCount:indexType:indexBuffer:
                                      indexBufferOffset:instanceCount:baseVertex:baseInstance:),
                            (IMP)Indexed);
    indirectOriginal = Patch(cls,
                             @selector(drawIndexedPrimitives:indexType:indexBuffer:
                                       indexBufferOffset:indirectBuffer:indirectBufferOffset:),
                             (IMP)Indirect);
    drawOriginal =
        Patch(cls,
              @selector(drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:),
              (IMP)Draw);
    encoderPatched = true;
  }
  if (encoder != nullptr) {
    objc_setAssociatedObject(encoder, &passKey, pass, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(encoder, &encoderTraceKey, trace, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return encoder;
}

void DecodeIndirect(OutshineProfileCommand *trace) {
  for (OutshineProfileIndirect *call in trace.indirect) {
    if (call.buffer.readback == nil) {
      ++call.pass.unavailableIndirect;
      continue;
    }
    IndirectArguments arguments;
    std::memcpy(&arguments,
                static_cast<const char *>(call.buffer.readback.contents) + call.offset,
                sizeof(arguments));
    const auto indices = uint64_t(arguments.count) * arguments.instances;
    call.pass.indirectIndices += indices;
    call.pass.work[call.pipeline] =
        @(call.pass.work[call.pipeline].unsignedLongLongValue + indices);
  }
}

void PrintPass(OutshineProfileCommand *trace, NSData *data, NSUInteger at) {
  OutshineProfilePass *pass = trace.passes[at];
  std::array<MTLCounterResultTimestamp, 4> timestamps{};
  if (data.length >= (at + 1) * sizeof(timestamps)) {
    std::memcpy(timestamps.data(),
                static_cast<const char *>(data.bytes) + at * sizeof(timestamps),
                sizeof(timestamps));
  }
  bool valid = true;
  for (const auto &sample : timestamps) {
    valid = valid && sample.timestamp > 0 && sample.timestamp != MTLCounterErrorValue;
  }
  valid = valid && timestamps[1].timestamp >= timestamps[0].timestamp &&
          timestamps[3].timestamp >= timestamps[2].timestamp;
  NSString *labels = [[[pass.pipelines allObjects] sortedArrayUsingSelector:@selector(compare:)]
      componentsJoinedByString:@","];
  std::println(
      stderr,
      "METAL_PASS serial={} pass={} valid={:d} vertex_ns={} fragment_ns={} calls={} "
      "indirect_calls={} direct_indices={} indirect_indices={} unavailable_indirect={} pipeline={}",
      trace.serial,
      at,
      static_cast<int>(valid),
      valid ? timestamps[1].timestamp - timestamps[0].timestamp : 0,
      valid ? timestamps[3].timestamp - timestamps[2].timestamp : 0,
      pass.calls,
      pass.indirectCalls,
      pass.directIndices,
      pass.indirectIndices,
      pass.unavailableIndirect,
      labels.UTF8String);
  for (NSString *key in pass.work) {
    std::println(stderr,
                 "METAL_WORK serial={} pass={} indices={} pipeline={}",
                 trace.serial,
                 at,
                 pass.work[key].unsignedLongLongValue,
                 key.UTF8String);
  }
}

void Complete(OutshineProfileCommand *trace) {
  if (trace.omittedPasses > 0) {
    std::println(
        stderr, "METAL_TRUNCATED serial={} omitted_passes={}", trace.serial, trace.omittedPasses);
  }
  DecodeIndirect(trace);
  const auto data = [trace.samples resolveCounterRange:NSMakeRange(0, trace.passes.count * 4)];
  for (NSUInteger at = 0; at < trace.passes.count; ++at) { PrintPass(trace, data, at); }
}

void CopyArguments(id<MTLCommandBuffer> commands, OutshineProfileCommand *trace) {
  if (trace.buffers.count == 0) { return; }
  const auto copy = [commands blitCommandEncoder];
  for (NSValue *key in trace.buffers) {
    OutshineProfileBuffer *buffer = trace.buffers[key];
    buffer.readback = [commands.device newBufferWithLength:buffer.bytes
                                                   options:MTLResourceStorageModeShared];
    if (buffer.readback != nil) {
      [copy copyFromBuffer:buffer.source
               sourceOffset:0
                   toBuffer:buffer.readback
          destinationOffset:0
                       size:buffer.bytes];
    }
  }
  [copy endEncoding];
}

void Commit(id self, SEL sel) {
  OutshineProfileCommand *trace = objc_getAssociatedObject(self, &traceKey);
  id<MTLCommandBuffer> commands = self;
  if (trace != nil) {
    CopyArguments(commands, trace);
    [commands addCompletedHandler:^([[maybe_unused]] id<MTLCommandBuffer> done) {
      @autoreleasepool {
        Complete(trace);
      }
    }];
  }
  reinterpret_cast<void (*)(id, SEL)>(commitOriginal)(self, sel);
}

__attribute__((constructor)) void Install() {
  @autoreleasepool {
    const auto device = MTLCreateSystemDefaultDevice();
    for (id<MTLCounterSet> set in device.counterSets) {
      if ([set.name isEqualToString:MTLCommonCounterSetTimestamp]) { counterSet = set; }
    }
    if ((counterSet == nullptr) ||
        ![device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) {
      return;
    }
    const auto queue = [device newCommandQueue];
    const auto commands = [queue commandBuffer];
    const Class cls = object_getClass(commands);
    renderOriginal = Patch(cls, @selector(renderCommandEncoderWithDescriptor:), (IMP)Render);
    commitOriginal = Patch(cls, @selector(commit), (IMP)Commit);
    std::println(stderr, "METAL_PASS installed");
  }
}
}
