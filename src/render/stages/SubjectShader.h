#ifndef OUTSHINE_RENDER_SUBJECTSHADER_H
#define OUTSHINE_RENDER_SUBJECTSHADER_H

namespace outshine::Render {

inline const char *const kSubjectBindingsMsl = R"(
struct S { float4x4 mvp; float4 anc; float4x4 prevMvp; float4 prevAnc; float4x4 model; };

struct M { float factor; float cut; float metalness; float roughness;
           float4 base; packed_float3 emissive; float normalScale; float identity;

           packed_float3 f0;

           float specularWeight;

           float transmission; float thickness; float attenuationDistance;
           packed_float3 attenuationColour;

           packed_float3 sheenColour; float sheenRoughness;

           float clearcoat; float clearcoatRoughness;

           float anisotropy; float anisotropyRotation;

           float iridescence; float iridescenceIor;
           float iridescenceThicknessMin; float iridescenceThicknessMax;
           packed_float3 colourUvU; packed_float3 colourUvV;
           packed_float3 normalUvU; packed_float3 normalUvV;
           packed_float3 metalRoughUvU; packed_float3 metalRoughUvV;
           packed_float3 emissiveUvU; packed_float3 emissiveUvV;
           packed_float3 specularStrengthUvU; packed_float3 specularStrengthUvV;
           packed_float3 specularTintUvU; packed_float3 specularTintUvV;
           float colourUvSecond; float normalUvSecond;
           float metalRoughUvSecond; float emissiveUvSecond;
           float specularStrengthUvSecond; float specularTintUvSecond; };

struct Light { float4 tint; float4 place; float4 beam; float4 cone; };

struct Lights { float4 count; float4 environment; Light items[16]; };

#define SUBJECT_SURFACE constant M &surface [[buffer(0)]], constant Lights &lights [[buffer(1)]], \
    device const BvhNode *bvhNodes [[buffer(2)]], device const BvhTri *bvhTris [[buffer(3)]], \
    texture2d<float> colourMap [[texture(0)]], sampler colourSampler [[sampler(0)]], \
    texture2d<float> normalMap [[texture(1)]], sampler normalSampler [[sampler(1)]], \
    texture2d<float> metalRoughMap [[texture(2)]], sampler metalRoughSampler [[sampler(2)]], \
    texture2d<float> emissiveMap [[texture(3)]], sampler emissiveSampler [[sampler(3)]], \
    texture2d<float> specularStrengthMap [[texture(4)]], sampler specularStrengthSampler [[sampler(4)]], \
    texture2d<float> specularTintMap [[texture(5)]], sampler specularTintSampler [[sampler(5)]], \
    texture2d<float> behindMap [[texture(6)]], sampler behindSampler [[sampler(6)]]

struct Occluders { device const BvhNode *nodes; device const BvhTri *tris; };
#define SUBJECT_OCCLUDERS Occluders{bvhNodes, bvhTris}

struct Uvs { float2 first; float2 second; };
static inline float2 uvBy(packed_float3 u, packed_float3 v, Uvs uv, float second) {

  float3 homogeneous = float3(mix(uv.first, uv.second, second), 1.0);
  return float2(dot(float3(u), homogeneous), dot(float3(v), homogeneous));
}

#define SUBJECT_UVS(in) Uvs{(in).uv, (in).uv1}
#define SUBJECT_COLOUR_TAP(uv) \
  colourMap.sample(colourSampler, \
                   uvBy(surface.colourUvU, surface.colourUvV, (uv), surface.colourUvSecond))
#define SUBJECT_NORMAL_TAP(uv) \
  normalMap.sample(normalSampler, \
                   uvBy(surface.normalUvU, surface.normalUvV, (uv), surface.normalUvSecond))
#define SUBJECT_METALROUGH_TAP(uv) \
  metalRoughMap.sample(metalRoughSampler, uvBy(surface.metalRoughUvU, surface.metalRoughUvV, (uv), \
                                               surface.metalRoughUvSecond))
#define SUBJECT_SPECULAR_STRENGTH_TAP(uv) \
  specularStrengthMap.sample(specularStrengthSampler, \
                             uvBy(surface.specularStrengthUvU, surface.specularStrengthUvV, (uv), \
                                  surface.specularStrengthUvSecond))
#define SUBJECT_SPECULAR_TINT_TAP(uv) \
  specularTintMap.sample(specularTintSampler, \
                         uvBy(surface.specularTintUvU, surface.specularTintUvV, (uv), \
                              surface.specularTintUvSecond))

#define SUBJECT_SPECULAR_F0(uv) \
  (float3(surface.f0) * specularStrengthMap.sample(specularStrengthSampler, \
                            uvBy(surface.specularStrengthUvU, surface.specularStrengthUvV, (uv), \
                                 surface.specularStrengthUvSecond)).a \
                      * specularTintMap.sample(specularTintSampler, \
                            uvBy(surface.specularTintUvU, surface.specularTintUvV, (uv), \
                                 surface.specularTintUvSecond)).rgb)

#define SUBJECT_SPECULAR_F90(uv) \
  (surface.specularWeight * specularStrengthMap.sample(specularStrengthSampler, \
                                uvBy(surface.specularStrengthUvU, surface.specularStrengthUvV, (uv), \
                                     surface.specularStrengthUvSecond)).a)
#define SUBJECT_EMISSIVE_TAP(uv) \
  emissiveMap.sample(emissiveSampler, \
                     uvBy(surface.emissiveUvU, surface.emissiveUvV, (uv), surface.emissiveUvSecond))

struct SFrag {
  float4 col [[color(0)]];
#if SUBJECT_WRITES_VELOCITY
  float2 vel [[color(1)]];
#endif
#if SUBJECT_WRITES_SHADING_NORMAL

