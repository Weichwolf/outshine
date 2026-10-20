#if SUBJECT_CONSTANT_UV
vec2 uv = vec2(0.0);
vec2 uv1 = vec2(0.0);
#else
layout(location = 0) VARYING vec4 textureCoordinates;
vec2 uv;
vec2 uv1;
#endif
layout(location = 2) VARYING vec4 colour;
layout(location = 3) VARYING vec4 normalMetric;
vec3 normal;
layout(location = 4) VARYING vec4 positionMetric;
vec3 position;
layout(location = 5) VARYING vec4 localPositionMetric;
vec3 localPosition;
layout(location = 6) VARYING vec4 lightSpace;
vec3 groundWorldM;
#if SUBJECT_WRITES_VELOCITY
layout(location = 7) VARYING vec4 curClip;
layout(location = 8) VARYING vec4 prevClip;
#endif

#if GROUND_PATCHES
layout(location = 11) flat VARYING uint groundSampleCode;
layout(location = 12) VARYING vec2 groundSampleUV;
#endif

#if defined(SUBJECT_VERTEX_STAGE)
void writeSubjectVaryings() {
#if !SUBJECT_CONSTANT_UV
  textureCoordinates = vec4(uv, uv1);
#endif
  normalMetric = vec4(normal, groundWorldM.x);
  positionMetric = vec4(position, groundWorldM.y);
  localPositionMetric = vec4(localPosition, groundWorldM.z);
}
#else
void readSubjectVaryings() {
#if !SUBJECT_CONSTANT_UV
  uv = textureCoordinates.xy;
  uv1 = textureCoordinates.zw;
#endif
  normal = normalMetric.xyz;
  position = positionMetric.xyz;
  localPosition = localPositionMetric.xyz;
  groundWorldM = vec3(normalMetric.w, positionMetric.w, localPositionMetric.w);
}
#endif
