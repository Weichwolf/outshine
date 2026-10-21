
struct Brdf { vec3 diffuse; vec3 specular; };

float max3(vec3 v) { return max(max(v.x, v.y), v.z); }

float brdfAnisotropicDistribution(float nh, float th, float bh, float at, float ab) {
  float a2 = at * ab;
  vec3 f = vec3(ab * th, at * bh, a2 * nh);
  float d = dot(f, f);
  if (!(d > 0.0)) { return 0.0; }
  float w2 = a2 / d;
  return a2 * w2 * w2 / kPi;
}

float brdfAnisotropicVisibility(float nl, float nv, float tv, float bv, float tl,
                                              float bl, float at, float ab) {
  float alongV = nl * length(vec3(at * tv, ab * bv, nv));
  float alongL = nv * length(vec3(at * tl, ab * bl, nl));
  float s = alongV + alongL;
  if (!(s > 0.0)) { return 0.0; }
  return clamp(0.5 / s, 0.0, 1.0);
}

float brdfDenominator(float nh, float a2) {
  precise float cosine = min(abs(nh), 1.0);
  precise float denominator = (1.0 - cosine) * (1.0 + cosine) + cosine * cosine * a2;
  return denominator;
}

float brdfDistribution(float nh, float a2) {
  float denominator = brdfDenominator(nh, a2);
  precise float scaled = a2 / denominator;
  precise float distribution = (scaled / denominator) * (1.0 / kPi);
  return distribution;
}

float brdfVisibility(float nl, float nv, float a2) {
  return 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
}

vec3 brdfWeightedSpecular(vec3 fresnel, float a2, float nl, float nv, float nh,
                        float incidentCosine) {
  if (!(a2 > 0.0)) { return vec3(0.0); }
  float denominator = brdfDenominator(nh, a2);
  precise vec3 scaled = fresnel / denominator;
  precise float visibility = (brdfVisibility(nl, nv, a2) * incidentCosine) * (1.0 / kPi);
  precise vec3 weighted = scaled * visibility;
  precise float fraction = a2 / denominator;
  precise vec3 specular = weighted * fraction;
  return min(specular, vec3(uintBitsToFloat(0x7f7fffffu)));
}

vec3 brdfSpecular(vec3 fresnel, float a2, float nl, float nv, float nh) {
  return brdfWeightedSpecular(fresnel, a2, nl, nv, nh, 1.0);
}

float toksvigA2(float a2, float l) {
  float d = l * a2 + 2.0 * (1.0 - a2) * (1.0 - l);
  float n = 2.0 * l * (1.0 - a2);
  float denominator = n + 2.0 * d;
  return denominator > 0.0 ? 2.0 * d / denominator : 1.0;
}

float roughenedBy(float roughness, float meanResultantLength) {
  if (!(meanResultantLength < 1.0)) { return roughness; }
  float alpha = roughness * roughness;
  return sqrt(sqrt(toksvigA2(alpha * alpha, meanResultantLength)));
}

vec3 brdfFresnel(vec3 f0, float f90, float vh) {
  float grazing = 1.0 - clamp(vh, 0.0, 1.0);
  float squared = grazing * grazing;
  return f0 + (vec3(f90) - f0) * (squared * squared * grazing);
}

float brdfLobe(float a2, float nl, float nv, float nh) {
  return brdfSpecular(vec3(1.0), a2, nl, nv, nh).x;
}

Brdf brdfCombine(vec3 diffuseColour, vec3 fresnel, float lobe) {
  Brdf terms;
  terms.diffuse = (vec3(1.0) - fresnel) * diffuseColour * (1.0 / kPi);
  terms.specular = fresnel * lobe;
  return terms;
}

Brdf brdfRgbMix(vec3 diffuseColour, vec3 fresnel, float lobe) {
  Brdf terms;
  terms.diffuse = (1.0 - max3(fresnel)) * diffuseColour * (1.0 / kPi);
  terms.specular = fresnel * lobe;
  return terms;
}

Brdf metalRoughBrdf(vec3 diffuseColour, vec3 f0, float f90, float a2,
                                  float nl, float nv, float nh, float vh) {
  vec3 fresnel = brdfFresnel(f0, f90, vh);
  Brdf terms = brdfCombine(diffuseColour, fresnel, 0.0);
  terms.specular = brdfSpecular(fresnel, a2, nl, nv, nh);
  return terms;
}
