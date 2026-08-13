#include "SubjectDraw.h"

#include <cmath>
#include <cstring>
#include <string>

#include "MetalRoughBrdf.h"
#include "SceneTargets.h"
#include "ShaderPrelude.h"
#include "ShadowRay.h"
#include "SurfaceState.h"
#include "TriangleBvh.h"

namespace outshine::Render {

namespace {

/* WHICH SCREEN-SPACE ORIENTATION glTF's FRONT FACE ARRIVES IN. MEASURED, not derived: the chain is
 * two flips and not one -- clip-space +Y is up while the framebuffer's runs down, and the eye basis
 * puts the view along -Z so the projection's own w is -z_eye -- and they cancel, leaving glTF's
 * counter-clockwise front face counter-clockwise on the target. The derivation that counted one flip
 * was written here first and culled every pixel of `render/coverage/quad`. */
constexpr SDL_GPUFrontFace kGltfFrontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

/* A glTF subject's winding is TRUSTED because the format defines one: the front face is
 * counter-clockwise and `Gltf::Subject` has already restated a mirroring node's order, so a face
 * that turns away is a back face and not an accident of how the file was authored. This is the
 * opposite case from an OSM ring, which arrives wound either way and is `Winding::Unknown` for it. */
constexpr Winding kSubjectWinding = Winding::Trusted;

/* THE TRANSFER FUNCTION THE FORMAT DECLARES ITS BASE COLOUR IN (glTF 2.0, `baseColorTexture`:
 * "the values are sRGB encoded"), evaluated in double and stored as f32. The hardware sRGB view it
 * replaces carries about twelve bits of this curve where the formula carries twenty-four: at texel
 * code 1 the sampler returned 1/4096 = 2.4414e-4 against the exact 3.0353e-4, and that is what
 * `simple-texture` measured as 12 833 differing pixels. */
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

/* SOURCE-OVER WITH STRAIGHT ALPHA, which is the one convention both sides of the comparison are
 * stated in. The alpha channel accumulates coverage the same way -- `a + dst*(1-a)` -- so a stack of
 * blended surfaces reports how much of the pixel they cover between them rather than only the last
 * one's contribution. */
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

/* The one place a surface kind becomes a shader entry point, over the two arms a layout selects.
 * `SetMaterials` refuses the two kinds this unit builds no pipeline for, so the arms below cover
 * every kind that reaches a draw. */
const char *FragmentEntryPoint(SurfaceKind kind, VertexLayout layout) {
  const bool textured = CarriesUv(layout);
  if (CarriesTangent(layout)) {
    switch (kind) {
      case SurfaceKind::Masked: return "fsMappedMasked";
      case SurfaceKind::Blended: return "fsMappedBlended";
      case SurfaceKind::Opaque:
      case SurfaceKind::ThinTransmissive:
      case SurfaceKind::Refractive: break;
    }
    return "fsMapped";
  }
  if (CarriesNormal(layout)) {
    switch (kind) {
      case SurfaceKind::Masked: return textured ? "fsLitMaskedTextured" : "fsLitMasked";
      case SurfaceKind::Blended: return textured ? "fsLitBlendedTextured" : "fsLitBlended";
      case SurfaceKind::Opaque:
      case SurfaceKind::ThinTransmissive:
      case SurfaceKind::Refractive: break;
    }
    return textured ? "fsLitTextured" : "fsLit";
  }
  switch (kind) {
    case SurfaceKind::Masked: return textured ? "fsMaskedTextured" : "fsMasked";
    case SurfaceKind::Blended: return textured ? "fsBlendedTextured" : "fsBlended";
    case SurfaceKind::Opaque:
    case SurfaceKind::ThinTransmissive:
    case SurfaceKind::Refractive: break;
  }
  return textured ? "fsTextured" : "fs";
}

const char *VertexEntryPoint(VertexLayout layout) {
  switch (layout) {
    case VertexLayout::Position: return "vs";
    case VertexLayout::PositionUv: return "vsTextured";
    case VertexLayout::PositionNormal: return "vsLit";
    case VertexLayout::PositionNormalUv: return "vsLitTextured";
    case VertexLayout::PositionNormalUvTangent: return "vsMapped";
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

} // namespace

/* THE ONE BINDING CONTRACT OF THIS UNIT, and every fragment entry point below carries it whole. The
 * argument list is a `#define` rather than fifteen copies for one reason: the four texture indices
 * and the two uniform slots are the SAME indices the encoder binds at, and fifteen restatements of
 * them is fifteen chances for one to disagree with the encoder. It is not a shader variant switch --
 * nothing here is conditional -- so the text every entry point compiles against is identical.
 *
 * AN ARM THAT SAMPLES NOTHING STILL DECLARES THE IMAGES, which is what keeps "no texture declared"
 * and "a white texture declared" different pictures: the difference is the ENTRY POINT, not the
 * binding, and an untextured arm never reaches a sampler however many are bound behind it. */
static const char *kSubjectBindingsMsl = R"(
struct S { float4x4 mvp; float4 anc; };
/* THE SLOT'S SURFACE ROW. `factor` is glTF's `baseColorFactor.a` and `cut` its `alphaCutoff`, which
 * both arms read; the rest is the metal-rough row only the lit arm reads. `packed_float3` and not
 * `float3`, because a Metal `float3` occupies sixteen bytes and would put `normalScale` four bytes
 * past where the host writes it. */
struct M { float factor; float cut; float metalness; float roughness;
           float4 base; packed_float3 emissive; float normalScale; };
/* `tint` is colour times intensity with the kind in `w` -- 0 directional, 1 point, 2 spot -- so the
 * multiplier and the shape travel together and a light cannot be half-declared. `place` is the
 * camera-relative position with the RECIPROCAL of the declared range in `w` -- zero where the file
 * declares none, so the range window costs a multiply and "no cutoff" is the same expression rather
 * than a branch. `beam` is the unit direction the light points. `cone` carries cos(outerConeAngle)
 * and the reciprocal of `cos(inner) - cos(outer)`, precomputed because the shader would otherwise
 * divide by it per fragment per light. */
struct Light { float4 tint; float4 place; float4 beam; float4 cone; };
/* `count.x` is how many lights are declared; `count.y` is the distance a shadow ray starts at, in
 * the subject's own metres, and it travels with the light list because it is the one number the
 * lighting loop needs that is a property of the subject rather than of a light. */
struct Lights { float4 count; Light items[16]; };

#define SUBJECT_SURFACE constant M &surface [[buffer(0)]], constant Lights &lights [[buffer(1)]], \
    device const BvhNode *bvhNodes [[buffer(2)]], device const BvhTri *bvhTris [[buffer(3)]], \
    texture2d<float> colourMap [[texture(0)]], sampler colourSampler [[sampler(0)]], \
    texture2d<float> normalMap [[texture(1)]], sampler normalSampler [[sampler(1)]], \
    texture2d<float> metalRoughMap [[texture(2)]], sampler metalRoughSampler [[sampler(2)]], \
    texture2d<float> emissiveMap [[texture(3)]], sampler emissiveSampler [[sampler(3)]]

/* THE TWO STORAGE BUFFERS AS ONE ARGUMENT, so a shading function takes the subject's own geometry
 * rather than two pointers that could be handed over out of step (`I.23`). */
struct Occluders { device const BvhNode *nodes; device const BvhTri *tris; };
#define SUBJECT_OCCLUDERS Occluders{bvhNodes, bvhTris}

/* THE FRAGMENT'S OUTPUT SET IS THE PASS'S ATTACHMENT SET, and the switch is spliced by the caller
 * from the compiled plan (board:1121). A velocity output declared into a pass that attaches no
 * velocity target is undefined and renders correctly, which is how it survived 118 tests; Metal's
 * validation aborts on it. `SUBJECT_SET_VELOCITY` exists so the six entry points below state WHAT
 * they write once each and never whether the target is there. */
struct SFrag {
  float4 col [[color(0)]];
#if SUBJECT_WRITES_VELOCITY
  float2 vel [[color(1)]];
#endif
#if SUBJECT_WRITES_SHADING_NORMAL
  /* The index follows the pass's attachment ORDER, which follows `Contributes` through the prune --
   * so it is 2 with a velocity target and 1 without, and it is spliced rather than fixed because a
   * hardcoded index is exactly the pipeline/pass disagreement board:1121 closed. */
  float4 nrm [[color(SUBJECT_NORMAL_COLOUR_INDEX)]];
#endif
};

#if SUBJECT_WRITES_VELOCITY
#define SUBJECT_SET_VELOCITY(o) (o).vel = float2(kVelStatic)
#else
#define SUBJECT_SET_VELOCITY(o) (void)0
#endif

/* THE NORMAL THE BRDF RECEIVED, WRITTEN FROM THE SAME LOCAL IT WAS SHADED WITH (board:1122). The
 * value is bound once per entry point and then used twice -- passed to `shadeRow` and written here
 * -- so the attachment carries what the lobe saw BY CONSTRUCTION rather than by inspection. It was
 * not readable before because it never existed as a value: `facing(in.n, front)` at six call sites
 * and `mappedNormal(...)` at one, every one of them an inline argument, and a recomputation in the
 * fragment could have agreed with Cycles perfectly while the lobe used something else.
 *
 * AN ARM THAT COMPUTES NO NORMAL WRITES A DECLARED ZERO, and that is a decision rather than whatever
 * the driver leaves: the emissive arms shade no lobe, zero is not a unit vector so no comparison can
 * mistake it for one, and a reader can tell "no shading normal here" from "a normal pointing away".
 * An undefined value that read plausibly would be ingested silently by the three-way comparison. */
#if SUBJECT_WRITES_SHADING_NORMAL
#define SUBJECT_SET_SHADING_NORMAL(o, n) (o).nrm = float4((n), 1.0)
#define SUBJECT_NO_SHADING_NORMAL(o) (o).nrm = float4(0.0, 0.0, 0.0, 1.0)
#else
#define SUBJECT_SET_SHADING_NORMAL(o, n) (void)0
#define SUBJECT_NO_SHADING_NORMAL(o) (void)0
#endif
)";

/* THE EMITTED ARM'S ENTRY POINTS: two layouts times the three answers glTF's `alphaMode` can give.
 * The textured arms multiply the sampled colour into the same declared radiance the plain ones emit,
 * which is what the metal-rough model reduces to for a dielectric at metalness 0
 * under a uniform environment: `colour(u,v) * factor * L`, flat, no direction. THE TAP IS ALREADY
 * LINEAR -- the texture holds linear f32 decoded on the CPU, so the sampler filters exact linear
 * values and nothing here decodes anything.
 *
 * THE RADIANCE IS FLAT-INTERPOLATED. Every vertex of one part carries the same value, and barycentric
 * interpolation of three equal floats is a weighted sum that need not return them; `flat` takes the
 * provoking vertex verbatim, so a declared value arrives as itself.
 *
 * THE OPAQUE ARMS READ `.rgb` AND NEVER `.a`, which is glTF's rule stated in the only place that can
 * hold it (`Specification.adoc:2178`: "the rendered output is fully opaque and any alpha value is
 * ignored"). Multiplying the sampled alpha into the colour there is the premultiplication
 * `AlphaBlendModeTest` puts a black box beside the word "Opaque" for. */
static const char *kSubjectMsl = R"(
struct VertexPlain { float3 p [[attribute(0)]]; float3 emitted [[attribute(2)]]; };
struct VertexTextured { float3 p [[attribute(0)]]; float2 uv [[attribute(1)]];
                        float3 emitted [[attribute(2)]]; };

struct SOut { float4 pos [[position]]; float2 uv; float3 emitted [[flat]]; };

vertex SOut vs(VertexPlain v [[stage_in]], constant S &s [[buffer(0)]]) {
  SOut o;
  o.pos = s.mvp * float4(v.p + s.anc.xyz, 1.0);
  o.uv = float2(0.0);
  o.emitted = v.emitted;
  return o;
}

vertex SOut vsTextured(VertexTextured v [[stage_in]], constant S &s [[buffer(0)]]) {
  SOut o;
  o.pos = s.mvp * float4(v.p + s.anc.xyz, 1.0);
  o.uv = v.uv;
  o.emitted = v.emitted;
  return o;
}

/* The declared radiance, not shaded. On an opaque arm the fourth channel is the direct fraction a
 * display transfer weights its occlusion by, and for a surface that emits what it was declared to
 * emit all of it is direct. */
fragment SFrag fs(SOut in [[stage_in]], SUBJECT_SURFACE) {
  SFrag o;
  o.col = float4(in.emitted, 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsMasked(SOut in [[stage_in]], SUBJECT_SURFACE) {
  if (surface.factor < surface.cut) { discard_fragment(); }
  SFrag o;
  o.col = float4(in.emitted, 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsBlended(SOut in [[stage_in]], SUBJECT_SURFACE) {
  SFrag o;
  o.col = float4(in.emitted, surface.factor);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

fragment SFrag fsTextured(SOut in [[stage_in]], SUBJECT_SURFACE) {
  SFrag o;
  o.col = float4(in.emitted * colourMap.sample(colourSampler, in.uv).rgb, 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

/* Greater-or-equal is kept, less-than discards: glTF states the surviving side, not the cut side
 * ("If the alpha value is greater than or equal to the alphaCutoff value then it is rendered as
 * fully opaque"), and the two differ on exactly the texels a linear ramp puts at the cutoff. */
fragment SFrag fsMaskedTextured(SOut in [[stage_in]], SUBJECT_SURFACE) {
  float4 tap = colourMap.sample(colourSampler, in.uv);
  if (surface.factor * tap.a < surface.cut) { discard_fragment(); }
  SFrag o;
  o.col = float4(in.emitted * tap.rgb, 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}

/* STRAIGHT, NOT PREMULTIPLIED: the colour goes out unweighted and the blend state applies the
 * weight, so the same fragment function would be correct under either convention only by accident.
 * The one convention this whole comparison is stated in is straight alpha. */
fragment SFrag fsBlendedTextured(SOut in [[stage_in]], SUBJECT_SURFACE) {
  float4 tap = colourMap.sample(colourSampler, in.uv);
  SFrag o;
  o.col = float4(in.emitted * tap.rgb, surface.factor * tap.a);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_NO_SHADING_NORMAL(o);
  return o;
}
)";

/* THE LIT ARM: the light list, and glTF's metal-rough BRDF evaluated against it. The model itself is
 * in `MetalRoughBrdf.h` beside its C++ twin, spliced in ahead of this text; what is here is the loop
 * over the lights, their shapes and their falloff.
 *
 * THE EYE IS THE ORIGIN of these coordinates: the vertex arm adds `anc`, which is the anchor minus
 * the camera, so a fragment's position IS its offset from the eye and the view vector needs no
 * camera uniform of its own.
 *
 * THE VISIBILITY TERM IS AN EXACT RAY AGAINST THE SUBJECT'S OWN GEOMETRY, which is the oracle's own
 * predicate rather than an approximation of it (`ShadowRay.h`). It is traced in the SUBJECT's frame
 * and not the eye's -- `lp` is the vertex before the anchor is added -- because the acceleration
 * structure is built once over vertices that do not move while the eye does. */
static const char *kSubjectLitMsl = R"(
struct VertexLit { float3 p [[attribute(0)]]; float3 n [[attribute(3)]]; };
struct VertexLitTextured { float3 p [[attribute(0)]]; float2 uv [[attribute(1)]];
                           float3 n [[attribute(3)]]; };

struct LOut { float4 pos [[position]]; float2 uv; float3 n; float3 p; float3 lp; };

vertex LOut vsLit(VertexLit v [[stage_in]], constant S &s [[buffer(0)]]) {
  LOut o;
  float3 placed = v.p + s.anc.xyz;
  o.pos = s.mvp * float4(placed, 1.0);
  o.uv = float2(0.0);
  o.n = v.n;
  o.p = placed;
  o.lp = v.p;
  return o;
}

vertex LOut vsLitTextured(VertexLitTextured v [[stage_in]], constant S &s [[buffer(0)]]) {
  LOut o;
  float3 placed = v.p + s.anc.xyz;
  o.pos = s.mvp * float4(placed, 1.0);
  o.uv = v.uv;
  o.n = v.n;
  o.p = placed;
  o.lp = v.p;
  return o;
}

/* THE ROUGHNESS, THE METALNESS AND THE EMITTED RADIANCE ARE ARGUMENTS AND NOT READS OF THE ROW,
 * because a textured surface states all three per texel and the untextured arm states them per
 * material -- one function that read the row would force the mapped arm to write the loop over the
 * lights a second time. */
static inline float3 shadeRow(constant Lights &lights, Occluders occluders, float3 localM, float3 n,
                              float3 p, float3 albedo, float metalness, float roughness,
                              float3 emitted) {
  float3 v = normalize(-p);
  float a = roughness * roughness;
  float a2 = a * a;
  float3 diffuseColour = albedo * (1.0 - metalness);
  float3 f0 = mix(float3(kDielectricF0), albedo, metalness);
  float nv = max(dot(n, v), 1.0e-6);
  /* THE RAY LEAVES FROM THE SURFACE AND NOT FROM INSIDE IT. Offsetting along the shading normal as
   * well as starting at `count.y` is what keeps a facet whose normal map tilts it towards the light
   * from re-finding its own triangle. */
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
      /* THE INVERSE-SQUARE LAW, WINDOWED BY THE DECLARED RANGE. `intensity` is candela, so a facet
       * facing the light receives `intensity / d^2`; the window is `KHR_lights_punctual`'s own
       * recommended function, `max(min(1 - (d/range)^4, 1), 0)`, and it is applied rather than
       * dropped because the corpus asset that declares a range depends on it -- six panels 2.25 m
       * apart, eight scene-wide lights, and a range of exactly half the spacing is what makes each
       * panel see only its own. `place.w` is 1/range, zero where the file declared none, and then
       * the fourth power is zero and the window is 1. */
      float reach = square * light.place.w * light.place.w;
      attenuation = clamp(1.0 - reach * reach, 0.0, 1.0) / square;
    }
    if (light.tint.w > 1.5) {
      attenuation = attenuation *
                    clamp((dot(light.beam.xyz, -toward) - light.cone.x) * light.cone.y, 0.0, 1.0);
    }
    float nl = dot(n, toward);
    if (nl <= 0.0 || attenuation <= 0.0) { continue; }
    /* THE VISIBILITY TERM, AND IT IS EVALUATED LAST OF THE THREE REJECTIONS ON PURPOSE: a facet
     * turned away from the light and a light attenuated to nothing are one dot product each, and
     * they retire the fragment before the traversal is entered at all. */
    if (bvhOccludes(occluders.nodes, occluders.tris, originM, toward, lights.count.y, reachM)) {
      continue;
    }
    float3 h = normalize(toward + v);
    Brdf reflected = metalRoughBrdf(diffuseColour, f0, a2, nl, nv,
                                    max(dot(n, h), 0.0), max(dot(v, h), 0.0));
    sum = sum + (reflected.diffuse + reflected.specular) * nl * attenuation * light.tint.rgb;
  }
  return sum + emitted;
}

/* THE ARM WITH NO UV AT ALL, so the emitted radiance can only be the factor: there is no
 * coordinate to sample an image with, and glTF's emissive is the factor where the material
 * declares no image. */
static inline float3 shade(constant M &surface, constant Lights &lights, Occluders occluders,
                           float3 localM, float3 n, float3 p, float3 albedo) {
  return shadeRow(lights, occluders, localM, n, p, albedo, surface.metalness, surface.roughness,
                  surface.emissive);
}

/* A DOUBLE-SIDED FACET HIT FROM BEHIND IS LIT BY ITS OTHER FACE, which is what the flip is: the
 * file's normal describes the front, and a back-facing fragment only exists at all because the
 * material said the surface has two sides. Without it such a facet faces away from every light and
 * comes back black -- and glTF's own `doubleSided` assets are exactly the ones that show it. */
static inline float3 facing(float3 n, bool front) {
  return select(-normalize(n), normalize(n), front);
}

fragment SFrag fsLit(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shade(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p, surface.base.rgb), 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal);
  return o;
}

fragment SFrag fsLitMasked(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  if (surface.factor < surface.cut) { discard_fragment(); }
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shade(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p, surface.base.rgb), 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal);
  return o;
}

fragment SFrag fsLitBlended(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shade(surface, lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p, surface.base.rgb),
                 surface.factor);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal);
  return o;
}
)";

/* THE LIT TEXTURED ARMS, split out for the same reason the emitted ones are: the sampled colour is
 * the BASE COLOUR the BRDF is evaluated with -- not a factor applied afterwards -- because glTF's
 * base colour feeds both halves of the model and multiplying a shaded result by it would tint the
 * specular lobe of a dielectric, which is a metal's behaviour. */
static const char *kSubjectLitTexturedMsl = R"(
/* glTF's EMISSION: `emissiveFactor` TIMES `emissiveTexture` (Specification.adoc:1436), and a slot
 * that declares no image binds one white texel so that the product is the factor alone. */
static inline float3 emittedAt(constant M &surface, texture2d<float> emissiveMap,
                               sampler emissiveSampler, float2 uv) {
  return surface.emissive * emissiveMap.sample(emissiveSampler, uv).rgb;
}

fragment SFrag fsLitTextured(LOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  float4 tap = colourMap.sample(colourSampler, in.uv);
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shadeRow(lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                          surface.base.rgb * tap.rgb,
                          surface.metalness, surface.roughness,
                          emittedAt(surface, emissiveMap, emissiveSampler, in.uv)), 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal);
  return o;
}

fragment SFrag fsLitMaskedTextured(LOut in [[stage_in]], bool front [[front_facing]],
                                   SUBJECT_SURFACE) {
  float4 tap = colourMap.sample(colourSampler, in.uv);
  if (surface.factor * tap.a < surface.cut) { discard_fragment(); }
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shadeRow(lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                          surface.base.rgb * tap.rgb,
                          surface.metalness, surface.roughness,
                          emittedAt(surface, emissiveMap, emissiveSampler, in.uv)), 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal);
  return o;
}

