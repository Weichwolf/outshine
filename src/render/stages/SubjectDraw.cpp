#include "SubjectDraw.h"

#include <new>

#include "Heap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "MetalRoughBrdf.h"
#include "IridescenceLobe.h"
#include "MicrofacetEnergy.h"
#include "SheenLobe.h"
#include "NormalFromMap.h"
#include "SceneTargets.h"
#include "ShaderPrelude.h"
#include "ShadowRay.h"
#include "SurfaceState.h"
#include "TriangleBvh.h"

namespace outshine::Render {

namespace {

constexpr SDL_GPUFrontFace kGltfFrontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

constexpr Winding kSubjectWinding = Winding::Trusted;

float LinearFromSrgb8(uint8_t code) {
  const float encoded = static_cast<float>(code) * (1.0f / 255.0f);
  if (encoded < 0.04045f) { return encoded * (1.0f / 12.92f); }
  return std::pow((encoded + 0.055f) * (1.0f / 1.055f), 2.4f);
}

SDL_GPUSamplerAddressMode AddressOf(SubjectWrap wrap) {
  switch (wrap) {
    case SubjectWrap::ClampToEdge: return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    case SubjectWrap::MirroredRepeat: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    case SubjectWrap::Repeat: return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  }
  return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
}

SDL_GPUFilter FilterOf(SubjectFilter filter) {
  return filter == SubjectFilter::Nearest ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
}

SDL_GPUColorTargetBlendState OverBlend() {
  SDL_GPUColorTargetBlendState blend{};
  blend.enable_blend = true;
  blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
  blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  return blend;
}

const char *FragmentEntryPoint(SurfaceKind kind, VertexLayout layout) {
  const bool textured = CarriesUv(layout);
  if (CarriesTangent(layout)) {
    switch (kind) {
      case SurfaceKind::Masked: return "fsMappedMasked";
      case SurfaceKind::Blended: return "fsMappedBlended";
      case SurfaceKind::ThinTransmissive:
      case SurfaceKind::Refractive: return "fsMappedTransmissive";
      case SurfaceKind::Opaque: break;
    }
    return "fsMapped";
  }
  if (CarriesNormal(layout)) {
    switch (kind) {
      case SurfaceKind::Masked: return textured ? "fsLitMaskedTextured" : "fsLitMasked";
      case SurfaceKind::Blended: return textured ? "fsLitBlendedTextured" : "fsLitBlended";
      case SurfaceKind::ThinTransmissive:
      case SurfaceKind::Refractive: return "fsLitTransmissive";
      case SurfaceKind::Opaque: break;
    }
    return textured ? "fsLitTextured" : "fsLit";
  }
  switch (kind) {
    case SurfaceKind::Masked: return textured ? "fsMaskedTextured" : "fsMasked";
    case SurfaceKind::Blended: return textured ? "fsBlendedTextured" : "fsBlended";
    case SurfaceKind::ThinTransmissive:
    case SurfaceKind::Refractive: return "fsTransmissive";
    case SurfaceKind::Opaque: break;
  }
  return textured ? "fsTextured" : "fs";
}

const char *VertexEntryPoint(VertexLayout layout) {
  switch (layout) {
    case VertexLayout::Position: return "vs";
    case VertexLayout::PositionUv: return "vsTextured";
    case VertexLayout::PositionUvUv1: return "vsTexturedTwo";
    case VertexLayout::PositionNormal: return "vsLit";
    case VertexLayout::PositionNormalUv: return "vsLitTextured";
    case VertexLayout::PositionNormalUvUv1: return "vsLitTexturedTwo";
    case VertexLayout::PositionNormalUvTangent: return "vsMapped";
    case VertexLayout::PositionNormalUvUv1Tangent: return "vsMappedTwo";
    case VertexLayout::PositionColour: return "vsTinted";
    case VertexLayout::PositionUvColour: return "vsTexturedTinted";
    case VertexLayout::PositionUvUv1Colour: return "vsTexturedTwoTinted";
    case VertexLayout::PositionNormalColour: return "vsLitTinted";
    case VertexLayout::PositionNormalUvColour: return "vsLitTexturedTinted";
    case VertexLayout::PositionNormalUvUv1Colour: return "vsLitTexturedTwoTinted";
    case VertexLayout::PositionNormalUvTangentColour: return "vsMappedTinted";
    case VertexLayout::PositionNormalUvUv1TangentColour: return "vsMappedTwoTinted";
  }
  return "vs";
}

const char *KindName(SurfaceKind kind) {
  switch (kind) {
    case SurfaceKind::Opaque: return "OPAQUE";
    case SurfaceKind::Masked: return "MASK";
    case SurfaceKind::Blended: return "BLEND";
    case SurfaceKind::ThinTransmissive: return "a thin transmissive sheet";
    case SurfaceKind::Refractive: return "a refracting volume";
  }
  return "an undeclared surface";
}

}

static const char *kSubjectBindingsMsl = R"(
struct S { float4x4 mvp; float4 anc; float4x4 prevMvp; float4 prevAnc; };

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

static const char *kSubjectMsl = R"(
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

static const char *kSubjectLitMsl = R"(
struct LOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour; float3 n; float3 p;
              float3 lp; SUBJECT_MOTION_VARYINGS };

#define SUBJECT_LIT_ARM(NAME, RUNS, UV, UV1, COLOUR) \
struct NAME##In { float3 p [[attribute(0)]]; SUBJECT_NORMAL_ATTRIBUTE \
                  SUBJECT_PREV_ATTRIBUTE RUNS }; \
