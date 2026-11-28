uniform sampler2DShadow skylightDepth;
uniform mat4 skylightProjection[8];
uniform vec3 skylightDirection[8];
uniform vec3 skylightRadiance[8];
uniform vec3 skylightEye;

vec3 occludedSkylight(vec3 unoccluded, vec3 normal) {
  float localWeight = 1. - smoothstep(36., 48., length(point - skylightEye));
  if (localWeight == 0.)
    return unoccluded;
  vec2 atlasTiles = vec2(4., 2.);
  vec2 halfTexel = .5 * atlasTiles / vec2(textureSize(skylightDepth, 0));
  vec3 removed = vec3(0.);
  for (int i = 0; i < skylightDirection.length(); ++i) {
    float cosine = dot(normal, skylightDirection[i]);
    if (cosine <= 0.)
      continue;
    vec3 uv = (skylightProjection[i] * vec4(point, 1.)).xyz * .5 + .5;
    float edge = max(abs(uv.x - .5), abs(uv.y - .5));
    if (edge >= .5 || uv.z <= 0. || uv.z >= 1.)
      continue;
    vec2 tile = vec2(i % 4, i / 4);
    vec2 tileUv = (tile + clamp(uv.xy, halfTexel, 1. - halfTexel)) / atlasTiles;
    float slope = sqrt(max(0., 1. - cosine * cosine)) / max(.22, cosine);
    float visibility = texture(skylightDepth, vec3(tileUv, uv.z - (.10 + .12 * slope) / 256.));
    removed += (1. - visibility) * (1. - smoothstep(.4, .5, edge)) * cosine * skylightRadiance[i];
  }
  return max(.03 * unoccluded, unoccluded - localWeight * (2. / float(skylightDirection.length())) * removed);
}