fragment SFrag fsLitBlendedTextured(LOut in [[stage_in]], bool front [[front_facing]],
                                    SUBJECT_SURFACE) {
  float4 tap = colourMap.sample(colourSampler, in.uv);
  const float3 shadingNormal = facing(in.n, front);
  SFrag o;
  o.col = float4(shadeRow(lights, SUBJECT_OCCLUDERS, in.lp, shadingNormal, in.p,
                          surface.base.rgb * tap.rgb,
                          surface.metalness, surface.roughness,
                          emittedAt(surface, emissiveMap, emissiveSampler, in.uv)),
                 surface.factor * tap.a);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shadingNormal);
  return o;
}
)";

/* THE NORMAL-MAPPED ARM: the same BRDF over a normal the file states per texel instead of per
 * vertex, and the tangent frame is what makes the two the same quantity.
 *
 * THE FRAME IS RE-ORTHOGONALISED HERE AND NOT TRUSTED AS IT ARRIVES. A supplied `TANGENT` is
 * interpolated across a triangle and quantised to f32 by whoever wrote the file, so it is neither
 * unit nor perpendicular at a fragment -- MEASURED on `NormalTangentMirrorTest`, |T.N| reaches
 * 1.13e-5 at its own vertices before any interpolation. Gram-Schmidt against the shading normal is
 * the correction the format's own sample implementation applies, and it costs one dot product.
 *
 * THE BITANGENT IS DERIVED FROM `w` AND NEVER CARRIED. glTF states the handedness as one number
 * exactly so that a mirrored body cannot end up with a bitangent that disagrees with its tangent;
 * a run that carried both would be two answers to one question.
 *
 * ONLY x AND y ARE SCALED. `normalTexture.scale` is defined as multiplying the sampled x and y
 * (Specification.adoc:1416), so the surface flattens towards its geometric normal as the scale goes
 * to zero -- scaling z as well would leave the normal unchanged and the parameter would do nothing.
 *
 * A BACK FACE TURNS THE WHOLE FRAME AROUND AND NOT ONLY THE NORMAL. Negating the normal alone would
 * leave a left-handed basis behind and mirror the map's x axis; the format's own note is to negate
 * the frame, which is what the sign below does to all three.
 *
 * ROUGHNESS AND METALNESS COME FROM THE FILE'S OWN IMAGE where it declares one, green and blue,
 * multiplied by the factors -- which is glTF's rule (Specification.adoc:1394) and is not an extra:
 * the alternative is the default factor of 1, and at roughness 1 the GGX distribution is the
 * constant 1/pi, so the lobe stops depending on the normal and a normal-mapped picture comes back
 * flat. */