vertex LOut NAME(NAME##In v [[stage_in]], constant S &s [[buffer(0)]]) { \
  LOut o; \
  float3 placed = v.p + s.anc.xyz; \
  o.pos = s.mvp * float4(placed, 1.0); \
  o.uv = UV; \
  o.uv1 = UV1; \
  o.colour = COLOUR; \
  o.n = v.n; \
  o.p = placed; \
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

static const char *kSubjectLitTexturedMsl = R"(
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

static const char *kSubjectMappedMsl = R"(
struct MOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour; float3 n; float3 p;
              float3 lp; float4 t; SUBJECT_MOTION_VARYINGS };

#define SUBJECT_MAPPED_ARM(NAME, RUNS, UV1, COLOUR) \
struct NAME##In { float3 p [[attribute(0)]]; SUBJECT_UV_ATTRIBUTE SUBJECT_NORMAL_ATTRIBUTE \
                  SUBJECT_TANGENT_ATTRIBUTE SUBJECT_PREV_ATTRIBUTE RUNS }; \
vertex MOut NAME(NAME##In v [[stage_in]], constant S &s [[buffer(0)]]) { \
  MOut o; \
  float3 placed = v.p + s.anc.xyz; \
  o.pos = s.mvp * float4(placed, 1.0); \
  o.uv = v.uv; \
  o.uv1 = UV1; \
  o.colour = COLOUR; \
  o.n = v.n; \
  o.p = placed; \
  o.lp = v.p; \
  o.t = v.t; \
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

namespace {

struct VertexShape {

  static constexpr uint32_t kRuns = 7;
  SDL_GPUVertexBufferDescription Buffers[kRuns];
  SDL_GPUVertexAttribute Attributes[kRuns];
  uint32_t Count = 0;
};

SDL_GPUVertexBufferDescription Run(uint32_t slot, uint32_t floats) {
  SDL_GPUVertexBufferDescription description{};
  description.slot = slot;
  description.pitch = floats * (uint32_t)sizeof(float);
  description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  return description;
}

SDL_GPUVertexAttribute At(uint32_t location, uint32_t slot, SDL_GPUVertexElementFormat format) {
  SDL_GPUVertexAttribute attribute{};
  attribute.location = location;
  attribute.buffer_slot = slot;
  attribute.format = format;
  attribute.offset = 0;
  return attribute;
}

VertexShape ShapeOf(VertexLayout layout, bool writesVelocity) {
  const bool textured = CarriesUv(layout);
  const bool lit = CarriesNormal(layout);
  const bool mapped = CarriesTangent(layout);
  VertexShape shape;
  shape.Buffers[shape.Count] = Run(shape.Count, 3);
  shape.Attributes[shape.Count] = At(0, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
  ++shape.Count;
  if (textured) {
    shape.Buffers[shape.Count] = Run(shape.Count, 2);
    shape.Attributes[shape.Count] = At(1, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
    ++shape.Count;
  }

  if (CarriesUv1(layout)) {
    shape.Buffers[shape.Count] = Run(shape.Count, 2);
    shape.Attributes[shape.Count] = At(6, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
    ++shape.Count;
  }

  shape.Buffers[shape.Count] = Run(shape.Count, 3);
  shape.Attributes[shape.Count] = At(lit ? 3 : 2, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
  ++shape.Count;
  if (mapped) {
    shape.Buffers[shape.Count] = Run(shape.Count, 4);
    shape.Attributes[shape.Count] = At(4, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
    ++shape.Count;
  }

  if (CarriesColour(layout)) {
    shape.Buffers[shape.Count] = Run(shape.Count, 4);
    shape.Attributes[shape.Count] = At(7, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
    ++shape.Count;
  }

  if (writesVelocity) {
    shape.Buffers[shape.Count] = Run(shape.Count, 3);
    shape.Attributes[shape.Count] = At(5, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
    ++shape.Count;
  }
  return shape;
}

constexpr uint32_t kSubjectFragmentUniforms = 2;

constexpr uint32_t kSubjectStorageBuffers = 2;

SDL_GPUShader *MakeShader(SDL_GPUDevice *device, const std::string &source, const char *entry,
                          SDL_GPUShaderStage stage) {
  const bool fragment = stage == SDL_GPU_SHADERSTAGE_FRAGMENT;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.entrypoint = entry;
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.stage = stage;
  wanted.num_samplers = fragment ? kSubjectImages : 0;
  wanted.num_storage_buffers = fragment ? kSubjectStorageBuffers : 0;
  wanted.num_uniform_buffers = fragment ? kSubjectFragmentUniforms : 1;
  return SDL_CreateGPUShader(device, &wanted);
}

}

bool SubjectDraw::Configure(const Gpu &gpu, std::string &error) {
  Device = gpu.Device;
  FiltersFloat32 = gpu.FiltersFloat32;

  Colours.clear();
  for (const Resource colour : gpu.SceneColours) { Colours.push_back(colour); }
  const auto attachmentIndex = [this](Resource which) -> long {
    const auto at = std::find(Colours.begin(), Colours.end(), which);
    return at == Colours.end() ? -1 : (long)(at - Colours.begin());
  };
  WritesVelocity = attachmentIndex(Resource::SceneVelocity) >= 0;
  const bool writesVelocity = WritesVelocity;
  const long normalIndex = attachmentIndex(Resource::SceneShadingNormal);
  const long identityIndex = attachmentIndex(Resource::SceneSurfaceIdentity);

  const std::string source = std::string(kMslPrelude) + kVelocityMsl + ShadowRayMsl() +
                             "\n#define SUBJECT_WRITES_VELOCITY " + (writesVelocity ? "1" : "0") +
                             "\n#define SUBJECT_WRITES_SHADING_NORMAL " +
                             (normalIndex >= 0 ? "1" : "0") +
                             "\n#define SUBJECT_NORMAL_COLOUR_INDEX " +
                             std::to_string(normalIndex < 0 ? 0 : normalIndex) +
                             "\n#define SUBJECT_WRITES_SURFACE_IDENTITY " +
                             (identityIndex >= 0 ? "1" : "0") +
                             "\n#define SUBJECT_IDENTITY_COLOUR_INDEX " +
                             std::to_string(identityIndex < 0 ? 0 : identityIndex) +
                             "\n" + kSubjectBindingsMsl + kSubjectMsl + MetalRoughBrdfMsl() + SheenLobeMsl() + IridescenceLobeMsl() + MicrofacetEnergyMsl() +
                             kSubjectLitMsl + kSubjectLitTexturedMsl + NormalFromMapMsl() +
                             kSubjectMappedMsl;

  Built = 0;

  const bool glass = Behind != nullptr;
  for (const SurfaceKind kind : {SurfaceKind::Opaque, SurfaceKind::Masked, SurfaceKind::Blended,
                                 SurfaceKind::ThinTransmissive, SurfaceKind::Refractive}) {
    if (!glass && (kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive)) {
      continue;
    }

    const bool blends = kind == SurfaceKind::Blended;
    SDL_GPUColorTargetDescription targets[kMaxColourAttachments] = {};
    targets[0].format = gpu.HdrFormat;
    if (blends) { targets[0].blend_state = OverBlend(); }

    if (writesVelocity) { targets[attachmentIndex(Resource::SceneVelocity)] = VelocityTarget(!blends); }

    if (normalIndex >= 0) { targets[normalIndex].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; }

    if (identityIndex >= 0) {
      targets[identityIndex].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    }

    for (const VertexLayoutRow &row : kVertexLayouts) {
      const VertexLayout layout = row.Layout;
      const VertexShape shape = ShapeOf(layout, WritesVelocity);
      const OwnedShader vertex(Device, MakeShader(Device, source, VertexEntryPoint(layout),
                                                  SDL_GPU_SHADERSTAGE_VERTEX));
      const OwnedShader fragment(Device, MakeShader(Device, source, FragmentEntryPoint(kind, layout),
                                                   SDL_GPU_SHADERSTAGE_FRAGMENT));
      if (!vertex || !fragment) {
        error = std::string("the subject's shader did not compile at ") +
                VertexEntryPoint(layout) + "/" + FragmentEntryPoint(kind, layout) + ": " +
                SDL_GetError();
        return false;
      }
      SDL_GPUGraphicsPipelineCreateInfo wanted{};
      wanted.vertex_shader = vertex.Get();
      wanted.fragment_shader = fragment.Get();
      wanted.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
      wanted.vertex_input_state.vertex_buffer_descriptions = shape.Buffers;
      wanted.vertex_input_state.num_vertex_buffers = shape.Count;
      wanted.vertex_input_state.vertex_attributes = shape.Attributes;
      wanted.vertex_input_state.num_vertex_attributes = shape.Count;
      wanted.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
      wanted.rasterizer_state.front_face = kGltfFrontFace;
      wanted.target_info.color_target_descriptions = targets;
      wanted.target_info.num_color_targets = (Uint32)Colours.size();
      wanted.target_info.has_depth_stencil_target = true;
      wanted.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
      wanted.depth_stencil_state.enable_depth_test = true;
      wanted.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;

      wanted.depth_stencil_state.enable_depth_write = !blends;

      for (const bool cullsBack : {false, true}) {
        wanted.rasterizer_state.cull_mode =
            cullsBack ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
        SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(Device, &wanted);
        if (!made) {
          error = std::string("the subject's pipeline was refused at ") +
                  FragmentEntryPoint(kind, layout) + ": " + SDL_GetError();
          return false;
        }
        Pipelines[PipelineAt(layout, kind, cullsBack)] = OwnedPipeline(Device, made);
        ++Built;
      }
    }
  }
  return true;
}

size_t SubjectDraw::PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack) {
  const size_t at = static_cast<size_t>(kind);
  return (static_cast<size_t>(layout) * 2u + (cullsBack ? 1u : 0u)) * kSurfaceKinds + at;
}

OwnedBuffer SubjectDraw::Fill(SDL_GPUBufferUsageFlags usage, const void *from, uint32_t bytes) {
  SDL_GPUBufferCreateInfo wantedBuffer{};
  wantedBuffer.usage = usage;
  wantedBuffer.size = bytes;
  OwnedBuffer buffer(Device, SDL_CreateGPUBuffer(Device, &wantedBuffer));
  if (!buffer) { return buffer; }

  SDL_GPUTransferBufferCreateInfo wantedTransfer{};
  wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  wantedTransfer.size = bytes;
  SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(Device, &wantedTransfer);
  if (!staging) {
    buffer.Reset();
    return buffer;
  }
  std::memcpy(SDL_MapGPUTransferBuffer(Device, staging, false), from, bytes);
  SDL_UnmapGPUTransferBuffer(Device, staging);

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTransferBufferLocation source{staging, 0};
  SDL_GPUBufferRegion into{buffer.Get(), 0, bytes};
  SDL_UploadToGPUBuffer(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(Device, staging);
  return buffer;
}

bool SubjectDraw::Cross(Crossing *what, size_t count, bool deferred, std::string &error) {
  uint32_t total = 0;
  for (size_t at = 0; at < count; ++at) {
    Crossing &one = what[at];
    if (one.Bytes == 0 || one.From == nullptr) {
      one.Into->Reset();
      *one.Held = 0;
      continue;
    }
    if (*one.Held != one.Bytes || !*one.Into) {
      SDL_GPUBufferCreateInfo wanted{};
      wanted.usage = one.Usage;
      wanted.size = one.Bytes;
      *one.Into = OwnedBuffer(Device, SDL_CreateGPUBuffer(Device, &wanted));
      if (!*one.Into) {
        *one.Held = 0;
        error = std::string("a vertex stream found no room on the device: ") + SDL_GetError();
        return false;
      }
      *one.Held = one.Bytes;
    }

    total = (total + one.Bytes + 15u) & ~15u;
  }
  if (total == 0) { return true; }

  if (!deferred) { return Submit(what, count, total, error); }
  const uint32_t wanted = StagingUsed_ + total;
  if (StagingBytes_ < wanted || !Staging_[StagingAt_]) {
    SDL_GPUTransferBufferCreateInfo room{};
    room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    room.size = wanted > StagingBytes_ ? wanted : StagingBytes_;
    if (wanted > StagingBytes_) { StagingBytes_ = wanted; }
    for (size_t slot = 0; slot < kStagingRing; ++slot) {
      Staging_[slot] = OwnedTransfer(Device, SDL_CreateGPUTransferBuffer(Device, &room));
      if (!Staging_[slot]) {
        error = std::string("the pose's staging buffer found no room on the device: ") + SDL_GetError();
        return false;
      }
    }
  }

  auto *const mapped =
      static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(Device, Staging_[StagingAt_].Get(), false));
  if (mapped == nullptr) {
    error = std::string("the pose's staging buffer did not map: ") + SDL_GetError();
    return false;
  }
  uint32_t at = StagingUsed_;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || what[one].From == nullptr) { continue; }
    std::memcpy(mapped + at, what[one].From, what[one].Bytes);
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_UnmapGPUTransferBuffer(Device, Staging_[StagingAt_].Get());

  at = StagingUsed_;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || what[one].From == nullptr) { continue; }
    if (StagedCount_ >= kStagedCrossings) {
      error = "a frame stages more runs than the " + std::to_string(kStagedCrossings) +
              " this subject declares room for";
      return false;
    }
    Staged_[StagedCount_++] =
        Staged{what[one].Into->Get(), at, what[one].Bytes, Staging_[StagingAt_].Get()};
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  StagingUsed_ = at;
  return true;
}

bool SubjectDraw::Submit(Crossing *what, size_t count, uint32_t total, std::string &error) {
  SDL_GPUTransferBufferCreateInfo room{};
  room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  room.size = total;
  OwnedTransfer once(Device, SDL_CreateGPUTransferBuffer(Device, &room));
  if (!once) {
    error = std::string("the topology's staging buffer found no room on the device: ") + SDL_GetError();
    return false;
  }
  auto *const mapped = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(Device, once.Get(), false));
  if (mapped == nullptr) {
    error = std::string("the topology's staging buffer did not map: ") + SDL_GetError();
    return false;
  }
  uint32_t at = 0;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || what[one].From == nullptr) { continue; }
    std::memcpy(mapped + at, what[one].From, what[one].Bytes);
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_UnmapGPUTransferBuffer(Device, once.Get());

  SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(Device);
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  at = 0;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || what[one].From == nullptr) { continue; }
    const SDL_GPUTransferBufferLocation source{once.Get(), at};
    const SDL_GPUBufferRegion into{what[one].Into->Get(), 0, what[one].Bytes};
    SDL_UploadToGPUBuffer(copy, &source, &into, false);
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  return true;
}

void SubjectDraw::FlushCrossings(SDL_GPUCommandBuffer *commands) {
  if (StagedCount_ == 0 || commands == nullptr) { return; }
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  for (size_t at = 0; at < StagedCount_; ++at) {
    const SDL_GPUTransferBufferLocation source{Staged_[at].Staging, Staged_[at].From};
    const SDL_GPUBufferRegion into{Staged_[at].Into, 0, Staged_[at].Bytes};
    SDL_UploadToGPUBuffer(copy, &source, &into, false);
  }
  SDL_EndGPUCopyPass(copy);
  StagedCount_ = 0;
  StagingUsed_ = 0;
  StagingAt_ = (StagingAt_ + 1) % kStagingRing;
}

constexpr bool kChainIsReadable = false;

SubjectDraw::BoundImage SubjectDraw::Upload(const SubjectTexture &texture, Transfer decode,
                                            TexelKind kind) {
  static const uint8_t white[4] = {255, 255, 255, 255};
  const uint32_t width = texture.Width > 0 ? texture.Width : 1;
  const uint32_t height = texture.Height > 0 ? texture.Height : 1;
  const uint8_t *texels = texture.Rgba ? texture.Rgba : white;
  std::vector<float> linear(static_cast<size_t>(width) * height * 4u, 0.0f);
  for (size_t texel = 0; texel < linear.size() / 4u; ++texel) {
    for (size_t channel = 0; channel < 3; ++channel) {
      const uint8_t code = texels[texel * 4u + channel];
      linear[texel * 4u + channel] = decode == Transfer::Srgb
                                         ? LinearFromSrgb8(code)
                                         : static_cast<float>(code) / 255.0f;
    }
    linear[texel * 4u + 3u] = static_cast<float>(texels[texel * 4u + 3u]) / 255.0f;
  }

  if (kind == TexelKind::Direction) {
    for (size_t texel = 0; texel < linear.size() / 4u; ++texel) { linear[texel * 4u + 3u] = 1.0f; }
  }

  BoundImage bound;
  SDL_GPUTextureCreateInfo wantedTexture{};
  wantedTexture.type = SDL_GPU_TEXTURETYPE_2D;
  wantedTexture.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  wantedTexture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedTexture.width = width;
  wantedTexture.height = height;
  wantedTexture.layer_count_or_depth = 1;

  uint32_t levels = 1;
  for (uint32_t extent = width > height ? width : height; extent > 1u; extent /= 2u) { ++levels; }

  if (texture.Mip == SubjectMip::None || !kChainIsReadable) { levels = 1; }
  wantedTexture.num_levels = levels;
  wantedTexture.sample_count = SDL_GPU_SAMPLECOUNT_1;
  bound.Image = OwnedTexture(Device, SDL_CreateGPUTexture(Device, &wantedTexture));

  const uint32_t indexChannels = kind == TexelKind::Direction ? 0u : IndexChannelsOf(linear);
  std::vector<float> level = linear;
  uint32_t levelWidth = width, levelHeight = height;
  for (uint32_t which = 0; which < levels; ++which) {
    if (which > 0) {
      std::vector<float> smaller;
      uint32_t smallerWidth = 0, smallerHeight = 0;
      HalveInPlace(level, levelWidth, levelHeight, smaller, smallerWidth, smallerHeight, kind,
                   indexChannels);
      level.swap(smaller);
      levelWidth = smallerWidth;
      levelHeight = smallerHeight;
    }
    const uint32_t bytes = levelWidth * levelHeight * 4u * (uint32_t)sizeof(float);
    SDL_GPUTransferBufferCreateInfo wantedTransfer{};
    wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    wantedTransfer.size = bytes;
    SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(Device, &wantedTransfer);
    std::memcpy(SDL_MapGPUTransferBuffer(Device, staging, false), level.data(), bytes);
    SDL_UnmapGPUTransferBuffer(Device, staging);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging;
    source.pixels_per_row = levelWidth;
    source.rows_per_layer = levelHeight;
    SDL_GPUTextureRegion into{};
    into.texture = bound.Image.Get();
    into.mip_level = which;
    into.w = levelWidth;
    into.h = levelHeight;
    into.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &into, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(Device, staging);
  }

  SDL_GPUSamplerCreateInfo wantedSampler{};
  wantedSampler.address_mode_u = AddressOf(texture.WrapU);
  wantedSampler.address_mode_v = AddressOf(texture.WrapV);
  wantedSampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  wantedSampler.min_filter = FilterOf(texture.Minify);
  wantedSampler.mag_filter = FilterOf(texture.Magnify);

  wantedSampler.mipmap_mode = texture.Mip == SubjectMip::Nearest
                                  ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                                  : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;

  wantedSampler.max_lod = 1000.0f;
  bound.Sample = OwnedSampler(Device, SDL_CreateGPUSampler(Device, &wantedSampler));
  return bound;
}

void SubjectDraw::BindSurface(const SubjectMaterial &material) {
  SurfaceSlot slot;
  slot.Kind = material.State().Kind();
  slot.CullsBack = CullsBackFaces(material.State(), kSubjectWinding);
  slot.ReadsSecondUv = material.ReadsSecondUv();
  slot.Colour = Upload(material.Colour, Transfer::Srgb, TexelKind::Value);
  slot.Normal = Upload(material.Normal, Transfer::Linear, TexelKind::Direction);
  slot.MetalRough = Upload(material.MetalRough, Transfer::Linear, TexelKind::Value);
  slot.Emissive = Upload(material.Emissive, Transfer::Srgb, TexelKind::Value);

  slot.SpecularStrength = Upload(material.SpecularStrength, Transfer::Linear, TexelKind::Value);
  slot.SpecularTint = Upload(material.SpecularTint, Transfer::Srgb, TexelKind::Value);

  const Material &row = material.Row;

  const float identity = (float)(Slots.size() + 1u);

  float f0[3];
  DielectricF0(row, f0);

  const float scalars[] = {
              material.Coverage(), material.State().CoverageCut(),
              row.Metalness,       row.Roughness,
              row.BaseColour[0],   row.BaseColour[1], row.BaseColour[2], row.BaseColour[3],
              row.Emission[0],     row.Emission[1],   row.Emission[2],   material.NormalScale,
              identity,            f0[0],             f0[1],             f0[2],
              DielectricF90(row),
              row.Transmission,    row.Thickness,     row.AttenuationDistance,
              row.AttenuationColour[0], row.AttenuationColour[1], row.AttenuationColour[2],
              row.SheenColour[0], row.SheenColour[1], row.SheenColour[2], row.SheenRoughness,
              row.Clearcoat,      row.ClearcoatRoughness,
              row.Anisotropy,     row.AnisotropyRotationRad,
              row.Iridescence,    row.IridescenceIor,
              row.IridescenceThicknessMinNm, row.IridescenceThicknessMaxNm};
  static_assert(sizeof scalars / sizeof scalars[0] == (size_t)kSurfaceScalars,
                "the surface row and its declared length are one statement");
  std::copy(std::begin(scalars), std::end(scalars), slot.Row.begin());

  const SubjectTexture *const images[kSubjectMaterialImages] = {&material.Colour, &material.Normal,
                                                        &material.MetalRough, &material.Emissive,
                                                        &material.SpecularStrength,
                                                        &material.SpecularTint};
  size_t at = (size_t)kSurfaceScalars;
  for (const SubjectTexture *image : images) {
    for (const double element : image->Uv.M) { slot.Row[at++] = (float)element; }
  }

  for (const SubjectTexture *image : images) {
    slot.Row[at++] = image->Set == UvSet::Second ? 1.0f : 0.0f;
  }
  Slots.push_back(std::move(slot));
}

bool SubjectDraw::SetMaterials(const std::vector<SubjectMaterial> &materials, std::string &error) {

  Slots.clear();
  Batches.clear();
  BatchLayout.clear();
  NIdx = 0;
  if (!Device) {
    error = "the subject unit has no device, so no surface can be bound";
    return false;
  }
  if (!FiltersFloat32) {
    error = "the device did not grant float32-filterable, and this unit's colour image is linear "
            "f32 so that the filter runs on exact linear values";
    return false;
  }
  for (size_t slot = 0; slot < materials.size(); ++slot) {
    const SurfaceKind kind = materials[slot].State().Kind();

    if ((kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive) &&
        Behind == nullptr && !GlassDrawnElsewhere_) {
      error = "surface slot " + std::to_string(slot) + " is " + KindName(kind) +
              ", and no pass of this plan draws it -- what is transmitted through a sheet or "
              "refracted by a volume is the scene behind it, so a subject carrying one needs the "
              "transmissive pass declared, and drawing it opaque instead would be a picture nobody "
              "asked for";
      Slots.clear();
      return false;
    }
    BindSurface(materials[slot]);
  }
  return true;
}

bool SubjectDraw::SetMesh(const SubjectMesh &mesh, std::string &error) {

  StagedCount_ = 0;
  StagingUsed_ = 0;
  NVerts = mesh.VertexCount;
  NIdx = mesh.IndexCount;
  HasUv = mesh.Uv != nullptr;
  HasUv1 = mesh.Uv1 != nullptr;
  HasNormal = mesh.Normals != nullptr;
  HasTangent = mesh.Tangents != nullptr;
  HasColour = mesh.Colours != nullptr;
  Batches.clear();
  BatchLayout.clear();
  for (int axis = 0; axis < 3; ++axis) {
    Anchor[axis] = mesh.Anchor[axis];
    PrevAnchor[axis] = mesh.PrevAnchor[axis];
  }
  for (int part = 0; part < 16; part++) { Model[part] = mesh.Model[part]; }
  if (NVerts == 0 || NIdx == 0 || !Device || !mesh.Emitted || !mesh.Verts || !mesh.Indices ||
      !mesh.Draws) {
    NIdx = 0;
    return true;
  }

  if (mesh.PrevVerts != nullptr && !WritesVelocity) {
    NIdx = 0;
    error = "the mesh carries a previous pose and the pass attaches no velocity target, so the run "
            "would reach no shader";
    return false;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {

    if (batch.MaterialSlot < Slots.size() && Slots[batch.MaterialSlot].ReadsSecondUv &&
        !(CarriesUv1(batch.Layout) && HasUv1)) {
      NIdx = 0;
      error = "surface slot " + std::to_string(batch.MaterialSlot) +
              " reads an image from the second uv set and the draw wearing it " +
              (HasUv1 ? "takes a vertex layout that binds no second run"
                      : "has no second uv run at all") +
              ", and the first set is not a substitute for it";
      return false;
    }
    if (batch.MaterialSlot >= Slots.size()) {
      NIdx = 0;
      error = "a draw names surface slot " + std::to_string(batch.MaterialSlot) +
              " over a table of " + std::to_string(Slots.size()) + " surfaces";
      return false;
    }
    if (batch.FirstIndex + batch.IndexCount > NIdx) {
      NIdx = 0;
      error = "a draw covers indices " + std::to_string(batch.FirstIndex) + " to " +
              std::to_string(batch.FirstIndex + batch.IndexCount) + " over a run of " +
              std::to_string(mesh.IndexCount);
      return false;
    }
  }
  Batches = mesh.Draws->Batches();

  BatchLayout.reserve(Batches.size());
  for (const DrawBatch &batch : Batches) {
    VertexRunsCarried carried;
    carried.Uv = CarriesUv(batch.Layout) && HasUv;
    carried.Normal = CarriesNormal(batch.Layout) && HasNormal;
    carried.Tangent = CarriesTangent(batch.Layout) && carried.Normal && carried.Uv && HasTangent;
    carried.Uv1 = CarriesUv1(batch.Layout) && carried.Uv && HasUv1;
    carried.Colour = CarriesColour(batch.Layout) && HasColour;
    VertexLayout drawn = VertexLayout::Position;
    if (!LayoutOf(carried, drawn)) {
      NIdx = 0;
      Batches.clear();
      BatchLayout.clear();
      error = "a draw's runs name no vertex layout this engine builds, and the nearest one is not "
              "an answer -- the combination is what the enumeration exists to refuse";
      return false;
    }
    BatchLayout.push_back(drawn);
  }

  {
    const Heap::Tagged uploading("mesh-upload");
    Idx = Fill(SDL_GPU_BUFFERUSAGE_INDEX, mesh.Indices, NIdx * (uint32_t)sizeof(uint32_t));
  }
  if (!Idx) {
    NIdx = 0;
    error = std::string("the subject's index run did not reach the device: ") + SDL_GetError();
    return false;
  }
  if (!HandStreams(mesh, false, error)) { return false; }

  {
    const Heap::Tagged building("mesh-bvh");

    Visibility_ = TriangleBvh::Over(Span<const float>(mesh.Verts, (size_t)NVerts * 3u),
                                    Span<const uint32_t>(mesh.Indices, (size_t)NIdx));
  }
  if (Visibility_.Empty()) {
    NIdx = 0;
    error = "the subject's " + std::to_string(NIdx / 3u) +
            " triangles built no visibility structure, so no light could be occluded by them";
    return false;
  }
  return HandVisibility(false, error);
}

bool SubjectDraw::HandStreams(const SubjectPose &pose, bool deferred, std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const uint32_t positionBytes = NVerts * 3u * (uint32_t)sizeof(float);
  const uint32_t pairBytes = NVerts * 2u * (uint32_t)sizeof(float);
  const uint32_t quadBytes = NVerts * 4u * (uint32_t)sizeof(float);
  const float *const previousPose = pose.PrevVerts != nullptr ? pose.PrevVerts : pose.Verts;
  const auto vertex = SDL_GPU_BUFFERUSAGE_VERTEX;

  Crossing streams[] = {
      {&Vtx, &Held_[(size_t)Stream::Vertex], vertex, pose.Verts, positionBytes},
      {&Emit, &Held_[(size_t)Stream::Emitted], vertex, pose.Emitted, positionBytes},
      {&Nrm, &Held_[(size_t)Stream::Normal], vertex, HasNormal ? pose.Normals : nullptr,
       HasNormal ? positionBytes : 0u},
      {&Tan, &Held_[(size_t)Stream::Tangent], vertex, HasTangent ? pose.Tangents : nullptr,
       HasTangent ? quadBytes : 0u},
      {&Uv, &Held_[(size_t)Stream::Uv], vertex, HasUv ? pose.Uv : nullptr, HasUv ? pairBytes : 0u},
      {&Uv1, &Held_[(size_t)Stream::Uv1], vertex, HasUv1 ? pose.Uv1 : nullptr,
       HasUv1 ? pairBytes : 0u},
      {&Col, &Held_[(size_t)Stream::Colour], vertex, HasColour ? pose.Colours : nullptr,
       HasColour ? quadBytes : 0u},
      {&Prev, &Held_[(size_t)Stream::Previous], vertex, WritesVelocity ? previousPose : nullptr,
       WritesVelocity ? positionBytes : 0u},
  };
  if (!Cross(streams, sizeof streams / sizeof streams[0], deferred, error)) {
    NIdx = 0;
    return false;
  }
  if (!Vtx || !Emit || (WritesVelocity && !Prev) || (HasColour && !Col)) {
    NIdx = 0;
    error = std::string("the subject's vertex streams did not reach the device: ") + SDL_GetError();
    return false;
  }
  return true;
}

bool SubjectDraw::HandVisibility(bool deferred, std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const auto storage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;

  Crossing structure[] = {
      {&BvhNodes, &Held_[(size_t)Stream::BvhNodes], storage, Visibility_.Nodes().Data(),
       (uint32_t)Visibility_.Nodes().Bytes()},
      {&BvhTris, &Held_[(size_t)Stream::BvhTriangles], storage, Visibility_.Triangles().Data(),
       (uint32_t)Visibility_.Triangles().Bytes()},
  };
  if (!Cross(structure, sizeof structure / sizeof structure[0], deferred, error)) {
    NIdx = 0;
    return false;
  }
  if (!BvhNodes || !BvhTris) {
    NIdx = 0;
    error = std::string("the subject's visibility structure did not reach the device: ") +
            SDL_GetError();
    return false;
  }

  const BvhNode &root = Visibility_.Nodes()[0];
  float diagonal = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    const float span = root.MaxM[axis] - root.MinM[axis];
    diagonal += span * span;
  }
  ShadowNearM_ = std::sqrt(diagonal) * kShadowRayNearFraction;
  return true;
}

bool SubjectDraw::SetPose(const SubjectPose &pose, std::string &error) {
  if (NIdx == 0 || Visibility_.Empty()) {
    error = "a pose arrived before any mesh, and there is no subject for it to be a pose of";
    return false;
  }
  if (pose.VertexCount != NVerts) {
    error = "the pose carries " + std::to_string(pose.VertexCount) + " vertices and the subject has " +
            std::to_string(NVerts) + ", so it is a different body rather than the same one moved";
    return false;
  }
  if (!pose.Verts || !pose.Emitted) {
    error = "a pose arrived without positions or emitted radiance, which every draw binds";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) {
    Anchor[axis] = pose.Anchor[axis];
    PrevAnchor[axis] = pose.PrevAnchor[axis];
  }
  for (int part = 0; part < 16; part++) { Model[part] = pose.Model[part]; }
  if (!HandStreams(pose, true, error)) { return false; }
  {
    const Heap::Tagged refitting("mesh-bvh");
    if (!Visibility_.Refit(Span<const float>(pose.Verts, (size_t)NVerts * 3u))) {
      error = "the subject's visibility structure did not refit to this pose";
      return false;
    }
  }
  return HandVisibility(true, error);
}

bool SubjectDraw::SetLights(const std::vector<SubjectLight> &lights, std::string &error) {
  if (lights.size() > kMaxSubjectLights) {
    error = "the subject declares " + std::to_string(lights.size()) +
            " punctual lights over a list of " + std::to_string(kMaxSubjectLights) +
            ", and a light this unit cannot bind is a refusal rather than a light left out of the "
            "picture";
    return false;
  }
  Placed = lights;
  return true;
}

std::array<float, SubjectDraw::kLightFloats> SubjectDraw::PackedLights(
    const FrameContext &ctx) const {
  std::array<float, kLightFloats> packed{};
  packed[0] = (float)Placed.size();
  packed[1] = ShadowNearM_;
  for (int channel = 0; channel < 3; ++channel) {
    packed[4 + channel] = (float)Environment.RadianceLinear[channel];
  }
  for (size_t at = 0; at < Placed.size(); ++at) {
    const PunctualLight &light = Placed[at].Light;
    float *entry = packed.data() + 8 + at * 4u * (size_t)kLightVec4s;
    for (int channel = 0; channel < 3; ++channel) {
      entry[channel] = light.Colour[channel] * light.Intensity;
    }
    entry[3] = light.Kind == LightKind::Directional ? 0.0f
                                                    : (light.Kind == LightKind::Point ? 1.0f : 2.0f);
    for (int axis = 0; axis < 3; ++axis) {
      entry[4 + axis] = (float)(Placed[at].PositionEcefM[axis] - ctx.Eye[axis]);
    }

    entry[7] = light.RangeM > 0.0f ? 1.0f / light.RangeM : 0.0f;
    for (int axis = 0; axis < 3; ++axis) { entry[8 + axis] = light.Direction[axis]; }
    const float outer = std::cos(light.OuterConeRad);
    const float inner = std::cos(light.InnerConeRad);
    entry[12] = outer;

    entry[13] = inner > outer ? 1.0f / (inner - outer) : 0.0f;
  }
  return packed;
}

uint32_t SubjectDraw::DrawCount() const {
  uint32_t drawn = 0;
  for (const DrawBatch &batch : Batches) { drawn += batch.Draws; }
  return drawn;
}

void SubjectDraw::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (NIdx == 0 || Batches.empty() || !Vtx || !Idx || !Emit || !BvhNodes || !BvhTris) { return; }
  float uniform[kUniFloats] = {};
  const auto place = [this, &ctx, &uniform, &into](uint32_t slot) {
    const double *const model =
        Placed_.empty() ? Model : Placed_.data() + (size_t)slot * 16u;
    double placed[16];
    for (int row = 0; row < 4; ++row) {
      for (int column = 0; column < 4; ++column) {
        double sum = 0.0;
        for (int over = 0; over < 4; ++over) {
          sum += (double)ctx.Mvp16[over * 4 + row] * model[column * 4 + over];
        }
        placed[column * 4 + row] = sum;
      }
    }
    for (int i = 0; i < 16; i++) { uniform[i] = (float)placed[i]; }
    for (int i = 0; i < 3; i++) { uniform[16 + i] = (float)(Anchor[i] - ctx.Eye[i]); }
    for (int i = 0; i < 16; i++) { uniform[20 + i] = ctx.PrevMvp16[i]; }
    for (int i = 0; i < 3; i++) { uniform[36 + i] = (float)(PrevAnchor[i] - ctx.PrevEye[i]); }
    SDL_PushGPUVertexUniformData(into.Commands, 0, uniform, sizeof uniform);
  };
  place(Batches.empty() ? 0u : Batches.front().ModelSlot);
  uint32_t standing = Batches.empty() ? 0u : Batches.front().ModelSlot;
  const std::array<float, kLightFloats> lights = PackedLights(ctx);
  SDL_PushGPUFragmentUniformData(into.Commands, 1, lights.data(),
                                 (uint32_t)(lights.size() * sizeof(float)));

  SDL_GPUBufferBinding indices{Idx.Get(), 0};
  SDL_BindGPUIndexBuffer(into.Pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  SDL_GPUBuffer *const occluders[kSubjectStorageBuffers] = {BvhNodes.Get(), BvhTris.Get()};
  SDL_BindGPUFragmentStorageBuffers(into.Pass, 0, occluders, kSubjectStorageBuffers);

  size_t bound = kPipelines;
  size_t boundSlot = 0;
  bool slotBound = false;
  for (size_t at = 0; at < Batches.size(); ++at) {
    const DrawBatch &batch = Batches[at];
    const SurfaceSlot &surface = Slots[batch.MaterialSlot];

    const bool glassSlot = surface.Kind == SurfaceKind::ThinTransmissive ||
                           surface.Kind == SurfaceKind::Refractive;
    if (glassSlot != (Behind != nullptr)) { continue; }

    const VertexLayout wanted = BatchLayout[at];
    const bool textured = CarriesUv(wanted);
    const bool lit = CarriesNormal(wanted);
    const bool mapped = CarriesTangent(wanted);
    const bool secondUv = CarriesUv1(wanted);
    const bool tinted = CarriesColour(wanted);

    const size_t wantedPipeline = PipelineAt(wanted, surface.Kind, surface.CullsBack);
    if (wantedPipeline != bound) {
      SDL_BindGPUGraphicsPipeline(into.Pass, Pipelines[wantedPipeline].Get());

      SDL_GPUBufferBinding runs[VertexShape::kRuns] = {};
      uint32_t count = 0;
      runs[count++] = SDL_GPUBufferBinding{Vtx.Get(), 0};
      if (textured) { runs[count++] = SDL_GPUBufferBinding{Uv.Get(), 0}; }
      if (secondUv) { runs[count++] = SDL_GPUBufferBinding{Uv1.Get(), 0}; }
      runs[count++] = SDL_GPUBufferBinding{lit ? Nrm.Get() : Emit.Get(), 0};
      if (mapped) { runs[count++] = SDL_GPUBufferBinding{Tan.Get(), 0}; }
      if (tinted) { runs[count++] = SDL_GPUBufferBinding{Col.Get(), 0}; }

      if (WritesVelocity) { runs[count++] = SDL_GPUBufferBinding{Prev.Get(), 0}; }
      SDL_BindGPUVertexBuffers(into.Pass, 0, runs, count);
      bound = wantedPipeline;
    }
    if (!slotBound || boundSlot != batch.MaterialSlot) {
      const SDL_GPUTextureSamplerBinding images[kSubjectImages] = {
          {surface.Colour.Image.Get(), surface.Colour.Sample.Get()},
          {surface.Normal.Image.Get(), surface.Normal.Sample.Get()},
          {surface.MetalRough.Image.Get(), surface.MetalRough.Sample.Get()},
          {surface.Emissive.Image.Get(), surface.Emissive.Sample.Get()},
          {surface.SpecularStrength.Image.Get(), surface.SpecularStrength.Sample.Get()},
          {surface.SpecularTint.Image.Get(), surface.SpecularTint.Sample.Get()},

          {Behind != nullptr ? Behind : surface.Colour.Image.Get(),
           BehindSampler != nullptr ? BehindSampler : surface.Colour.Sample.Get()}};
      SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kSubjectImages);
      SDL_PushGPUFragmentUniformData(into.Commands, 0, surface.Row.data(),
                                     (uint32_t)(surface.Row.size() * sizeof(float)));
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    if (batch.ModelSlot != standing) {
      place(batch.ModelSlot);
      standing = batch.ModelSlot;
    }
    SDL_DrawGPUIndexedPrimitives(into.Pass, batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

}
