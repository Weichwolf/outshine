layout(location = 0) out vec4 outColour;
#if SUBJECT_WRITES_VELOCITY
layout(location = 1) out vec2 outVelocity;
#endif
#if SUBJECT_NORMAL_LOCATION > 0
layout(location = SUBJECT_NORMAL_LOCATION) out vec4 outNormal;
#endif
#if SUBJECT_IDENTITY_LOCATION > 0
layout(location = SUBJECT_IDENTITY_LOCATION) out vec4 outIdentity;
#endif
void outputSurface(vec4 shaded, vec3 n, float identity) {
  outColour = shaded;
#if SUBJECT_WRITES_VELOCITY
  outVelocity = curClip.xy / curClip.w - prevClip.xy / prevClip.w;
#endif
#if SUBJECT_NORMAL_LOCATION > 0
  outNormal = vec4(n, gl_FrontFacing ? 1.0 : -1.0);
#endif
#if SUBJECT_IDENTITY_LOCATION > 0
  outIdentity = vec4(identity, 0.0, 0.0, 1.0);
#endif
}