static const char *kSubjectMappedMsl = R"(
struct VertexMapped { float3 p [[attribute(0)]]; float2 uv [[attribute(1)]];
                      float3 n [[attribute(3)]]; float4 t [[attribute(4)]]; };

struct MOut { float4 pos [[position]]; float2 uv; float3 n; float3 p; float3 lp; float4 t; };

vertex MOut vsMapped(VertexMapped v [[stage_in]], constant S &s [[buffer(0)]]) {
  MOut o;
  float3 placed = v.p + s.anc.xyz;
  o.pos = s.mvp * float4(placed, 1.0);
  o.uv = v.uv;
  o.n = v.n;
  o.p = placed;
  o.lp = v.p;
  o.t = v.t;
  return o;
}

static inline float3 mappedNormal(constant M &surface, texture2d<float> normalMap,
                                  sampler normalSampler, MOut in, bool front) {
  float side = select(-1.0, 1.0, front);
  float3 n = normalize(in.n) * side;
  float3 raw = in.t.xyz * side;
  float3 t = normalize(raw - n * dot(n, raw));
  float3 b = cross(n, t) * in.t.w;
  float3 tap = normalMap.sample(normalSampler, in.uv).xyz * 2.0 - 1.0;
  float3 scaled = float3(tap.xy * surface.normalScale, tap.z);
  return normalize(t * scaled.x + b * scaled.y + n * scaled.z);
}

