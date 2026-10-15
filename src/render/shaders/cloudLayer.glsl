#ifndef OUTSHINE_CLOUD_LAYER_GLSL
#define OUTSHINE_CLOUD_LAYER_GLSL

struct CloudLayer {
  vec4 shell;
  vec4 field;
  vec4 drift;
};

float cloudAltitude(vec3 positionM, float radiusM) {
  float relative = dot(positionM, positionM) + 2.0 * radiusM * positionM.y;
  return relative / (sqrt(relative + radiusM * radiusM) + radiusM);
}

vec3 cloudSphereRoots(vec3 originM, vec3 direction, float heightM, float radiusM) {
  float b = dot(originM, direction) + radiusM * direction.y;
  float c = dot(originM, originM) + 2.0 * radiusM * originM.y -
            heightM * (2.0 * radiusM + heightM);
  float discriminant = b * b - c;
  if (discriminant < 0.0) { return vec3(0.0); }
  float q = -b - (b < 0.0 ? -1.0 : 1.0) * sqrt(discriminant);
  float other = q == 0.0 ? 0.0 : c / q;
  return vec3(min(q, other), max(q, other), 1.0);
}

mat2 cloudShell(CloudLayer layer, vec3 originM, vec3 direction, float maximumM) {
  vec3 outer = cloudSphereRoots(originM, direction, layer.shell.y + layer.shell.z, layer.shell.x);
  if (outer.z == 0.0 || outer.y <= 0.0) { return mat2(0.0); }
  vec3 inner = cloudSphereRoots(originM, direction, layer.shell.y, layer.shell.x);
  vec3 ground = cloudSphereRoots(originM, direction, 0.0, layer.shell.x);
  float start = max(outer.x, 0.0);
  float stop = min(outer.y, maximumM);
  bool inward = dot(originM, direction) + layer.shell.x * direction.y < 0.0;
  if (ground.z > 0.0 && inward && ground.x >= 0.0) { stop = min(stop, ground.x); }
  if (stop <= start) { return mat2(0.0); }
  if (inner.z == 0.0 || inner.y <= start || inner.x >= stop) {
    return mat2(vec2(start, stop), vec2(0.0));
  }
  vec2 first = vec2(start, clamp(inner.x, start, stop));
  float secondStart = max(inner.y, start);
  vec2 second = vec2(secondStart, max(secondStart, stop));
  if (first.y == first.x && second.y > second.x) {
    first = second;
    second = vec2(0.0);
  }
  if (first.y <= first.x) { first = vec2(0.0); }
  if (second.y <= second.x) { second = vec2(0.0); }
  return mat2(first, second);
}

float cloudHash(ivec3 cell) {
  uvec3 key = uvec3(cell);
  uint bits = key.x * 1597334677u;
  bits ^= key.y * 3812015801u;
  bits ^= key.z * 2798796415u;
  bits ^= bits >> 16;
  bits *= 2246822519u;
  bits ^= bits >> 13;
  return float(bits >> 8) / 16777215.0;
}

float cloudNoise(vec3 position) {
  ivec3 cell = ivec3(floor(position));
  vec3 fraction = fract(position);
  vec3 blend = fraction * fraction * (3.0 - 2.0 * fraction);
  float result = 0.0;
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      for (int z = 0; z < 2; ++z) {
        vec3 weight = mix(vec3(1.0) - blend, blend, bvec3(x != 0, y != 0, z != 0));
        result += weight.x * weight.y * weight.z * cloudHash(cell + ivec3(x, y, z));
      }
    }
  }
  return result;
}

float cloudExtinction(CloudLayer layer, vec3 positionM) {
  if (layer.field.x <= 0.0) { return 0.0; }
  float height = (cloudAltitude(positionM, layer.shell.x) - layer.shell.y) / layer.shell.z;
  if (height <= 0.0 || height >= 1.0) { return 0.0; }
  vec3 advected = (positionM - layer.drift.xyz) / 1400.0;
  float broad = cloudNoise(advected);
  float detail = cloudNoise(advected * 3.1 + vec3(19.0, 7.0, 3.0));
  float structure = 0.8 * broad + 0.2 * detail;
  float amount = clamp((structure - (1.0 - layer.field.x)) * 2.0, 0.0, 1.0);
  return layer.shell.w * 6.0 * height * (1.0 - height) * amount;
}

float cloudOpticalDepth(CloudLayer layer, vec3 originM, vec3 direction, float maximumM) {
  if (layer.field.x <= 0.0) { return 0.0; }
  mat2 intervals = cloudShell(layer, originM, direction, maximumM);
  float optical = 0.0;
  for (int interval = 0; interval < 2; ++interval) {
    vec2 span = intervals[interval];
    if (span.y <= span.x) { continue; }
    float stepM = (span.y - span.x) / 16.0;
    for (int sampleIndex = 0; sampleIndex < 16; ++sampleIndex) {
      float distanceM = span.x + (float(sampleIndex) + 0.5) * stepM;
      optical += cloudExtinction(layer, originM + direction * distanceM) * stepM;
    }
  }
  return optical;
}

#endif
