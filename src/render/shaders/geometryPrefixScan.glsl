layout(local_size_x = 256) in;
#if LIVE_INPUT
layout(std430, set = 0, binding = 0) readonly buffer Inputs { uvec4 inputs[]; };
#else
layout(std430, set = 0, binding = 0) readonly buffer Inputs { uvec2 inputs[]; };
#endif
#if LIVE_INPUT
layout(std430, set = 0, binding = 1) readonly buffer Kept { uint kept[]; };
layout(std430, set = 0, binding = 2) readonly buffer Visible { uint visible[]; };
layout(std430, set = 1, binding = 2) buffer Slots { uint slots[]; };
#endif
layout(std430, set = 1, binding = 0) writeonly buffer Prefix { uvec2 prefix[]; };
layout(std430, set = 1, binding = 1) writeonly buffer Tails { uvec2 tails[]; };
layout(std140, set = 2, binding = 0) uniform View { uint count; uint gridWidth; } view;
shared uint values[256];
shared uint keys[256];
void main() {
  uint block = gl_WorkGroupID.y * view.gridWidth + gl_WorkGroupID.x;
  uint lane = gl_LocalInvocationID.x;
  uint at = block * 256u + lane;
#if LIVE_INPUT
  uvec2 item = at < view.count ? inputs[at].zw : uvec2(0u, 0xffffffffu);
  if (at < view.count && (at == 0u || inputs[at - 1u].w != item.y)) {
    slots[view.count + item.y] = 0u;
  }
  uint mine = at < view.count && visible[item.x] != 0u ? kept[at] : 0u;
#else
  uvec2 item = at < view.count ? inputs[at] : uvec2(0u, 0xffffffffu);
  uint mine = item.x;
#endif
  values[lane] = mine;
  keys[lane] = item.y;
  barrier();
  for (uint step = 1u; step < 256u; step <<= 1u) {
    uint previous = lane >= step && keys[lane - step] == item.y ? values[lane - step] : 0u;
    barrier();
    values[lane] += previous;
    barrier();
  }
  if (at >= view.count) { return; }
  uvec2 sum = uvec2(values[lane], item.y);
  prefix[at] = sum;
  if (lane == 255u || at + 1u == view.count) { tails[block] = sum; }
}
