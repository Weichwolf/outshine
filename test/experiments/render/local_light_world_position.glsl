vec3 worldRelativePosition(ivec3 cell, vec3 localPosition, ivec3 cameraCell,
                           vec3 cameraLocal, float width) {
  return fma(vec3(cell - cameraCell), vec3(width), localPosition - cameraLocal);
}
