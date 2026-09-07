layout(std430, set = 2, binding = SKY_IRRADIANCE_BINDING) readonly buffer SkyIrradiance {
  float skyIrradiance[];
};
vec3 skyDiffuseRadiance() {
  return vec3(skyIrradiance[0], skyIrradiance[1], skyIrradiance[2]) * lights.environment.w / acos(-1.0);
}
vec3 groundDiffuseRadiance() {
  vec3 sunlight = vec3(skyIrradiance[3], skyIrradiance[4], skyIrradiance[5]) *
                  max(lights.skyGround.w, 0.0) * lights.environment.w / acos(-1.0);
  return lights.skyGround.rgb * (sunlight + skyDiffuseRadiance());
}
