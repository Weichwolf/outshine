#ifndef SCAN_LANES
#define SCAN_LANES 256
#endif
#ifdef SCAN_LIVE_PREFIX
#define SCAN_LIMIT live
#else
#define SCAN_LIMIT kScanLanes
#endif
layout(local_size_x = SCAN_LANES, local_size_y = 1, local_size_z = 1) in;
layout(std430, set = 0, binding = 0) readonly buffer Buffer_kept { uint kept[]; };
layout(std430, set = 0, binding = 1) readonly buffer Buffer_batches { uint batches[]; };
layout(std430, set = 1, binding = 0) buffer Buffer_slot { uint slot[]; };
layout(std430, set = 1, binding = 1) buffer Buffer_args { uint args[]; };
const uint kScanLanes = gl_WorkGroupSize.x;
const uint kCullRejected = 0xffffffffu;
shared uint block[kScanLanes];
shared uint carried;

void main() {
  uint batch = gl_WorkGroupID.x;
  uint lane = gl_LocalInvocationID.x;

  const uint first = batches[2u * batch];
  const uint count = batches[2u * batch + 1u];
  if (lane == 0u) { carried = 0u; }
  barrier();

  for (uint from = 0u; from < count; from += kScanLanes) {
#ifdef SCAN_LIVE_PREFIX
    const uint live = min(count - from, kScanLanes);
#endif
    const uint at = from + lane;
    const uint mine = at < count ? kept[first + at] : 0u;
    block[lane] = mine;
    barrier();
    for (uint step = 1u; step < SCAN_LIMIT; step <<= 1u) {
      const uint taken = lane >= step ? block[lane - step] : 0u;
      barrier();
      block[lane] += taken;
      barrier();
    }
    if (at < count) {
      slot[first + at] = mine == 0u ? kCullRejected : carried + block[lane] - mine;
    }
    barrier();
    if (lane == SCAN_LIMIT - 1u) { carried += block[SCAN_LIMIT - 1u]; }
    barrier();
  }
  if (lane == 0u) { args[SCAN_ARGUMENT_STRIDE * batch] = carried; }
}
