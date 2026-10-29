vec4 radiancePrefix(sampler2D image, vec2 at) {
  vec2 most = vec2(textureSize(image, 0) - 1);
  vec2 point = clamp(at, vec2(0.0), most);
  ivec2 low = ivec2(floor(point));
  ivec2 high = min(low + 1, ivec2(most));
  vec2 fraction = fract(point);
  return mix(mix(texelFetch(image, low, 0), texelFetch(image, ivec2(high.x, low.y), 0), fraction.x),
             mix(texelFetch(image, ivec2(low.x, high.y), 0), texelFetch(image, high, 0), fraction.x),
             fraction.y);
}

vec4 radianceSample(sampler2D image, vec2 uv, float scattering) {
  if (scattering != 3.0) { return texture(image, uv); }
  vec2 at = uv * vec2(textureSize(image, 0) - 1);
  vec2 halfSpan = 0.5 * (abs(dFdx(at)) + abs(dFdy(at)));
  vec2 low = at - halfSpan;
  vec2 high = at + halfSpan;
  float area = 4.0 * halfSpan.x * halfSpan.y;
  if (!(area > 0.0)) { return vec4(0.0); }
  vec4 value = (radiancePrefix(image, high) + radiancePrefix(image, low) -
                radiancePrefix(image, vec2(low.x, high.y)) -
                radiancePrefix(image, vec2(high.x, low.y))) / area;
  value.a = clamp(value.a, 0.0, 1.0);
  value.rgb = value.a > 0.0 ? max(value.rgb, vec3(0.0)) : vec3(0.0);
  return value;
}
