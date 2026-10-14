vec3 thinSheetDiffuse(vec3 reflection, vec3 transmission, float cosine) {
  return (reflection * max(cosine, 0.0) + transmission * max(-cosine, 0.0)) / acos(-1.0);
}