  float4 nrm [[color(SUBJECT_NORMAL_COLOUR_INDEX)]];
#endif
#if SUBJECT_WRITES_SURFACE_IDENTITY
  float4 idn [[color(SUBJECT_IDENTITY_COLOUR_INDEX)]];
#endif
};

#if SUBJECT_WRITES_VELOCITY
#define SUBJECT_PREV_ATTRIBUTE float3 prevP [[attribute(5)]];
#define SUBJECT_MOTION_VARYINGS float4 curClip; float4 prevClip;
#define SUBJECT_SET_MOTION(o, v, s) \
  (((o).curClip = (o).pos), \
   ((o).prevClip = (s).prevMvp * float4((v).prevP + (s).prevAnc.xyz, 1.0)))
#define SUBJECT_SET_VELOCITY(o, in) \
  (o).vel = (in).curClip.xy / (in).curClip.w - (in).prevClip.xy / (in).prevClip.w
#else
#define SUBJECT_PREV_ATTRIBUTE
#define SUBJECT_MOTION_VARYINGS
#define SUBJECT_SET_MOTION(o, v, s) (void)0
#define SUBJECT_SET_VELOCITY(o, in) (void)0
#endif

#define SUBJECT_UV_ATTRIBUTE float2 uv [[attribute(1)]];
#define SUBJECT_UV1_ATTRIBUTE float2 uv1 [[attribute(6)]];
#define SUBJECT_NORMAL_ATTRIBUTE float3 n [[attribute(3)]];
#define SUBJECT_TANGENT_ATTRIBUTE float4 t [[attribute(4)]];
#define SUBJECT_COLOUR_ATTRIBUTE float4 colour [[attribute(7)]];
#define SUBJECT_NO_COLOUR_ATTRIBUTE

#define SUBJECT_NO_VERTEX_COLOUR float4(1.0)

#if SUBJECT_WRITES_SHADING_NORMAL
#define SUBJECT_SET_SHADING_NORMAL(o, n, f) (o).nrm = float4((n), select(-1.0, 1.0, (f)))
#define SUBJECT_NO_SHADING_NORMAL(o) (o).nrm = float4(0.0, 0.0, 0.0, 1.0)
#else
#define SUBJECT_SET_SHADING_NORMAL(o, n, f) (void)0
#define SUBJECT_NO_SHADING_NORMAL(o) (void)0
#endif

#if SUBJECT_WRITES_SURFACE_IDENTITY
#define SUBJECT_SET_SURFACE_IDENTITY(o, m) (o).idn = float4((m).identity, 0.0, 0.0, 1.0)
#else
#define SUBJECT_SET_SURFACE_IDENTITY(o, m) (void)0
#endif

static inline float3 transmitted(constant M &surface, texture2d<float> behindMap,
                                 float2 screen, float3 albedo) {
  const float3 behind = behindMap.read(uint2(screen)).rgb;
  float3 medium = float3(1.0);
  if (surface.thickness > 0.0 && !isinf(surface.attenuationDistance)) {
    const float3 tint = max(float3(surface.attenuationColour), float3(1e-5));
    medium = exp(log(tint) * (surface.thickness / surface.attenuationDistance));
  }
  return behind * albedo * medium * surface.transmission;
}
)";

