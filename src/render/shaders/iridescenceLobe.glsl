
float iridescenceIorToF0(float transmitted, float incident) {
  float d = (transmitted - incident) / (transmitted + incident);
  return d * d;
}

vec3 iridescenceF0ToIor(vec3 f0) {
  vec3 s = sqrt(clamp(f0, 0.0, kIriF0Ceiling));
  return (1.0 + s) / (1.0 - s);
}

float iridescenceSchlick(float f0, float cosTheta) {
  float m = 1.0 - cosTheta;
  float m2 = m * m;
  return f0 + (1.0 - f0) * m2 * m2 * m;
}

vec3 iridescenceSchlick3(vec3 f0, float cosTheta) {
  float m = 1.0 - cosTheta;
  float m2 = m * m;
  return f0 + (1.0 - f0) * m2 * m2 * m;
}

vec3 iridescenceSensitivity(float opdNm, vec3 shift) {
  float phase = 2.0 * kPi * opdNm * 1.0e-9;
  vec3 xyz = kIriVal * sqrt(2.0 * kPi * kIriVar) * cos(kIriPos * phase + shift) *
               exp(-phase * phase * kIriVar);
  xyz.x += kIriValX2 * sqrt(2.0 * kPi * kIriVarX2) * cos(kIriPosX2 * phase + shift.x) *
           exp(-kIriVarX2 * phase * phase);
  return kIriXyzToRgb * (xyz / kIriNorm);
}

vec3 iridescenceFresnel(float cosTheta1, float thicknessNm, float filmIor,
                                        vec3 baseF0) {
  float sinTheta2Sq = (kIriOutsideIor / filmIor) * (kIriOutsideIor / filmIor) *
                      (1.0 - cosTheta1 * cosTheta1);
  float cosTheta2Sq = 1.0 - sinTheta2Sq;
  if (cosTheta2Sq < 0.0) { return vec3(1.0); }
  float cosTheta2 = sqrt(cosTheta2Sq);

  float r12 = iridescenceSchlick(iridescenceIorToF0(filmIor, kIriOutsideIor), cosTheta1);
  float t121 = 1.0 - r12;
  float phi12 = filmIor < kIriOutsideIor ? kPi : 0.0;
  float phi21 = kPi - phi12;

  vec3 baseIor = iridescenceF0ToIor(baseF0 + 0.0001);
  vec3 r1 = (baseIor - filmIor) / (baseIor + filmIor);
  vec3 r23 = iridescenceSchlick3(r1 * r1, cosTheta2);
  vec3 phi = vec3(phi21) + mix(vec3(0.0), vec3(kPi), lessThan(baseIor, vec3(filmIor)));

  float opd = 2.0 * filmIor * thicknessNm * cosTheta2;

  vec3 r123 = clamp(r12 * r23, 1.0e-5, 0.9999);
  vec3 r123root = sqrt(r123);
  vec3 rs = t121 * t121 * r23 / (1.0 - r123);
  vec3 f = r12 + rs;
  vec3 cm = rs - t121;
  for (int m = 1; m <= 2; ++m) {
    cm *= r123root;
    f += cm * 2.0 * iridescenceSensitivity(float(m) * opd, float(m) * phi);
  }
  return clamp(f, 0.0, 1.0);
}
