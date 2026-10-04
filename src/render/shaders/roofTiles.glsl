struct RoofTileJoint {
  float coverage;
  float courseSlope;
  float tileSlope;
};

RoofTileJoint roofTileJoint(float course, float tile, float courseWidth, float tileWidth) {
  float row = periodicBand(course, 0.0f, 0.055f, courseWidth);
  float column = periodicBand(tile, 0.485f, 0.515f, tileWidth);
  return RoofTileJoint(row + column - row * column,
      (1.0f - column) * periodicBandSlope(course, 0.0f, 0.055f, courseWidth),
      (1.0f - row) * periodicBandSlope(tile, 0.485f, 0.515f, tileWidth));
}

vec3 roofTileAxis(vec3 localDx, vec3 localDy, float heightDx, float heightDy) {
  vec3 n = cross(localDx, localDy);
  float normalSquared = dot(n, n);
  if (!(normalSquared > 1.0e-16f)) { return n * 0.0f; }
  n = n * inversesqrt(normalSquared);
  vec3 axis = cross(surfaceGradient(n, localDx, localDy, heightDx, heightDy), n);
  float axisSquared = dot(axis, axis);
  return axisSquared > 1.0e-8f ? axis * inversesqrt(axisSquared) : axis * 0.0f;
}
