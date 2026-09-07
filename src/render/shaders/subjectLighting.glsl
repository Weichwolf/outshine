vec3 shadeRow(M surface,

                              vec3 localM, vec3 n, vec3 p, vec3 albedo, float metalness,
                              float roughness, vec3 dielectricF0, float dielectricF90,
                              vec3 emitted, vec3 tangentDir, vec4 lightSpace,
                              sampler2D shadowMap) {
  vec3 sheenColour = vec3(surface.sheenColour);
  float sheenRoughness = surface.sheenRoughness;
  float clearcoat = surface.clearcoat;
  float clearcoatRoughness = surface.clearcoatRoughness;
  float anisotropy = surface.anisotropy;
  float anisotropyRotation = surface.anisotropyRotation;

  float iridescence = surface.iridescence;
  float iridescenceThickness = surface.iridescenceThicknessMax;

  if (!(iridescenceThickness > 0.0)) { iridescence = 0.0; }
  vec3 v = normalize(lights.viewPosition.xyz - p * lights.viewPosition.w);
  float a = roughness * roughness;
  float a2 = a * a;
  vec3 diffuseColour = albedo * (1.0 - metalness) * (1.0 - surface.transmission);
  vec3 f0 = mix(dielectricF0, albedo, metalness);

  float f90 = mix(dielectricF90, 1.0, metalness);
  float nv = max(dot(n, v), 1.0e-6);

  vec3 energyScale = ggxEnergyScale(f0, roughness, nv);

  float anisoLen = length(tangentDir);
  bool anisotropic = anisotropy > 0.0 && anisoLen > 0.0;
  vec3 anisoT = vec3(1.0, 0.0, 0.0);
  vec3 anisoB = vec3(0.0, 1.0, 0.0);
  if (anisotropic) {
    vec3 alongT = tangentDir / anisoLen;
    vec3 alongB = normalize(cross(n, alongT));
    float turnC = cos(anisotropyRotation);
    float turnS = sin(anisotropyRotation);
    anisoT = normalize(alongT * turnC + alongB * turnS);
    anisoB = normalize(cross(n, anisoT));
  }
  vec3 originM = localM + n * lights.count.y;
  vec3 sum = vec3(0.0);
  int count = int(lights.count.x);
  for (int at = 0; at < count; at = at + 1) {
    Light light = lights.items[at];
    vec3 toward = -light.beam.xyz;
    float attenuation = 1.0;
    float reachM = uintBitsToFloat(0x7f800000u);
    if (light.tint.w > 0.5) {
      vec3 offset = light.place.xyz - p;
      float square = dot(offset, offset);
      if (square <= 0.0) { continue; }
      toward = offset * inversesqrt(square);
      reachM = sqrt(square);

      float reach = square * light.place.w * light.place.w;
      attenuation = clamp(1.0 - reach * reach, 0.0, 1.0) / square;
    }
    if (light.tint.w > 1.5) {
      float angular = clamp((dot(light.beam.xyz, -toward) - light.cone.x) * light.cone.y, 0.0, 1.0);
      attenuation *= angular * angular;
    }
    if (light.tint.w <= 0.5 && lights.count.z > 0.5) {
      vec3 lit = lightSpace.xyz / max(1.0e-6, lightSpace.w);
      vec2 atlasUv = lit.xy * vec2(0.5, -0.5) + 0.5;
      if (atlasUv.x >= 0.0 && atlasUv.x <= 1.0 && atlasUv.y >= 0.0 && atlasUv.y <= 1.0 &&
          lit.z >= 0.0 && lit.z <= 1.0) {
        float nearest = textureLod(shadowMap, atlasUv, 0.0).r;
        float bias = lights.count.w;
        attenuation = attenuation * (lit.z + bias < nearest ? 0.0 : 1.0);
      }
    }
    float nl = dot(n, toward);
    if (nl <= 0.0 || attenuation <= 0.0) { continue; }

    vec3 h = normalize(toward + v);
    float nh = max(dot(n, h), 0.0);
    float vh = max(dot(v, h), 0.0);

    float lobe = brdfLobe(a2, nl, nv, nh);
    if (anisotropic) {

      float at = mix(a, 1.0, anisotropy * anisotropy);
      float ab = a;
      lobe = brdfAnisotropicDistribution(nh, dot(anisoT, h), dot(anisoB, h), at, ab) *
             brdfAnisotropicVisibility(nl, nv, dot(anisoT, v), dot(anisoB, v),
                                       dot(anisoT, toward), dot(anisoB, toward), at, ab);
    }
    Brdf reflected;
    if (iridescence > 0.0) {

      vec3 filmed = iridescenceFresnel(vh, iridescenceThickness, surface.iridescenceIor, f0);
      reflected = brdfRgbMix(diffuseColour, mix(brdfFresnel(f0, f90, vh), filmed, iridescence), lobe);
    } else {
      reflected = brdfCombine(diffuseColour, brdfFresnel(f0, f90, vh), lobe);
    }
    reflected.specular *= energyScale;

    vec3 sheen = sheenColour * sheenDistribution(nh, sheenRoughness) *
                   sheenVisibility(nl, nv, sheenRoughness);
    float keep = sheenAlbedoScaling(sheenColour, nv, sheenRoughness);
    vec3 layered = (reflected.diffuse + reflected.specular) * keep + sheen;

    if (clearcoat > 0.0) {
      float coatA = clearcoatRoughness * clearcoatRoughness;
      float coatA2 = coatA * coatA;

      float coatF = 0.04 + 0.96 * pow(1.0 - nv, 5.0);
      float coatLobe = brdfLobe(coatA2, nl, nv, nh);
      float weight = clearcoat * coatF;
      layered = layered * (1.0 - weight) + vec3(weight * coatLobe);
    }
    sum = sum + layered * nl * attenuation * light.tint.rgb;
  }

  const float nvClamped = clamp(nv, 0.0, 1.0);

  const vec3 specularEnvironment = brdfFresnel(f0, f90, nvClamped);
  const float skyShare = clamp(dot(n, lights.up.xyz) * 0.5 + 0.5, 0.0, 1.0);
  vec3 skyRadiance = lights.environment.rgb;
  vec3 groundRadiance = lights.bounced.rgb;
  if (lights.environment.w > 0.0) {
    skyRadiance += skyDiffuseRadiance();
    groundRadiance += groundDiffuseRadiance();
  }
  const vec3 ambient = mix(groundRadiance, skyRadiance, skyShare);
  sum = sum + ambient * (diffuseColour + specularEnvironment);
  return sum + emitted;
}

vec3 shade(M surface,
                            vec3 localM, vec3 n, vec3 p, vec3 albedo, vec4 lightSpace,
                           sampler2D shadowMap) {
  return shadeRow(surface,  localM, n, p, albedo, surface.metalness,
                  surface.roughness, vec3(surface.f0), surface.specularWeight,
                  surface.emissive, vec3(0.0), lightSpace, shadowMap);
}

vec3 facing(vec3 n, bool front) {
  return (front ? normalize(n) : -normalize(n));
}