/* The metal-rough row the mapped arm shades with: the surface's own factors times the file's image.
 * A slot that declares no image binds one white texel, and white is the multiplicative identity
 * here, so "no image" and "the factors alone" are the same statement rather than two arms. */
/* THE COLOUR AND THE NORMAL IT WAS SHADED WITH, TOGETHER (board:1122, `F.21`). Returning a struct
 * rather than taking an out-parameter is what makes the pair inseparable: a caller cannot take the
 * colour and recompute the normal, which is the reconstruction the third leg exists to rule out. */
struct Shaded { float3 col; float3 nrm; };

static inline Shaded mappedShade(constant M &surface, constant Lights &lights, Occluders occluders,
                                 texture2d<float> colourMap, sampler colourSampler,
                                 texture2d<float> normalMap, sampler normalSampler,
                                 texture2d<float> metalRoughMap, sampler metalRoughSampler,
                                 texture2d<float> emissiveMap, sampler emissiveSampler,
                                 MOut in, bool front) {
  float4 orm = metalRoughMap.sample(metalRoughSampler, in.uv);
  float3 albedo = surface.base.rgb * colourMap.sample(colourSampler, in.uv).rgb;
  const float3 shadingNormal = mappedNormal(surface, normalMap, normalSampler, in, front);
  return Shaded{shadeRow(lights, occluders, in.lp, shadingNormal, in.p, albedo,
                  surface.metalness * orm.b, surface.roughness * orm.g,
                  emittedAt(surface, emissiveMap, emissiveSampler, in.uv)),
                shadingNormal};
}

