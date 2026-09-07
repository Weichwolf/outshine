
float ggxDirectionalAlbedo(float nv, float roughness) {
  float rf = clamp(roughness, 0.0, 1.0) * float(kEnergyRoughnessSteps - 1);
  float vf = clamp(nv, 0.0, 1.0) * float(kEnergyViewSteps - 1);
  int r0 = int(rf); int v0 = int(vf);
  int r1 = min(r0 + 1, kEnergyRoughnessSteps - 1);
  int v1 = min(v0 + 1, kEnergyViewSteps - 1);
  float rt = rf - float(r0); float vt = vf - float(v0);
  float a = mix(kGgxAlbedo[r0 * kEnergyViewSteps + v0], kGgxAlbedo[r0 * kEnergyViewSteps + v1], vt);
  float b = mix(kGgxAlbedo[r1 * kEnergyViewSteps + v0], kGgxAlbedo[r1 * kEnergyViewSteps + v1], vt);
  return mix(a, b, rt);
}

float ggxEnergyAverage(float roughness) {
  float rf = clamp(roughness, 0.0, 1.0) * float(kEnergyRoughnessSteps - 1);
  int r0 = int(rf); int r1 = min(r0 + 1, kEnergyRoughnessSteps - 1);
  return mix(kGgxAlbedoAverage[r0], kGgxAlbedoAverage[r1], rf - float(r0));
}

vec3 schlickAverage(vec3 f0) { return f0 + (1.0 - f0) / 21.0; }

vec3 ggxEnergyScale(vec3 f0, float roughness, float nv) {
  float e = ggxDirectionalAlbedo(nv, roughness);
  if (!(e > 0.0) || !(e < 1.0)) { return vec3(1.0); }
  float missing = (1.0 - e) / e;
  float eAverage = ggxEnergyAverage(roughness);
  vec3 favg = schlickAverage(f0);
  vec3 fms = favg * eAverage / (1.0 - favg * (1.0 - eAverage));
  return 1.0 + fms * missing;
}
