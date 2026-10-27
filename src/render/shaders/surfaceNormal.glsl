vec3 transformedSurfaceNormal(mat3 frame, vec3 normal) {
  return normalize(transpose(inverse(frame)) * normal);
}
