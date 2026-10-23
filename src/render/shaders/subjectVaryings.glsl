#if PRISM_GRID && !defined(SUBJECT_VERTEX_STAGE)
#define SUBJECT_VARYING(at)
#else
#define SUBJECT_VARYING(at) layout(location = at) VARYING
#endif
#if SUBJECT_CONSTANT_UV
vec2 uv = vec2(0.0);
vec2 uv1 = vec2(0.0);
#else
SUBJECT_VARYING(0) vec4 textureCoordinates;
vec2 uv;
vec2 uv1;
#endif
SUBJECT_VARYING(2) vec4 colour;
SUBJECT_VARYING(3) vec4 normalMetric;
vec3 normal;
SUBJECT_VARYING(4) vec4 positionMetric;
vec3 position;
SUBJECT_VARYING(5) vec4 localPositionMetric;
vec3 localPosition;
SUBJECT_VARYING(6) vec4 lightSpace;
vec3 groundWorldM;
#if SUBJECT_WRITES_VELOCITY
SUBJECT_VARYING(7) vec4 curClip;
SUBJECT_VARYING(8) vec4 prevClip;
#endif

#if GROUND_PATCHES
layout(location = 11) flat VARYING uint groundSampleCode;
SUBJECT_VARYING(12) vec2 groundSampleUV;
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
