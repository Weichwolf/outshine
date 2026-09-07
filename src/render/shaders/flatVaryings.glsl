layout(location = 0) VARYING vec2 uv;
layout(location = 1) VARYING vec2 uv1;
layout(location = 2) VARYING vec4 colour;
layout(location = 3) flat VARYING vec3 emitted;
#if SUBJECT_WRITES_VELOCITY
layout(location = 7) VARYING vec4 curClip;
layout(location = 8) VARYING vec4 prevClip;
#endif
