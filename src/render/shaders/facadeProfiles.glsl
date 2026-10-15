const vec4 kFacadeProfiles[] = vec4[](
#define FACADE_PROFILE(name, value, bay, floor, width, height) vec4(bay, floor, width, height),
#include "content/shade/FacadeProfiles.inc"
#undef FACADE_PROFILE
vec4(0.0f));

const int kFacadeStyleCount = kFacadeProfiles.length() - 1;

vec2 facadeFractions(int style) {
  return kFacadeProfiles[style].zw;
}
