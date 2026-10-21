const float kCullAffineGuard = 16.0 * 1.1920928955078125e-7;
const float kCullAffineMinimum = 1.1754943508222875e-38;

bool cullAffineNormal(vec4 value) {
  if (any(isnan(value)) || any(isinf(value))) { return false; }
  for (uint axis = 0u; axis < 4u; ++axis) {
    if (value[axis] != 0.0 && abs(value[axis]) < kCullAffineMinimum) { return false; }
  }
  return true;
}

bool cullAffineScale(mat4 model, out float scale) {
  if (model[0].w != 0.0 || model[1].w != 0.0 || model[2].w != 0.0 || model[3].w != 1.0) {
    return false;
  }
  for (uint column = 0u; column < 4u; ++column) {
    if (!cullAffineNormal(model[column])) { return false; }
  }
  const vec3 largest = max(max(abs(model[0].xyz), abs(model[1].xyz)), abs(model[2].xyz));
  const float peak = max(max(largest.x, largest.y), largest.z);
  if (peak == 0.0) { scale = 0.0; return true; }
  const mat3 linear = mat3(model) / peak;
  const mat3 gram = transpose(linear) * linear;
  const vec3 sums = vec3(dot(abs(gram[0]), vec3(1.0)),
                         dot(abs(gram[1]), vec3(1.0)),
                         dot(abs(gram[2]), vec3(1.0)));
  scale = sqrt(max(max(sums.x, sums.y), sums.z)) * peak * (1.0 + kCullAffineGuard);
  return cullAffineNormal(vec4(scale));
}

bool cullAffineSphere(mat4 model, vec4 local, vec3 shift, float scale, out vec4 world) {
  if (!cullAffineNormal(local) || local.w < 0.0 || !cullAffineNormal(vec4(shift, 0.0))) {
    return false;
  }
  const mat3 magnitude = mat3(abs(model[0].xyz), abs(model[1].xyz), abs(model[2].xyz));
  const vec3 reach = magnitude * (abs(local.xyz) + vec3(local.w));
  const vec3 error = kCullAffineGuard *
                    (reach + abs(model[3].xyz) + abs(shift) + vec3(1.0));
  world.xyz = (model * vec4(local.xyz, 1.0)).xyz + shift;
  world.w = (local.w * scale + length(error)) * (1.0 + kCullAffineGuard);
  return cullAffineNormal(world);
}

bool cullAffineOutside(vec4 plane, vec3 centre, float radius) {
  const float error = kCullAffineGuard *
                      (dot(abs(plane.xyz), abs(centre)) + abs(plane.w) + radius + 1.0);
  return dot(plane.xyz, centre) + plane.w < -radius - error;
}