inline const char *const kSubjectMsl = R"(
struct SOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour;
              float3 emitted [[flat]]; SUBJECT_MOTION_VARYINGS };

#define SUBJECT_EMITTED_ARM(NAME, RUNS, UV, UV1, COLOUR) \
struct NAME##In { float3 p [[attribute(0)]]; float3 emitted [[attribute(2)]]; \
                  SUBJECT_PREV_ATTRIBUTE RUNS }; \
vertex SOut NAME(NAME##In v [[stage_in]], constant S &s [[buffer(0)]]) { \
  SOut o; \
  o.pos = s.mvp * float4(v.p + s.anc.xyz, 1.0); \
  o.uv = UV; \
  o.uv1 = UV1; \
  o.colour = COLOUR; \
  o.emitted = v.emitted; \
  SUBJECT_SET_MOTION(o, v, s); \
  return o; \
}

SUBJECT_EMITTED_ARM(vs, SUBJECT_NO_COLOUR_ATTRIBUTE,
                    float2(0.0), float2(0.0), SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_EMITTED_ARM(vsTextured, SUBJECT_UV_ATTRIBUTE,
                    v.uv, float2(0.0), SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_EMITTED_ARM(vsTexturedTwo, SUBJECT_UV_ATTRIBUTE SUBJECT_UV1_ATTRIBUTE,
                    v.uv, v.uv1, SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_EMITTED_ARM(vsTinted, SUBJECT_COLOUR_ATTRIBUTE,
                    float2(0.0), float2(0.0), v.colour)
SUBJECT_EMITTED_ARM(vsTexturedTinted, SUBJECT_UV_ATTRIBUTE SUBJECT_COLOUR_ATTRIBUTE,
                    v.uv, float2(0.0), v.colour)
SUBJECT_EMITTED_ARM(vsTexturedTwoTinted,
                    SUBJECT_UV_ATTRIBUTE SUBJECT_UV1_ATTRIBUTE SUBJECT_COLOUR_ATTRIBUTE,
                    v.uv, v.uv1, v.colour)

fragment SFrag fs(SOut in [[stage_in]], SUBJECT_SURFACE) {
  SFrag o;
  o.col = float4(in.emitted * in.colour.rgb, 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsMasked(SOut in [[stage_in]], SUBJECT_SURFACE) {
  if (surface.factor * in.colour.a < surface.cut) { discard_fragment(); }
  SFrag o;
  o.col = float4(in.emitted * in.colour.rgb, 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsTransmissive(SOut in [[stage_in]], SUBJECT_SURFACE) {
  SFrag o;
  o.col = float4(in.emitted * in.colour.rgb +
                     transmitted(surface, behindMap, in.pos.xy,
                                 surface.base.rgb * in.colour.rgb),
                 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsBlended(SOut in [[stage_in]], SUBJECT_SURFACE) {
  SFrag o;
  o.col = float4(in.emitted * in.colour.rgb, surface.factor * in.colour.a);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsTextured(SOut in [[stage_in]], SUBJECT_SURFACE) {
  SFrag o;
  o.col = float4(in.emitted * SUBJECT_COLOUR_TAP(SUBJECT_UVS(in)).rgb * in.colour.rgb, 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsMaskedTextured(SOut in [[stage_in]], SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  if (surface.factor * tap.a * in.colour.a < surface.cut) { discard_fragment(); }
  SFrag o;
  o.col = float4(in.emitted * tap.rgb * in.colour.rgb, 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsBlendedTextured(SOut in [[stage_in]], SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  SFrag o;
  o.col = float4(in.emitted * tap.rgb * in.colour.rgb, surface.factor * tap.a * in.colour.a);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}
)";

inline const char *const kSubjectLitMsl = R"(
struct LOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour; float3 n; float3 p;
              float3 lp; SUBJECT_MOTION_VARYINGS };

#define SUBJECT_LIT_ARM(NAME, RUNS, UV, UV1, COLOUR) \
struct NAME##In { float3 p [[attribute(0)]]; SUBJECT_NORMAL_ATTRIBUTE \
                  SUBJECT_PREV_ATTRIBUTE RUNS }; \
vertex LOut NAME(NAME##In v [[stage_in]], constant S &s [[buffer(0)]]) { \
  LOut o; \
  o.pos = s.mvp * float4(v.p + s.anc.xyz, 1.0); \
  o.uv = UV; \
  o.uv1 = UV1; \
  o.colour = COLOUR; \
  o.n = normalize(s.model[0].xyz * v.n.x + s.model[1].xyz * v.n.y + s.model[2].xyz * v.n.z); \
  o.p = (s.model * float4(v.p, 1.0)).xyz; \
  o.lp = v.p; \
  SUBJECT_SET_MOTION(o, v, s); \
  return o; \
}

SUBJECT_LIT_ARM(vsLit, SUBJECT_NO_COLOUR_ATTRIBUTE,
                float2(0.0), float2(0.0), SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_LIT_ARM(vsLitTextured, SUBJECT_UV_ATTRIBUTE,
                v.uv, float2(0.0), SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_LIT_ARM(vsLitTexturedTwo, SUBJECT_UV_ATTRIBUTE SUBJECT_UV1_ATTRIBUTE,
                v.uv, v.uv1, SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_LIT_ARM(vsLitTinted, SUBJECT_COLOUR_ATTRIBUTE,
                float2(0.0), float2(0.0), v.colour)
SUBJECT_LIT_ARM(vsLitTexturedTinted, SUBJECT_UV_ATTRIBUTE SUBJECT_COLOUR_ATTRIBUTE,
                v.uv, float2(0.0), v.colour)
SUBJECT_LIT_ARM(vsLitTexturedTwoTinted,
                SUBJECT_UV_ATTRIBUTE SUBJECT_UV1_ATTRIBUTE SUBJECT_COLOUR_ATTRIBUTE,
                v.uv, v.uv1, v.colour)

static inline float3 shadeRow(constant M &surface, constant Lights &lights, Occluders occluders,
                              float3 localM, float3 n, float3 p, float3 albedo, float metalness,
                              float roughness, float3 dielectricF0, float dielectricF90,
                              float3 emitted, float3 tangentDir) {
  float3 sheenColour = float3(surface.sheenColour);
  float sheenRoughness = surface.sheenRoughness;
  float clearcoat = surface.clearcoat;
  float clearcoatRoughness = surface.clearcoatRoughness;
  float anisotropy = surface.anisotropy;
  float anisotropyRotation = surface.anisotropyRotation;

  float iridescence = surface.iridescence;
  float iridescenceThickness = surface.iridescenceThicknessMax;

  if (!(iridescenceThickness > 0.0)) { iridescence = 0.0; }
  float3 v = normalize(-p);
  float a = roughness * roughness;
  float a2 = a * a;
  float3 diffuseColour = albedo * (1.0 - metalness) * (1.0 - surface.transmission);
  float3 f0 = mix(dielectricF0, albedo, metalness);

  float f90 = mix(dielectricF90, 1.0, metalness);
  float nv = max(dot(n, v), 1.0e-6);

  float3 energyScale = ggxEnergyScale(f0, roughness, nv);

  float anisoLen = length(tangentDir);
  bool anisotropic = anisotropy > 0.0 && anisoLen > 0.0;
  float3 anisoT = float3(1.0, 0.0, 0.0);
  float3 anisoB = float3(0.0, 1.0, 0.0);
  if (anisotropic) {
    float3 alongT = tangentDir / anisoLen;
    float3 alongB = normalize(cross(n, alongT));
    float turnC = cos(anisotropyRotation);
    float turnS = sin(anisotropyRotation);
    anisoT = normalize(alongT * turnC + alongB * turnS);
    anisoB = normalize(cross(n, anisoT));
  }
  float3 originM = localM + n * lights.count.y;
  float3 sum = float3(0.0);
  int count = int(lights.count.x);
  for (int at = 0; at < count; at = at + 1) {
    Light light = lights.items[at];
    float3 toward = -light.beam.xyz;
    float attenuation = 1.0;
    float reachM = INFINITY;
    if (light.tint.w > 0.5) {
      float3 offset = light.place.xyz - p;
      float square = dot(offset, offset);
      if (square <= 0.0) { continue; }
      toward = offset * rsqrt(square);
      reachM = sqrt(square);

      float reach = square * light.place.w * light.place.w;
      attenuation = clamp(1.0 - reach * reach, 0.0, 1.0) / square;
    }
    if (light.tint.w > 1.5) {
      attenuation = attenuation *
                    clamp((dot(light.beam.xyz, -toward) - light.cone.x) * light.cone.y, 0.0, 1.0);
    }
    float nl = dot(n, toward);
    if (nl <= 0.0 || attenuation <= 0.0) { continue; }

    if (bvhOccludes(occluders.nodes, occluders.tris, originM, toward, lights.count.y, reachM)) {
      continue;
    }
    float3 h = normalize(toward + v);
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

      float3 filmed = iridescenceFresnel(vh, iridescenceThickness, surface.iridescenceIor, f0);
      reflected = brdfRgbMix(diffuseColour, mix(brdfFresnel(f0, f90, vh), filmed, iridescence), lobe);
    } else {
      reflected = brdfCombine(diffuseColour, brdfFresnel(f0, f90, vh), lobe);
    }
    reflected.specular *= energyScale;

    float3 sheen = sheenColour * sheenDistribution(nh, sheenRoughness) *
                   sheenVisibility(nl, nv, sheenRoughness);
    float keep = sheenAlbedoScaling(sheenColour, nv, sheenRoughness);
    float3 layered = (reflected.diffuse + reflected.specular) * keep + sheen;

    if (clearcoat > 0.0) {
      float coatA = clearcoatRoughness * clearcoatRoughness;
      float coatA2 = coatA * coatA;

      float coatF = 0.04 + 0.96 * pow(1.0 - nv, 5.0);
      float coatLobe = brdfLobe(coatA2, nl, nv, nh);
      float weight = clearcoat * coatF;
      layered = layered * (1.0 - weight) + float3(weight * coatLobe);
    }
    sum = sum + layered * nl * attenuation * light.tint.rgb;
  }

  const float nvClamped = clamp(nv, 0.0, 1.0);

  const float3 specularEnvironment = brdfFresnel(f0, f90, nvClamped);
  sum = sum + lights.environment.rgb * (diffuseColour + specularEnvironment);
  return sum + emitted;
}

static inline float3 shade(constant M &surface, constant Lights &lights, Occluders occluders,
                           float3 localM, float3 n, float3 p, float3 albedo) {
  return shadeRow(surface, lights, occluders, localM, n, p, albedo, surface.metalness,
                  surface.roughness, float3(surface.f0), surface.specularWeight,
                  surface.emissive, float3(0.0));
}

static inline float3 facing(float3 n, bool front) {
  return select(-normalize(n), normalize(n), front);
}

fragment SFrag fsLit(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shade(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                       surface.base.rgb * in.colour.rgb), 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal, front);
  return o;
}

fragment SFrag fsLitMasked(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  if (surface.factor * in.colour.a < surface.cut) { discard_fragment(); }
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shade(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                       surface.base.rgb * in.colour.rgb), 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal, front);
  return o;
}

fragment SFrag fsLitTransmissive(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  const float3 shadingNormal = facing(in.n, front);
  const float3 albedo = surface.base.rgb * in.colour.rgb;
  SFrag o;
  o.col = float4(shade(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p, albedo) +
                     transmitted(surface, behindMap, in.pos.xy, albedo),
                 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal, front);
  return o;
}

fragment SFrag fsLitBlended(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shade(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                       surface.base.rgb * in.colour.rgb),
                 surface.factor * in.colour.a);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal, front);
  return o;
}
)";

inline const char *const kSubjectLitTexturedMsl = R"(
static inline float3 emittedAt(constant M &surface, texture2d<float> emissiveMap,
                               sampler emissiveSampler, Uvs uv) {
  return surface.emissive * SUBJECT_EMISSIVE_TAP(uv).rgb;
}

fragment SFrag fsLitTextured(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shadeRow(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                          surface.base.rgb * tap.rgb * in.colour.rgb,
                          surface.metalness, surface.roughness, SUBJECT_SPECULAR_F0(SUBJECT_UVS(in)),
                          SUBJECT_SPECULAR_F90(SUBJECT_UVS(in)),
                          emittedAt(surface, emissiveMap, emissiveSampler, SUBJECT_UVS(in)),
                          float3(0.0)), 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal, front);
  return o;
}

fragment SFrag fsLitMaskedTextured(LOut in [[stage_in]], bool front [[front_facing]],
                                   SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  if (surface.factor * tap.a * in.colour.a < surface.cut) { discard_fragment(); }
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shadeRow(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                          surface.base.rgb * tap.rgb * in.colour.rgb,
                          surface.metalness, surface.roughness, SUBJECT_SPECULAR_F0(SUBJECT_UVS(in)),
                          SUBJECT_SPECULAR_F90(SUBJECT_UVS(in)),
                          emittedAt(surface, emissiveMap, emissiveSampler, SUBJECT_UVS(in)),
                          float3(0.0)), 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal, front);
  return o;
}

fragment SFrag fsLitBlendedTextured(LOut in [[stage_in]], bool front [[front_facing]],
                                    SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shadeRow(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                          surface.base.rgb * tap.rgb * in.colour.rgb,
                          surface.metalness, surface.roughness, SUBJECT_SPECULAR_F0(SUBJECT_UVS(in)),
                          SUBJECT_SPECULAR_F90(SUBJECT_UVS(in)),
                          emittedAt(surface, emissiveMap, emissiveSampler, SUBJECT_UVS(in)),
                          float3(0.0)),
                 surface.factor * tap.a * in.colour.a);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal, front);
  return o;
}
)";

inline const char *const kSubjectMappedMsl = R"(
struct MOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour; float3 n; float3 p;
              float3 lp; float4 t; SUBJECT_MOTION_VARYINGS };

#define SUBJECT_MAPPED_ARM(NAME, RUNS, UV1, COLOUR) \
struct NAME##In { float3 p [[attribute(0)]]; SUBJECT_UV_ATTRIBUTE SUBJECT_NORMAL_ATTRIBUTE \
                  SUBJECT_TANGENT_ATTRIBUTE SUBJECT_PREV_ATTRIBUTE RUNS }; \
vertex MOut NAME(NAME##In v [[stage_in]], constant S &s [[buffer(0)]]) { \
  MOut o; \
  o.pos = s.mvp * float4(v.p + s.anc.xyz, 1.0); \
  o.uv = v.uv; \
  o.uv1 = UV1; \
  o.colour = COLOUR; \
  o.n = normalize(s.model[0].xyz * v.n.x + s.model[1].xyz * v.n.y + s.model[2].xyz * v.n.z); \
  o.p = (s.model * float4(v.p, 1.0)).xyz; \
  o.lp = v.p; \
  o.t = float4(normalize(s.model[0].xyz * v.t.x + s.model[1].xyz * v.t.y + \
                         s.model[2].xyz * v.t.z), \
               v.t.w); \
  SUBJECT_SET_MOTION(o, v, s); \
  return o; \
}

SUBJECT_MAPPED_ARM(vsMapped, SUBJECT_NO_COLOUR_ATTRIBUTE, float2(0.0), SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_MAPPED_ARM(vsMappedTwo, SUBJECT_UV1_ATTRIBUTE, v.uv1, SUBJECT_NO_VERTEX_COLOUR)
SUBJECT_MAPPED_ARM(vsMappedTinted, SUBJECT_COLOUR_ATTRIBUTE, float2(0.0), v.colour)
SUBJECT_MAPPED_ARM(vsMappedTwoTinted, SUBJECT_UV1_ATTRIBUTE SUBJECT_COLOUR_ATTRIBUTE,
                   v.uv1, v.colour)

static inline float3 mappedNormal(constant M &surface, texture2d<float> normalMap,
                                  sampler normalSampler, MOut in, bool front) {
  float3 tap = SUBJECT_NORMAL_TAP(SUBJECT_UVS(in)).xyz * 2.0 - 1.0;
  return normalFromMap(in.n, in.t, tap, surface.normalScale, front);
}

struct Shaded { float3 col; float3 nrm; };

static inline Shaded mappedShade(constant M &surface, constant Lights &lights, Occluders occluders,
                                 texture2d<float> colourMap, sampler colourSampler,
                                 texture2d<float> normalMap, sampler normalSampler,
                                 texture2d<float> metalRoughMap, sampler metalRoughSampler,
                                 texture2d<float> emissiveMap, sampler emissiveSampler,
                                 texture2d<float> specularStrengthMap,
                                 sampler specularStrengthSampler,
                                 texture2d<float> specularTintMap, sampler specularTintSampler,
                                 MOut in, bool front) {
  float4 orm = SUBJECT_METALROUGH_TAP(SUBJECT_UVS(in));

  const float3 specularF0 = SUBJECT_SPECULAR_F0(SUBJECT_UVS(in));
  float3 albedo = surface.base.rgb * SUBJECT_COLOUR_TAP(SUBJECT_UVS(in)).rgb * in.colour.rgb;
  const float3 shadingNormal = mappedNormal(surface, normalMap, normalSampler, in, front);

  const float meanResultantLength = SUBJECT_NORMAL_TAP(SUBJECT_UVS(in)).w;
  return Shaded{shadeRow(surface, lights, occluders, in.lp, shadingNormal, in.p, albedo,
                         surface.metalness * orm.b,
                         roughenedBy(surface.roughness * orm.g, meanResultantLength), specularF0,
                         SUBJECT_SPECULAR_F90(SUBJECT_UVS(in)),
                         emittedAt(surface, emissiveMap, emissiveSampler, SUBJECT_UVS(in)),
                         in.t.xyz),
                shadingNormal};
}

#define SUBJECT_MAPPED_SHADE mappedShade(surface, lights, SUBJECT_OCCLUDERS, colourMap, \
    colourSampler, normalMap, normalSampler, metalRoughMap, metalRoughSampler, emissiveMap, \
    emissiveSampler, specularStrengthMap, specularStrengthSampler, specularTintMap, \
    specularTintSampler, in, front)

fragment SFrag fsMapped(MOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  const Shaded shaded = SUBJECT_MAPPED_SHADE;
  SFrag o;
  o.col = float4(shaded.col, 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shaded.nrm, front);
  return o;
}

fragment SFrag fsMappedMasked(MOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  if (surface.factor * tap.a * in.colour.a < surface.cut) { discard_fragment(); }
  const Shaded shaded = SUBJECT_MAPPED_SHADE;
  SFrag o;
  o.col = float4(shaded.col, 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shaded.nrm, front);
  return o;
}

fragment SFrag fsMappedTransmissive(MOut in [[stage_in]], bool front [[front_facing]],
                                    SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  const Shaded shaded = SUBJECT_MAPPED_SHADE;
  SFrag o;
  o.col = float4(shaded.col + transmitted(surface, behindMap, in.pos.xy,
                                          surface.base.rgb * tap.rgb * in.colour.rgb),
                 1.0);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shaded.nrm, front);
  return o;
}

fragment SFrag fsMappedBlended(MOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  float4 tap = SUBJECT_COLOUR_TAP(SUBJECT_UVS(in));
  const Shaded shaded = SUBJECT_MAPPED_SHADE;
  SFrag o;
  o.col = float4(shaded.col, surface.factor * tap.a * in.colour.a);
  SUBJECT_SET_VELOCITY(o, in);
  SUBJECT_SET_SURFACE_IDENTITY(o, surface);
  SUBJECT_SET_SHADING_NORMAL(o, shaded.nrm, front);
  return o;
}
)";

inline const char *const kDepthOnlyMsl = R"(
struct S { float4x4 mvp; };
struct VIn { float3 p [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; };
vertex VOut vsDepth(VIn v [[stage_in]], constant S &s [[buffer(0)]]) {
  VOut o;
  o.pos = s.mvp * float4(v.p, 1.0);
  return o;
}
fragment void fsDepth(VOut in [[stage_in]]) {}
)";

}

#endif
