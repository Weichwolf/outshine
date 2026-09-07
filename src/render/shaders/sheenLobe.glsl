
float sheenDistribution(float nh, float roughness) {
  float alpha = roughness * roughness;
  if (!(alpha > 0.0)) { return 0.0; }
  float inverse = 1.0 / alpha;
  float sin2h = 1.0 - nh * nh;
  if (!(sin2h > 0.0)) { return 0.0; }
  return (2.0 + inverse) * pow(sin2h, inverse * 0.5) / (2.0 * kPi);
}

float sheenLambdaFit(float x, float alpha) {
  float smoothness = (1.0 - alpha) * (1.0 - alpha);
  float a = mix(21.5473, 25.3245, smoothness);
  float b = mix(3.82987, 3.32435, smoothness);
  float c = mix(0.19823, 0.16801, smoothness);
  float d = mix(-1.97760, -1.27393, smoothness);
  float e = mix(-4.32054, -4.85967, smoothness);
  return a / (1.0 + b * pow(x, c)) + d * x + e;
}

float sheenLambda(float cosTheta, float alpha) {
  return abs(cosTheta) < 0.5 ? exp(sheenLambdaFit(cosTheta, alpha))
                              : exp(2.0 * sheenLambdaFit(0.5, alpha) -
                                    sheenLambdaFit(1.0 - cosTheta, alpha));
}

float sheenVisibility(float nl, float nv, float roughness) {
  float alpha = roughness * roughness;
  if (!(alpha > 0.0) || !(nl > 0.0) || !(nv > 0.0)) { return 0.0; }
  return 1.0 / ((1.0 + sheenLambda(nv, alpha) + sheenLambda(nl, alpha)) * (4.0 * nv * nl));
}

float sheenAlbedo(float nv, float roughness) {
  int r = clamp(int(roughness * float(kSheenSteps)), 0, kSheenSteps - 1);
  int v = clamp(int(nv * float(kSheenSteps)), 0, kSheenSteps - 1);
  return kSheenAlbedo[r * kSheenSteps + v];
}

float sheenAlbedoScaling(vec3 sheenColour, float nv, float roughness) {
  float strongest = max(max(sheenColour.x, sheenColour.y), sheenColour.z);
  return 1.0 - strongest * sheenAlbedo(nv, roughness);
}