#define SUBJECT_MAPPED_SHADE mappedShade(surface, lights, SUBJECT_OCCLUDERS, colourMap, \
    colourSampler, normalMap, normalSampler, metalRoughMap, metalRoughSampler, emissiveMap, \
    emissiveSampler, in, front)

fragment SFrag fsMapped(MOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  const Shaded shaded = SUBJECT_MAPPED_SHADE;
  SFrag o;
  o.col = float4(shaded.col, 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shaded.nrm);
  return o;
}

fragment SFrag fsMappedMasked(MOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  float4 tap = colourMap.sample(colourSampler, in.uv);
  if (surface.factor * tap.a < surface.cut) { discard_fragment(); }
  const Shaded shaded = SUBJECT_MAPPED_SHADE;
  SFrag o;
  o.col = float4(shaded.col, 1.0);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shaded.nrm);
  return o;
}

fragment SFrag fsMappedBlended(MOut in [[stage_in]], bool front [[front_facing]], SUBJECT_SURFACE) {
  float4 tap = colourMap.sample(colourSampler, in.uv);
  const Shaded shaded = SUBJECT_MAPPED_SHADE;
  SFrag o;
  o.col = float4(shaded.col, surface.factor * tap.a);
  SUBJECT_SET_VELOCITY(o);
  SUBJECT_SET_SHADING_NORMAL(o, shaded.nrm);
  return o;
}
)";

