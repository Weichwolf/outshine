layout(location = 0) VARYING vec2 uv;
layout(location = 1) VARYING vec2 uv1;
layout(location = 2) VARYING vec4 colour;
layout(location = 3) VARYING vec3 normal;
layout(location = 4) VARYING vec3 position;
layout(location = 5) VARYING vec3 localPosition;
layout(location = 6) VARYING vec4 lightSpace;
#if SUBJECT_WRITES_VELOCITY
layout(location = 7) VARYING vec4 curClip;
layout(location = 8) VARYING vec4 prevClip;
#endif
