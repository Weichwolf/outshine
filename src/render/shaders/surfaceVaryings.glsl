#if PRISM_GRID && !defined(SURFACE_VERTEX_STAGE)
#define SURFACE_VARYING(at)
#else
#define SURFACE_VARYING(at) layout(location = at) VARYING
#endif
#if SURFACE_CONSTANT_UV
vec2 uv = vec2(0.0);
vec2 uv1 = vec2(0.0);
#else
SURFACE_VARYING(0) vec4 textureCoordinates;
vec2 uv;
vec2 uv1;
#endif
SURFACE_VARYING(2) vec4 colour;
SURFACE_VARYING(3) vec4 normalMetric;
vec3 normal;
SURFACE_VARYING(4) vec4 positionMetric;
vec3 position;
SURFACE_VARYING(5) vec4 localPositionMetric;
vec3 localPosition;
SURFACE_VARYING(6) vec4 lightSpace;
vec3 groundWorldM;
#if SURFACE_WRITES_VELOCITY
SURFACE_VARYING(7) vec4 curClip;
SURFACE_VARYING(8) vec4 prevClip;
#endif

#if GROUND_PATCHES
layout(location = 11) flat VARYING uint groundSampleCode;
SURFACE_VARYING(12) vec2 groundSampleUV;
#endif

#if defined(SURFACE_VERTEX_STAGE)
void writeSurfaceVaryings() {
#if !SURFACE_CONSTANT_UV
  textureCoordinates = vec4(uv, uv1);
#endif
  normalMetric = vec4(normal, groundWorldM.x);
  positionMetric = vec4(position, groundWorldM.y);
  localPositionMetric = vec4(localPosition, groundWorldM.z);
}
#else
void readSurfaceVaryings(vec4 sampledPositionMetric) {
#if !SURFACE_CONSTANT_UV
  uv = textureCoordinates.xy;
  uv1 = textureCoordinates.zw;
#endif
  normal = normalMetric.xyz;
  position = sampledPositionMetric.xyz;
  localPosition = localPositionMetric.xyz;
  groundWorldM = vec3(normalMetric.w, sampledPositionMetric.w, localPositionMetric.w);
}
void readSurfaceVaryings() { readSurfaceVaryings(positionMetric); }
#endif