namespace {

/* THE FIVE VERTEX LAYOUTS, as the pipeline states them. Every attribute sits in a buffer of its own
 * and at offset 0: the subject's positions, its uvs, its normals, its tangents and its declared
 * radiance come out of the consumer as separate runs, and interleaving them here would be a copy
 * nobody asked for. The untextured pipeline has no uv slot at all rather than an empty one. */
struct VertexShape {
  SDL_GPUVertexBufferDescription Buffers[4];
  SDL_GPUVertexAttribute Attributes[4];
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

VertexShape ShapeOf(VertexLayout layout) {
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
  /* THE LIT ARMS BIND NO RADIANCE RUN AT ALL, which is the vertex-buffer half of what the two arms
   * are: a lit surface's colour comes from its own row and the light list, and a per-vertex radiance
   * beside it would be a second answer to the same question. */
  shape.Buffers[shape.Count] = Run(shape.Count, 3);
  shape.Attributes[shape.Count] = At(lit ? 3 : 2, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
  ++shape.Count;
  if (mapped) {
    shape.Buffers[shape.Count] = Run(shape.Count, 4);
    shape.Attributes[shape.Count] = At(4, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
    ++shape.Count;
  }
  return shape;
}

/* HOW MANY IMAGES AND UNIFORM SLOTS EVERY FRAGMENT OF THIS UNIT DECLARES, which is the whole of the
 * binding contract `SUBJECT_SURFACE` spells: one number here and one macro there, and the encoder
 * binds exactly this many. */
constexpr uint32_t kSubjectImages = 4;
constexpr uint32_t kSubjectFragmentUniforms = 2;
/* THE SUBJECT'S OWN GEOMETRY, WHICH EVERY FRAGMENT CAN SEE: the acceleration structure's nodes and
 * its triangles. Both are declared by every fragment entry point for the same reason the four
 * images are -- the binding contract is one text, and an arm that traces nothing still declares
 * what an arm that traces reads. */
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

} // namespace

bool SubjectDraw::Configure(const Gpu &gpu, std::string &error) {
  Device = gpu.Device;
  FiltersFloat32 = gpu.FiltersFloat32;

  /* WHAT THIS PASS ATTACHES, DECIDED ONCE AND SPLICED INTO BOTH HALVES (board:1121): the shader's
   * output set and the pipeline's target list come from the same answer, so they cannot disagree.
   * Reading the plan's set rather than counting a constant is the whole repair -- a pipeline that
   * declared a target the pass does not attach renders correctly and is undefined. */
  Colours.clear();
  for (const Resource colour : gpu.SceneColours) { Colours.push_back(colour); }
  const auto attachmentIndex = [this](Resource which) -> long {
    const auto at = std::find(Colours.begin(), Colours.end(), which);
    return at == Colours.end() ? -1 : (long)(at - Colours.begin());
  };
  const bool writesVelocity = attachmentIndex(Resource::SceneVelocity) >= 0;
  const long normalIndex = attachmentIndex(Resource::SceneShadingNormal);

  const std::string source = std::string(kMslPrelude) + kVelocityMsl + ShadowRayMsl() +
                             "\n#define SUBJECT_WRITES_VELOCITY " + (writesVelocity ? "1" : "0") +
                             "\n#define SUBJECT_WRITES_SHADING_NORMAL " +
                             (normalIndex >= 0 ? "1" : "0") +
                             "\n#define SUBJECT_NORMAL_COLOUR_INDEX " +
                             std::to_string(normalIndex < 0 ? 0 : normalIndex) +
                             "\n" + kSubjectBindingsMsl + kSubjectMsl + MetalRoughBrdfMsl() +
                             kSubjectLitMsl + kSubjectLitTexturedMsl + kSubjectMappedMsl;

  /* THIRTY PIPELINES: five vertex layouts, two facings, three alpha modes. The facing is the SLOT's
   * because glTF states it per material -- `TextureSettingsTest` hides a green checkmark behind a
   * polygon facing the wrong way and lets the flag decide which of the two is seen, and one cull
   * mode for the whole subject draws the wrong cell whichever way it is set. */
  for (const SurfaceKind kind : {SurfaceKind::Opaque, SurfaceKind::Masked, SurfaceKind::Blended}) {
    const bool blends = kind == SurfaceKind::Blended;
    SDL_GPUColorTargetDescription targets[kMaxColourAttachments] = {};
    targets[0].format = gpu.HdrFormat;
    if (blends) { targets[0].blend_state = OverBlend(); }
    /* A BLENDED SURFACE WRITES NO VELOCITY, and that follows from its writing no depth rather than
     * being a second decision: a temporal resolve reprojects a pixel through the depth that was
     * written there, so a surface that left none has no motion to claim and would overwrite the
     * motion of whatever did. */
    if (writesVelocity) { targets[attachmentIndex(Resource::SceneVelocity)] = VelocityTarget(!blends); }
    /* THE NORMAL TARGET IS NOT BLENDED WHATEVER THE SURFACE IS. A blended surface's colour is an
     * `over` composite, and compositing two normals would produce a direction neither fragment
     * shaded with -- the attachment carries what the LAST writer received, which is the only reading
     * of it that is a normal at all. */
    if (normalIndex >= 0) { targets[normalIndex].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; }

    for (const VertexLayout layout :
         {VertexLayout::Position, VertexLayout::PositionUv, VertexLayout::PositionNormal,
          VertexLayout::PositionNormalUv, VertexLayout::PositionNormalUvTangent}) {
      const VertexShape shape = ShapeOf(layout);
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
      /* `MASK` writes depth like any solid surface -- Khronos says so in the asset's own words,
       * "Depth buffering with writes enabled may be used in MASK mode" -- because a kept fragment is
       * fully opaque. `BLEND` cannot: it composites with what is behind it, so writing the depth
       * that would hide that is the one thing it must not do (`core/SurfaceState.h` agrees). */
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

/* ONE IMAGE ON THE DEVICE, IN LINEAR f32. One texel of white where the surface declares none, so
 * every slot's binding is complete -- it is never a stand-in for a missing texture, because the
 * pipeline that draws a surface declaring none does not sample it, and white is the multiplicative
 * identity of the two arms that do.
 *
 * WHETHER THE THREE COLOUR CHANNELS CARRY THE sRGB TRANSFER IS THE SOCKET'S QUESTION and is answered
 * by the caller: glTF puts base colour and emissive in sRGB and the normal, occlusion and
 * metallic-roughness maps in linear. Alpha never carries it on either arm. */
SubjectDraw::BoundImage SubjectDraw::Upload(const SubjectTexture &texture, Transfer decode) {
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

  BoundImage bound;
  SDL_GPUTextureCreateInfo wantedTexture{};
  wantedTexture.type = SDL_GPU_TEXTURETYPE_2D;
  wantedTexture.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  wantedTexture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedTexture.width = width;
  wantedTexture.height = height;
  wantedTexture.layer_count_or_depth = 1;
  wantedTexture.num_levels = 1;
  wantedTexture.sample_count = SDL_GPU_SAMPLECOUNT_1;
  bound.Image = OwnedTexture(Device, SDL_CreateGPUTexture(Device, &wantedTexture));

  const uint32_t bytes = width * height * 4u * (uint32_t)sizeof(float);
  SDL_GPUTransferBufferCreateInfo wantedTransfer{};
  wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  wantedTransfer.size = bytes;
  SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(Device, &wantedTransfer);
  std::memcpy(SDL_MapGPUTransferBuffer(Device, staging, false), linear.data(), bytes);
  SDL_UnmapGPUTransferBuffer(Device, staging);
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = staging;
  source.pixels_per_row = width;
  source.rows_per_layer = height;
  SDL_GPUTextureRegion into{};
  into.texture = bound.Image.Get();
  into.w = width;
  into.h = height;
  into.d = 1;
  SDL_UploadToGPUTexture(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(Device, staging);

  SDL_GPUSamplerCreateInfo wantedSampler{};
  wantedSampler.address_mode_u = AddressOf(texture.WrapU);
  wantedSampler.address_mode_v = AddressOf(texture.WrapV);
  wantedSampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.min_filter = FilterOf(texture.Magnify);
  wantedSampler.mag_filter = FilterOf(texture.Magnify);
  wantedSampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  bound.Sample = OwnedSampler(Device, SDL_CreateGPUSampler(Device, &wantedSampler));
  return bound;
}

void SubjectDraw::BindSurface(const SubjectMaterial &material) {
  SurfaceSlot slot;
  slot.Kind = material.State().Kind();
  slot.CullsBack = CullsBackFaces(material.State(), kSubjectWinding);
  slot.Colour = Upload(material.Colour, Transfer::Srgb);
  slot.Normal = Upload(material.Normal, Transfer::Linear);
  slot.MetalRough = Upload(material.MetalRough, Transfer::Linear);
  slot.Emissive = Upload(material.Emissive, Transfer::Srgb);

  const Material &row = material.Row;
  slot.Row = {material.Coverage(), material.State().CoverageCut(),
              row.Metalness,       row.Roughness,
              row.BaseColour[0],   row.BaseColour[1], row.BaseColour[2], row.BaseColour[3],
              row.Emission[0],     row.Emission[1],   row.Emission[2],   material.NormalScale};
  Slots.push_back(std::move(slot));
}

bool SubjectDraw::SetMaterials(const std::vector<SubjectMaterial> &materials, std::string &error) {
  /* A NEW TABLE RETIRES THE OLD LIST, because a batch names a slot INDEX and the indices it names
   * belonged to the table being replaced. `SetMesh` is what re-validates them, so until it runs
   * there is no list -- which is what makes "encode a batch against a table that no longer holds its
   * slot" unspellable rather than a bounds check nobody reaches. */
  Slots.clear();
  Batches.clear();
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
    if (kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive) {
      error = "surface slot " + std::to_string(slot) + " is " + KindName(kind) +
              ", and this unit draws OPAQUE, MASK and BLEND -- what is transmitted through a sheet "
              "or refracted by a volume is the scene behind it, which is a read of the frame this "
              "unit does not have, not a blend factor it could substitute";
      Slots.clear();
      return false;
    }
    BindSurface(materials[slot]);
  }
  return true;
}

bool SubjectDraw::SetMesh(const SubjectMesh &mesh, std::string &error) {
  NVerts = mesh.VertexCount;
  NIdx = mesh.IndexCount;
  HasUv = mesh.Uv != nullptr;
  HasNormal = mesh.Normals != nullptr;
  HasTangent = mesh.Tangents != nullptr;
  Batches.clear();
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = mesh.Anchor[axis]; }
  if (NVerts == 0 || NIdx == 0 || !Device || !mesh.Emitted || !mesh.Verts || !mesh.Indices ||
      !mesh.Draws) {
    NIdx = 0;
    return true;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {
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

  const uint32_t positionBytes = NVerts * 3u * (uint32_t)sizeof(float);
  Vtx = Fill(SDL_GPU_BUFFERUSAGE_VERTEX, mesh.Verts, positionBytes);
  Emit = Fill(SDL_GPU_BUFFERUSAGE_VERTEX, mesh.Emitted, positionBytes);
  Nrm = HasNormal ? Fill(SDL_GPU_BUFFERUSAGE_VERTEX, mesh.Normals, positionBytes) : OwnedBuffer();
  Tan = HasTangent ? Fill(SDL_GPU_BUFFERUSAGE_VERTEX, mesh.Tangents,
                          NVerts * 4u * (uint32_t)sizeof(float))
                   : OwnedBuffer();
  Uv = HasUv ? Fill(SDL_GPU_BUFFERUSAGE_VERTEX, mesh.Uv, NVerts * 2u * (uint32_t)sizeof(float))
             : OwnedBuffer();
  Idx = Fill(SDL_GPU_BUFFERUSAGE_INDEX, mesh.Indices, NIdx * (uint32_t)sizeof(uint32_t));
  if (!Vtx || !Emit || !Idx) {
    NIdx = 0;
    error = std::string("the subject's mesh did not reach the device: ") + SDL_GetError();
    return false;
  }

  /* THE VISIBILITY STRUCTURE IS BUILT OVER THE WHOLE INDEX RUN and not per batch, because a shadow
   * is cast by the subject and not by a draw: a body split into thirty primitives shadows itself
   * across every one of those seams, and thirty structures would each be blind to the other
   * twenty-nine. */
  const TriangleBvh visibility = TriangleBvh::Over(
      Span<const float>(mesh.Verts, (size_t)NVerts * 3u),
      Span<const uint32_t>(mesh.Indices, (size_t)NIdx));
  if (visibility.Empty()) {
    NIdx = 0;
    error = "the subject's " + std::to_string(NIdx / 3u) +
            " triangles built no visibility structure, so no light could be occluded by them";
    return false;
  }
  BvhNodes = Fill(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, visibility.Nodes().Data(),
                  (uint32_t)visibility.Nodes().Bytes());
  BvhTris = Fill(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, visibility.Triangles().Data(),
                 (uint32_t)visibility.Triangles().Bytes());
  if (!BvhNodes || !BvhTris) {
    NIdx = 0;
    error = std::string("the subject's visibility structure did not reach the device: ") +
            SDL_GetError();
    return false;
  }
  /* THE RAY'S START, IN THE SUBJECT'S OWN METRES: a fixed fraction of the structure's root box,
   * which is the only length scale the subject has. A constant in metres would be four orders too
   * large on a 3 cm part and four too small on a city. */
  const BvhNode &root = visibility.Nodes()[0];
  float diagonal = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    const float span = root.MaxM[axis] - root.MinM[axis];
    diagonal += span * span;
  }
  ShadowNearM_ = std::sqrt(diagonal) * kShadowRayNearFraction;
  return true;
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

/* THE LIGHT LIST IN THE SHADER'S OWN ALPHABET, rebuilt per frame because one of its numbers is
 * camera-relative: a light's position is an ECEF double and the shader works in metres from the eye,
 * so the subtraction has to happen where the eye is known and in the width the eye is known in. */
std::array<float, SubjectDraw::kLightFloats> SubjectDraw::PackedLights(
    const FrameContext &ctx) const {
  std::array<float, kLightFloats> packed{};
  packed[0] = (float)Placed.size();
  packed[1] = ShadowNearM_;
  for (size_t at = 0; at < Placed.size(); ++at) {
    const PunctualLight &light = Placed[at].Light;
    float *entry = packed.data() + 4 + at * 4u * (size_t)kLightVec4s;
    for (int channel = 0; channel < 3; ++channel) {
      entry[channel] = light.Colour[channel] * light.Intensity;
    }
    entry[3] = light.Kind == LightKind::Directional ? 0.0f
                                                    : (light.Kind == LightKind::Point ? 1.0f : 2.0f);
    for (int axis = 0; axis < 3; ++axis) {
      entry[4 + axis] = (float)(Placed[at].PositionEcefM[axis] - ctx.Eye[axis]);
    }
    /* The reciprocal of the declared range, and 0 where the file declared none -- which makes the
     * window's fourth power zero and the window itself 1, so "no cutoff" needs no branch. */
    entry[7] = light.RangeM > 0.0f ? 1.0f / light.RangeM : 0.0f;
    for (int axis = 0; axis < 3; ++axis) { entry[8 + axis] = light.Direction[axis]; }
    const float outer = std::cos(light.OuterConeRad);
    const float inner = std::cos(light.InnerConeRad);
    entry[12] = outer;
    /* The reciprocal of the cone's own span, so the fragment multiplies where it would divide. The
     * reader refuses `inner >= outer`, so the difference is strictly positive here. */
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
  for (int i = 0; i < 16; i++) { uniform[i] = ctx.Mvp16[i]; }
  for (int i = 0; i < 3; i++) { uniform[16 + i] = (float)(Anchor[i] - ctx.Eye[i]); }
  SDL_PushGPUVertexUniformData(into.Commands, 0, uniform, sizeof uniform);
  const std::array<float, kLightFloats> lights = PackedLights(ctx);
  SDL_PushGPUFragmentUniformData(into.Commands, 1, lights.data(),
                                 (uint32_t)(lights.size() * sizeof(float)));

  SDL_GPUBufferBinding indices{Idx.Get(), 0};
  SDL_BindGPUIndexBuffer(into.Pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  /* THE SUBJECT'S GEOMETRY IS BOUND ONCE FOR THE WHOLE LIST, because there is one of it: every
   * batch shades against the same occluders and rebinding per batch would be the same two handles
   * written again. */
  SDL_GPUBuffer *const occluders[kSubjectStorageBuffers] = {BvhNodes.Get(), BvhTris.Get()};
  SDL_BindGPUFragmentStorageBuffers(into.Pass, 0, occluders, kSubjectStorageBuffers);

  /* THE LIST IS ALREADY IN ORDER -- opaque, then masked, then blended back to front, which is
   * `DrawKey`'s own ordering -- so the encoder only notices where the state changes: the batcher
   * merged what shared a pipeline and a slot, and what is left is exactly the changes that had to
   * happen. */
  size_t bound = kPipelines;
  size_t boundSlot = 0;
  bool slotBound = false;
  for (const DrawBatch &batch : Batches) {
    const SurfaceSlot &surface = Slots[batch.MaterialSlot];
    /* WHAT THE MESH ACTUALLY CARRIES DECIDES, not what the draw asked for: a list built against a
     * subject and a mesh set from another would otherwise bind a pipeline whose slot has no buffer.
     * The draw's own layout is the ceiling and the mesh is the floor. */
    const bool textured = CarriesUv(batch.Layout) && HasUv && Uv;
    const bool lit = CarriesNormal(batch.Layout) && HasNormal && Nrm;
    const bool mapped = CarriesTangent(batch.Layout) && lit && textured && HasTangent && Tan;
    const VertexLayout wanted =
        mapped ? VertexLayout::PositionNormalUvTangent
               : (lit ? (textured ? VertexLayout::PositionNormalUv : VertexLayout::PositionNormal)
                      : (textured ? VertexLayout::PositionUv : VertexLayout::Position));
    /* THE FACING FOLLOWS THE SLOT, AND THE SORT KEY DOES NOT CARRY IT: two batches of one layout
     * that disagree about `doubleSided` cost a pipeline change here rather than being merged. That
     * is a batching cost and not a correctness one, and it is the honest place to pay it -- putting
     * the flag in the key would sort a subject's parts by a device state. */
    const size_t wantedPipeline = PipelineAt(wanted, surface.Kind, surface.CullsBack);
    if (wantedPipeline != bound) {
      SDL_BindGPUGraphicsPipeline(into.Pass, Pipelines[wantedPipeline].Get());
      /* The two arms put the radiance and the normal in the same slot index, so every slot the
       * incoming layout declares is rebound: leaving the other one's buffer where it was is how a
       * uv slot ends up holding radiance. */
      SDL_GPUBufferBinding runs[4] = {};
      uint32_t count = 0;
      runs[count++] = SDL_GPUBufferBinding{Vtx.Get(), 0};
      if (textured) { runs[count++] = SDL_GPUBufferBinding{Uv.Get(), 0}; }
      runs[count++] = SDL_GPUBufferBinding{lit ? Nrm.Get() : Emit.Get(), 0};
      if (mapped) { runs[count++] = SDL_GPUBufferBinding{Tan.Get(), 0}; }
      SDL_BindGPUVertexBuffers(into.Pass, 0, runs, count);
      bound = wantedPipeline;
    }
    if (!slotBound || boundSlot != batch.MaterialSlot) {
      const SDL_GPUTextureSamplerBinding images[kSubjectImages] = {
          {surface.Colour.Image.Get(), surface.Colour.Sample.Get()},
          {surface.Normal.Image.Get(), surface.Normal.Sample.Get()},
          {surface.MetalRough.Image.Get(), surface.MetalRough.Sample.Get()},
          {surface.Emissive.Image.Get(), surface.Emissive.Sample.Get()}};
      SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kSubjectImages);
      SDL_PushGPUFragmentUniformData(into.Commands, 0, surface.Row.data(),
                                     (uint32_t)(surface.Row.size() * sizeof(float)));
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    SDL_DrawGPUIndexedPrimitives(into.Pass, batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

} // namespace outshine::Render
