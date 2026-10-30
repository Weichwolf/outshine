struct SurfaceSample {
  vec3 Albedo;
  float Alpha;
  vec3 Normal;
  float Roughness;
  float Metalness;
  vec3 Emission;
  vec3 SpecularF0;
  float SpecularF90;
  vec3 Tangent;
};

vec2 surfaceTextureCoordinate(vec3 u, vec3 v, float second) {
  vec3 homogeneous = vec3(mix(uv, uv1, second), 1.0);
  return vec2(dot(u, homogeneous), dot(v, homogeneous));
}
#if LIT_TEXTURED
vec4 sampleSurfaceTexture(sampler2D source, vec3 u, vec3 v, float second) {
  vec2 dx = mix(surfaceDx.uv, surfaceDx.uv1, second);
  vec2 dy = mix(surfaceDy.uv, surfaceDy.uv1, second);
  return textureGrad(source, surfaceTextureCoordinate(u, v, second),
                     vec2(dot(u.xy, dx), dot(v.xy, dx)),
                     vec2(dot(u.xy, dy), dot(v.xy, dy)));
}
#endif
SurfaceSample sampleSurfaceMaterial(inout M surface) {
  vec4 tap = vec4(1.0);
  vec4 orm = vec4(1.0);
  vec3 emission = surface.emissive;
  vec3 f0 = surface.f0;
  float f90 = surface.specularWeight;
  float roughness = surface.roughness;
  vec3 tangentDir = vec3(0.0);
  vec3 shadingNormal = facing(normal, SURFACE_FRONT_FACING);
#if LIT_TEXTURED
  tap = sampleSurfaceTexture(colourMap, surface.colourUvU, surface.colourUvV, surface.colourUvSecond);
  orm = sampleSurfaceTexture(metalRoughMap, surface.metalRoughUvU, surface.metalRoughUvV, surface.metalRoughUvSecond);
#if SURFACE_BASE_LOCATION == 0
  emission *= sampleSurfaceTexture(emissiveMap, surface.emissiveUvU, surface.emissiveUvV, surface.emissiveUvSecond).rgb;
  float strength = sampleSurfaceTexture(specularStrengthMap, surface.specularStrengthUvU, surface.specularStrengthUvV, surface.specularStrengthUvSecond).a;
  f0 *= strength * sampleSurfaceTexture(specularTintMap, surface.specularTintUvU, surface.specularTintUvV, surface.specularTintUvSecond).rgb;
  f90 *= strength;
#endif
#endif
#if SURFACE_BASE_LOCATION == 0 && LIT_TEXTURED
  if (surface.scattering > 1.5) {
    orm.g = pow(clamp(orm.g / max(orm.a, 1e-8), 0.0, 1.0), 0.25);
    orm.b /= max(orm.a, 1e-8);
  }
#endif
  roughness *= orm.g;
#if LIT_MAPPED
  vec4 mapped = sampleSurfaceTexture(normalMap, surface.normalUvU, surface.normalUvV, surface.normalUvSecond);
  shadingNormal = normalFromMap(normal, tangent, mapped.xyz * 2.0 - 1.0, surface.normalScale, SURFACE_FRONT_FACING);
  roughness = roughenedBy(roughness, mapped.w);
  tangentDir = tangent.xyz;
#endif
  vec3 albedo = surface.base.rgb * tap.rgb;
  float metalness = surface.metalness * orm.b;
  if (surface.pattern > 0.5) {
    vec2 heightChange;
    applyFacade(int(facadeCode), vec2(facadeBay, uv.y), colour.rgb, localPosition, groundWorldM, facing(normal, SURFACE_FRONT_FACING), surface.pattern > 1.5, uv1, surfaceDx, surfaceDy, albedo, roughness, heightChange);
    shadingNormal = bumpNormal(facing(normal, SURFACE_FRONT_FACING), shadingNormal, surfaceDx.position, surfaceDy.position, heightChange.x, heightChange.y);
  }
  else { albedo *= colour.rgb; }
#if SURFACE_BASE_LOCATION == 0
  if (surface.scattering > 0.5) {
    surface.thinReflection = albedo;
#if LIT_TEXTURED
    surface.diffuseTransmission *= sampleSurfaceTexture(diffuseTransmissionMap, surface.diffuseTransmissionUvU, surface.diffuseTransmissionUvV,
                   surface.diffuseTransmissionUvSecond).rgb;
    if (surface.scattering > 1.5) {
      float coverage = max(tap.a, 1e-8);
      vec4 sheet = sampleSurfaceTexture(diffuseReflectionMap, surface.diffuseReflectionUvU, surface.diffuseReflectionUvV,
                     surface.diffuseReflectionUvSecond);
      surface.thinWeight = clamp(sheet.a / coverage, 0.0, 1.0);
      surface.thinReflection = sheet.rgb / coverage;
      surface.diffuseTransmission /= coverage;
      albedo /= max(coverage * (1.0 - surface.thinWeight), 1e-8);
    }
#endif
    conserveThinSheetEnergy(surface);
  }
#endif
  return SurfaceSample(albedo, tap.a, shadingNormal, roughness, metalness,
                        emission, f0, f90, tangentDir);
}
