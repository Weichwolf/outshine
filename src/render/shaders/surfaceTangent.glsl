vec4 transformedSurfaceTangent(mat3 frame, vec4 tangent) {
  float orientation = determinant(frame) < 0.0 ? -1.0 : 1.0;
  return vec4(normalize(frame[0] * tangent.x + frame[1] * tangent.y + frame[2] * tangent.z),
              tangent.w * orientation);
}
