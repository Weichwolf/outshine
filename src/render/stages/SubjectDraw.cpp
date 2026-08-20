#include "SubjectDraw.h"

#include <new>

#include "Heap.h"

#include <algorithm>
#include <array>
#include <cmath>
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

/* THE ARM A LAYOUT ENTERS THROUGH, and it is a switch with no `default` on purpose: a ninth layout
 * added to the table in `draw/DrawList.h` stops compiling here until it says which vertex arm it is
 * drawn by, which is the compiler holding what a lookup table would only report (`Enum.2`). */
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
struct S { float4x4 mvp; float4 anc; float4x4 prevMvp; float4 prevAnc; };
/* THE SLOT'S SURFACE ROW. `factor` is glTF's `baseColorFactor.a` and `cut` its `alphaCutoff`, which
 * both arms read; the rest is the metal-rough row only the lit arm reads. `packed_float3` and not
 * `float3`, because a Metal `float3` occupies sixteen bytes and would put `normalScale` four bytes
 * past where the host writes it. */
/* AND THE FOUR uv MATRICES RIDE IN THE SAME ROW (board:1177), two `packed_float3` each -- the two
 * rows of an affine 2x3, so `u'` and `v'` are one dot product apiece and the third row of the 3x3 is
 * the `(0, 0, 1)` no affine map varies.
 *
 * AND ONE SELECTOR PER SOCKET BEHIND THEM (board:1182): 0 reads the first uv set, 1 the second. Four
 * trailing scalars rather than a `float4`, because a `float4` here would want sixteen-byte alignment
 * and the host writes this row as a flat run of floats -- the padding would put every selector four
 * bytes from where it was written. */
struct M { float factor; float cut; float metalness; float roughness;
           float4 base; packed_float3 emissive; float normalScale; float identity;
           /* The dielectric normal-incidence reflectance, already combined from the file's ior and
            * specular factors by `core/Material.h` (board:1205). The shader recomputes nothing. */
           packed_float3 f0;
           /* The same extension's grazing half (board:1428). Carried as its own number because the
            * strength image modulates it exactly as it modulates F0, so a fragment that samples one
            * has already paid for the other. */
           float specularWeight;
           /* `KHR_materials_transmission` AND `KHR_materials_volume` (board:1386). `transmission` is
            * the fraction that passes through; `thickness` is what makes a volume a volume, and the
            * format says so outright -- *if the value is 0 the material is thin-walled*. The
            * attenuation pair is Beer-Lambert's: light travelling `attenuationDistance` through the
            * medium comes out `attenuationColour`, and an infinite distance absorbs nothing. */
           float transmission; float thickness; float attenuationDistance;
           packed_float3 attenuationColour;
           /* `KHR_materials_sheen`: a retroreflective layer over the base. Black disables it, which
            * is the extension's own default and its own rule. */
           packed_float3 sheenColour; float sheenRoughness;
           /* `KHR_materials_clearcoat`: a thin dielectric layer over everything below. Zero weight
            * disables it, which is the extension's own default. */
           float clearcoat; float clearcoatRoughness;
           /* `KHR_materials_anisotropy`: strength and the rotation from the tangent, in radians. */
           float anisotropy; float anisotropyRotation;
           /* `KHR_materials_iridescence`: a thin film whose interference TINTS the specular Fresnel
            * rather than adding a lobe. Zero strength disables it, and so does zero thickness. */
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
/* `environment` IS A CONSTANT RADIANCE FROM EVERY DIRECTION (board:1206), in the same scene-referred
 * units as a light's tint, and zero where the declaration named none. It sits inside this uniform
 * because it IS a light -- the one whose solid angle is the whole sphere. */
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

/* THE TWO STORAGE BUFFERS AS ONE ARGUMENT, so a shading function takes the subject's own geometry
 * rather than two pointers that could be handed over out of step (`I.23`). */
struct Occluders { device const BvhNode *nodes; device const BvhTri *tris; };
#define SUBJECT_OCCLUDERS Occluders{bvhNodes, bvhTris}


/* ONE TAP PER SOCKET, AND THE TEXTURE, ITS SAMPLER AND ITS uv MATRIX ARE NAMED TOGETHER OR NOT AT ALL
 * (board:1177). Written out at each of the thirteen sample sites, the three could be paired wrong --
 * the normal map read through the colour's transform is a picture nobody would attribute to a typo --
 * so the socket is spelled ONCE, here, and every site says only which socket it wants.
 *
 * THE MATRIX IS THE IDENTITY WHERE THE FILE DECLARED NO TRANSFORM, so this is the whole of the
 * extension in the fragment: one multiply-add, no branch, no second arm and no pipeline permutation.
 * The `sin` and `cos` are the host's, once per surface, and have no spelling here. */
/* THE TWO SETS TRAVEL AS ONE VALUE (board:1182) so that no site can hand over the first where the
 * second was meant, or the same one twice -- which is exactly the defect `MultiUVTest` exists to
 * show, and it is a defect two positional `float2` arguments would spell (`I.24`). */
struct Uvs { float2 first; float2 second; };
static inline float2 uvBy(packed_float3 u, packed_float3 v, Uvs uv, float second) {
  /* `mix` AND NOT A BRANCH: the selector is written as exactly 0 or 1 from an enumeration on the
   * host, so this is exact, and the second uv set costs no divergence and no second fragment arm. */
  float3 homogeneous = float3(mix(uv.first, uv.second, second), 1.0);
  return float2(dot(float3(u), homogeneous), dot(float3(v), homogeneous));
}
/* THE PAIRING OF THE TWO VARYINGS HAS ONE SPELLING, here, for the reason the socket does. */
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
/* THE ROW'S F0 MODULATED BY `KHR_materials_specular`'s TWO IMAGES, AND IT IS A MACRO SO THAT EVERY
 * ARM USES IT (board:1205). Putting this in the mapped arm alone was a measured mistake: forcing the
 * mapped arm's F0 to zero left `SpecularTest`'s picture IDENTICAL to every digit, because a panel with
 * no colour or normal map never enters that arm -- and the extension's own textured materials declare
 * no base-colour image either. A white texel is the multiplicative identity, so a surface declaring
 * neither image reaches the same number it would have had. */
#define SUBJECT_SPECULAR_F0(uv) \
  (float3(surface.f0) * specularStrengthMap.sample(specularStrengthSampler, \
                            uvBy(surface.specularStrengthUvU, surface.specularStrengthUvV, (uv), \
                                 surface.specularStrengthUvSecond)).a \
                      * specularTintMap.sample(specularTintSampler, \
                            uvBy(surface.specularTintUvU, surface.specularTintUvV, (uv), \
                                 surface.specularTintUvSecond)).rgb)
/* F90's TAP IS F0's, MINUS THE TINT. `dielectric_f90 = specular` is a SCALAR in the extension, so the
 * strength image's alpha reaches it and the sRGB tint image does not -- a tinted F90 would colour the
 * rim of a surface whose reflection the format says is white there. */
#define SUBJECT_SPECULAR_F90(uv) \
  (surface.specularWeight * specularStrengthMap.sample(specularStrengthSampler, \
                                uvBy(surface.specularStrengthUvU, surface.specularStrengthUvV, (uv), \
                                     surface.specularStrengthUvSecond)).a)
#define SUBJECT_EMISSIVE_TAP(uv) \
  emissiveMap.sample(emissiveSampler, \
                     uvBy(surface.emissiveUvU, surface.emissiveUvV, (uv), surface.emissiveUvSecond))

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
#if SUBJECT_WRITES_SURFACE_IDENTITY
  float4 idn [[color(SUBJECT_IDENTITY_COLOUR_INDEX)]];
#endif
};

/* THE SCREEN-SPACE MOTION OF THE SURFACE UNDER THIS PIXEL (board:1169), and it was the sentinel at
 * every one of the eighteen fragment entry points below until an animated case existed to move
 * something. Both halves of the displacement are here: the vertex moved -- the pose is baked into
 * the position run, so the previous pose arrives as its own run -- and the camera moved, which is
 * the previous view-projection the renderer keeps from the last submitted frame.
 *
 * THE DIVIDE HAPPENS IN THE FRAGMENT because a perspective divide does not commute with the
 * interpolation: dividing at the vertex and interpolating the result would give a motion that is
 * right at three corners and wrong across the triangle.
 *
 * THE SENTINEL SURVIVES AS THE CLEAR VALUE AND ONLY THERE (`SceneTargets.h`): a pixel no fragment
 * reached is "nothing dynamic wrote this", and a pixel one did carries a number.
 *
 * `SUBJECT_SET_MOTION` COPIES `o.pos` AND SO IS WRITTEN AFTER IT: the clip position has to reach the
 * fragment as an interpolated varying, and `[[position]]` arrives there already divided and in
 * pixels. */
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

/* WHICH ATTRIBUTE INDEX EACH RUN ARRIVES AT, STATED ONCE PER RUN (board:1193). The vertex arms below
 * are sixteen stage_in structs over five optional runs, and every index written out per struct is a
 * chance for one of them to disagree with the vertex descriptor `ShapeOf` builds -- `attribute(1)`
 * alone appeared five times before the vertex colour doubled the layouts. */
#define SUBJECT_UV_ATTRIBUTE float2 uv [[attribute(1)]];
#define SUBJECT_UV1_ATTRIBUTE float2 uv1 [[attribute(6)]];
#define SUBJECT_NORMAL_ATTRIBUTE float3 n [[attribute(3)]];
#define SUBJECT_TANGENT_ATTRIBUTE float4 t [[attribute(4)]];
#define SUBJECT_COLOUR_ATTRIBUTE float4 colour [[attribute(7)]];
#define SUBJECT_NO_COLOUR_ATTRIBUTE

/* AND WHAT A LAYOUT WITHOUT THE COLOUR RUN WRITES IN ITS PLACE. It is the same statement as the one
 * white texel a surface with no image binds: white is the multiplicative identity of base colour, so
 * "this primitive declares no COLOR_0" and "the factors alone" are one arm rather than a second set
 * of eighteen fragment entry points. The value is written by the VERTEX arm, so it cannot come from
 * an unbound buffer -- there is no run behind it to be missing. */
#define SUBJECT_NO_VERTEX_COLOUR float4(1.0)

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
#define SUBJECT_SET_SHADING_NORMAL(o, n, f) (o).nrm = float4((n), select(-1.0, 1.0, (f)))
#define SUBJECT_NO_SHADING_NORMAL(o) (o).nrm = float4(0.0, 0.0, 0.0, 1.0)
#else
#define SUBJECT_SET_SHADING_NORMAL(o, n, f) (void)0
#define SUBJECT_NO_SHADING_NORMAL(o) (void)0
#endif

/* WHICH SURFACE THE FRAGMENT WORE (board:1138). EVERY arm writes it and none of them computes it:
 * the value is the slot's own row, so the answer is what the encoder bound rather than anything this
 * shader decided, and an arm that shades nothing still says which surface shaded nothing.
 *
 * A DISCARDED FRAGMENT WRITES NO ATTACHMENT AT ALL, which is why the masked arms may set it before
 * their cut without claiming a surface at a pixel the cut removed.
 *
 * THE OTHER THREE CHANNELS ARE DECLARED ZERO AND ONE rather than left to the driver: a reader that
 * found a plausible value in them would have found whatever the target held. */
#if SUBJECT_WRITES_SURFACE_IDENTITY
#define SUBJECT_SET_SURFACE_IDENTITY(o, m) (o).idn = float4((m).identity, 0.0, 0.0, 1.0)
#else
#define SUBJECT_SET_SURFACE_IDENTITY(o, m) (void)0
#endif

/* WHAT PASSES THROUGH A SURFACE, AND IT IS THE SCENE BEHIND IT (board:1386).
 *
 * `KHR_materials_transmission`: the transmitted light is TINTED BY BASE COLOUR, so a stained window
 * colours what is seen through it and a clear one does not. `KHR_materials_volume` adds the medium:
 * over a path of `thickness` through it, Beer-Lambert attenuates by
 * `exp(-thickness / attenuationDistance * -log(attenuationColour))` -- the extension's own form,
 * which reduces to the attenuation colour at exactly one attenuation distance and to no absorption at
 * all when that distance is infinite. A thin wall has zero thickness and therefore no absorption,
 * which is the same expression rather than a second branch.
 *
 * THE BACKGROUND IS SAMPLED AT THE FRAGMENT'S OWN PIXEL AND IS NOT REFRACTED, and that is a declared
 * shortfall rather than an oversight: bending the sample by the surface's normal and index is what
 * makes a thick lens displace what is behind it, and this arm shows it undisplaced. The capability
 * answers in both directions -- the tint and the absorption are exact, the displacement is absent. */
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
/* BOTH UV SETS REACH THE FRAGMENT AND THE SOCKET'S OWN SELECTOR DECIDES WHICH IT READS
 * (board:1182). Two varyings and not one, because a material may read the first set on one socket
 * and the second on another -- `MultiUVTest` does exactly that -- so a single interpolated pair
 * would have to be chosen per material where the choice is per reference.
 *
 * AN ARM WITH NO SECOND RUN WRITES ZERO INTO `uv1` AND NOTHING READS IT. That is not a fall-back:
 * a surface whose reference names the second set can only be drawn through a layout that carries
 * the run, because `SetMesh` refuses the pairing outright, so the zero has no path to a sampler.
 *
 * `colour` IS glTF's `COLOR_0` AND IT IS INTERPOLATED WHERE `emitted` IS FLAT (board:1193): the
 * radiance is one declared value per part and the vertex colour is per vertex, so taking the
 * provoking vertex's here would draw every triangle in the colour of one of its corners. */
struct SOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour;
              float3 emitted [[flat]]; SUBJECT_MOTION_VARYINGS };

/* ONE EMITTED VERTEX ARM, WRITTEN ONCE AND INSTANTIATED PER LAYOUT (board:1193). Six of them differ
 * only in which runs the stage_in struct declares and which of them the varyings take, and the
 * vertex colour turned three arms into six: written out, the same four lines of placement
 * arithmetic would stand six times and a repair would have six places to miss. */
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

/* The declared radiance, not shaded. On an opaque arm the fourth channel is the direct fraction a
 * display transfer weights its occlusion by, and for a surface that emits what it was declared to
 * emit all of it is direct. */
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

/* A SURFACE WITH NO NORMAL SHADES NOTHING, so what it emits and what passes through it are all it
 * has -- there is no reflected lobe to add and none is invented. */
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

/* Greater-or-equal is kept, less-than discards: glTF states the surviving side, not the cut side
 * ("If the alpha value is greater than or equal to the alphaCutoff value then it is rendered as
 * fully opaque"), and the two differ on exactly the texels a linear ramp puts at the cutoff. */
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

/* STRAIGHT, NOT PREMULTIPLIED: the colour goes out unweighted and the blend state applies the
 * weight, so the same fragment function would be correct under either convention only by accident.
 * The one convention this whole comparison is stated in is straight alpha. */
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
struct LOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour; float3 n; float3 p;
              float3 lp; SUBJECT_MOTION_VARYINGS };

/* THE LIT VERTEX ARMS, on the same terms as the emitted ones (board:1193) and with the normal in
 * every one of them: what varies between the six is the two uv runs and the vertex colour. */
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

/* THE ROUGHNESS, THE METALNESS AND THE EMITTED RADIANCE ARE ARGUMENTS AND NOT READS OF THE ROW,
 * because a textured surface states all three per texel and the untextured arm states them per
 * material -- one function that read the row would force the mapped arm to write the loop over the
 * lights a second time. */
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
  /* `KHR_materials_iridescence` WITHOUT ITS THICKNESS TEXTURE SITS AT THE MAXIMUM, which is the
   * extension's own implicit sample of 1.0 through `mix(minimum, maximum, texture.g)`. The minimum
   * therefore reaches nothing yet and `board:1405` is what will make it reachable. */
  float iridescence = surface.iridescence;
  float iridescenceThickness = surface.iridescenceThicknessMax;
  /* *THE THIN-FILM THICKNESS OF 0.0 NM DISABLES THE IRIDESCENCE*, and it is stated here rather than
   * inside the lobe: the lobe answers what a film of that thickness reflects, and a film of no
   * thickness is not a film. */
  if (!(iridescenceThickness > 0.0)) { iridescence = 0.0; }
  float3 v = normalize(-p);
  float a = roughness * roughness;
  float a2 = a * a;
  float3 diffuseColour = albedo * (1.0 - metalness);
  float3 f0 = mix(dielectricF0, albedo, metalness);
  /* A CONDUCTOR'S GRAZING REFLECTANCE IS UNITY AND THE EXTENSION SAYS SO BY NAMING ITS OWN `f90`
   * *dielectric* (board:1428). The same blend the F0 pair takes, so a half-metallic row crosses
   * between the two the way every other quantity in this shader does. */
  float f90 = mix(dielectricF90, 1.0, metalness);
  float nv = max(dot(n, v), 1.0e-6);
  /* THE ENERGY THE SINGLE BOUNCE LOST, PUT BACK (board:1408). glTF's Appendix B traces one bounce off
   * the microfacets and drops everything that would have left after two; the core specification says
   * outright that an implementation of the BRDF **MAY** vary, and this is a derived correction with no
   * free parameter rather than a match to a renderer.
   *
   * READ ONCE PER FRAGMENT because it is a function of the view alone, and applied to the SPECULAR term
   * only -- the diffuse half is not what lost the energy.
   *
   * IT TAKES THE BASE F0 EVEN WHERE IRIDESCENCE TINTS THE FIRST BOUNCE, and that is a named
   * simplification: the multiplier is driven by the AVERAGE Fresnel over the hemisphere, and a thin
   * film's average is not its normal-incidence value -- a second integral this does not carry. The two
   * are the same wherever the film is absent, which is every surface but one kind.
   *
   * THE ANISOTROPIC ARM FEEDS IT THE BASE ROUGHNESS, because the lost energy is a property of how much
   * the facets shadow each other and the stretched lobe redistributes that rather than changing its
   * total. */
  float3 energyScale = ggxEnergyScale(f0, roughness, nv);
  /* THE RAY LEAVES FROM THE SURFACE AND NOT FROM INSIDE IT. Offsetting along the shading normal as
   * well as starting at `count.y` is what keeps a facet whose normal map tilts it towards the light
   * from re-finding its own triangle. */
  /* `KHR_materials_anisotropy`'s OWN FRAME, built once: the direction is the tangent turned by the
   * declared rotation inside the tangent plane, and the bitangent follows from it and the normal.
   * A subject with no tangent hands a zero vector, which the extension's own requirement makes a
   * material error rather than something to invent a direction for -- so the lobe stays round. */
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
    float nh = max(dot(n, h), 0.0);
    float vh = max(dot(v, h), 0.0);
    /* ONE LOBE AND ONE FRESNEL, CHOSEN, AND ONE COMBINATION APPLIED TO WHICHEVER WAS CHOSEN. The two
     * extensions that reach in here reach into DIFFERENT halves -- `KHR_materials_anisotropy`
     * replaces the lobe's shape and leaves the Fresnel alone; `KHR_materials_iridescence` replaces
     * the Fresnel and leaves the lobe alone -- so a surface declaring both is the product of the two
     * substitutions and needs no third arm. */
    float lobe = brdfLobe(a2, nl, nv, nh);
    if (anisotropic) {
      /* The extension's own parametrisation: `at` along the direction, `ab` across it. */
      float at = mix(a, 1.0, anisotropy * anisotropy);
      float ab = a;
      lobe = brdfAnisotropicDistribution(nh, dot(anisoT, h), dot(anisoB, h), at, ab) *
             brdfAnisotropicVisibility(nl, nv, dot(anisoT, v), dot(anisoB, v),
                                       dot(anisoT, toward), dot(anisoB, toward), at, ab);
    }
    Brdf reflected;
    if (iridescence > 0.0) {
      /* *A MODIFIED FRESNEL REFLECTANCE TERM THAT ACCOUNTS FOR INTER-REFLECTIONS* -- the extension's
       * own sentence, and the whole of what it changes. The strength is a blend between the plain
       * curve and the film's, which is what `iridescence_strength` means in both of its pseudocode
       * blocks, and the base is then weighted by `1 - max(F)` rather than channelwise. */
      float3 filmed = iridescenceFresnel(vh, iridescenceThickness, surface.iridescenceIor, f0);
      reflected = brdfRgbMix(diffuseColour, mix(brdfFresnel(f0, f90, vh), filmed, iridescence), lobe);
    } else {
      reflected = brdfCombine(diffuseColour, brdfFresnel(f0, f90, vh), lobe);
    }
    reflected.specular *= energyScale;
    /* `KHR_materials_sheen` LAYERED OVER THE BASE, and the base SCALED so the two together send out
     * no more than arrived: *sheen_material = sheenColor * sheen_brdf + material *
     * sheen_albedo_scaling*, the extension's own line. A black sheen colour makes the scaling exactly
     * one and the lobe exactly zero, so a surface that declares none is the arithmetic it was. */
    float3 sheen = sheenColour * sheenDistribution(nh, sheenRoughness) *
                   sheenVisibility(nl, nv, sheenRoughness);
    float keep = sheenAlbedoScaling(sheenColour, nv, sheenRoughness);
    float3 layered = (reflected.diffuse + reflected.specular) * keep + sheen;
    /* `KHR_materials_clearcoat` LAYERED OVER EVERYTHING BELOW IT, and the extension's own operator:
     * *the layer is weighted with weight * fresnel(ior). The base is weighted with 1 - (weight *
     * fresnel(ior))*, at a fixed ior of 1.5 -- an F0 of 0.04. **The coat reuses the metal-rough
     * SPECULAR lobe rather than introducing one**, which the extension states outright, so what
     * varies is its roughness and nothing else.
     *
     * ITS NORMAL IS THE GEOMETRIC ONE BY THE FORMAT'S RULE, not by omission: *if
     * clearcoatNormalTexture is not given, no normal mapping is applied to the clear coat layer, even
     * if normal mapping is applied to the base material*. */
    if (clearcoat > 0.0) {
      float coatA = clearcoatRoughness * clearcoatRoughness;
      float coatA2 = coatA * coatA;
      /* `NdotV` AND NOT `VdotH`, AND THE EXTENSION SAYS WHY IN ITS OWN WORDS (board:1441): *we compute
       * the microfacet Fresnel term with NdotV instead of VdotH. That means that we ignore the
       * orientation of the microsurface.* Its pseudocode is
       * `clearcoat_fresnel = 0.04 + (1 - 0.04) * (1 - abs(VdotNc))^5`.
       *
       * THE HALF-VECTOR FORM WAS NEARLY CONSTANT ACROSS A SUBJECT and that is what the measurement
       * caught: under one distant light `v.h` barely moves over a sphere, so the base was attenuated by
       * about the same factor everywhere, where a coat's reflectance runs from 0.04 head-on to 1 at
       * grazing. [MEASURED] on `shaded-sphere-metal-clearcoat`, ours over the oracle by view angle:
       * 0.758 at `n.v` 0.9-1.0, 0.995 at 0.7-0.9, 2.132 at 0.0-0.1 -- **too dark where the coat should
       * transmit and too bright where it should reflect, crossing in the middle**, which is the signature
       * of a constant attenuation standing in for a view-dependent one. */
      float coatF = 0.04 + 0.96 * pow(1.0 - nv, 5.0);
      float coatLobe = brdfLobe(coatA2, nl, nv, nh);
      float weight = clearcoat * coatF;
      layered = layered * (1.0 - weight) + float3(weight * coatLobe);
    }
    sum = sum + layered * nl * attenuation * light.tint.rgb;
  }
  /* THE ENVIRONMENT, GATHERED ANALYTICALLY BECAUSE IT IS CONSTANT (board:1206).
   *
   * THE DIFFUSE HALF IS EXACT AND NOT AN APPROXIMATION. A uniform radiance L from every direction
   * leaves a Lambert surface as `albedo * L`: the cosine-weighted integral of a constant over the
   * hemisphere is pi and the Lambert BRDF is albedo/pi, so the two cancel.
   *
   * THE SPECULAR HALF IS SCHLICK'S FRESNEL AT THE VIEW ANGLE, WHICH IS EXACT FOR A MIRROR AND AN
   * OVER-ESTIMATE FOR A ROUGH SURFACE -- and the direction of the error is stated because the
   * alternative was worse. Under a constant environment a perfect mirror returns `F(nv) * L`, since
   * every direction carries the same L; that is the roughness-0 limit, and `SpecularTest` and
   * `IORTestGrid` are roughness-0 panels, so the row the corpus measures is the row this gets right
   * with no approximation in it.
   *
   * KARIS'S TWO-TERM ENVIRONMENT-BRDF FIT WAS TRIED AND REJECTED, WITH THE NUMBER. At `roughness = 0`
   * and `nv = 1` it returns `F0 * 0.9941 + 0.00588`, which for a dielectric's `F0 = 0.04` is 0.0457
   * against an exact 0.04 -- **14 % high, on exactly the panels this term exists to make decidable**,
   * and it would have read as this engine's F0 being wrong.
   *
   * WHAT IS OWED, AND IT IS OWED RATHER THAN HIDDEN: a rough surface's directional albedo is below
   * `F(nv)` because a GGX lobe loses energy to masking, so a rough dielectric under an environment is
   * drawn too bright here. No corpus case measures that today; the first one that does is what pays
   * for the split-sum. */
  const float nvClamped = clamp(nv, 0.0, 1.0);
  /* THE SAME FRESNEL THE LOBE USES, AND IT USED TO BE A SECOND SPELLING OF IT (board:1428). Written
   * out here, this term carried a grazing reflectance of one no matter what the file declared, so a
   * panel asking for NO specular reflection still wore the environment as a rim -- and every reading
   * of it came out identical because the row's own number never reached this line. [MEASURED] on
   * `SpecularTest`'s `specularFactor = 0` panel, whose oracle is exactly zero: 0.01059 linear as a
   * mean, a rim peaking at 0.242 against a black, smooth, non-metallic body.
   *
   * `nv` AND NOT `vh` IS THIS TERM'S OWN ANGLE and that has not changed: the mirror direction under a
   * constant environment is the view's own reflection, so the half-vector is the normal. */
  const float3 specularEnvironment = brdfFresnel(f0, f90, nvClamped);
  sum = sum + lights.environment.rgb * (diffuseColour + specularEnvironment);
  return sum + emitted;
}

/* THE ARM WITH NO UV AT ALL, so the emitted radiance can only be the factor: there is no
 * coordinate to sample an image with, and glTF's emissive is the factor where the material
 * declares no image. */
static inline float3 shade(constant M &surface, constant Lights &lights, Occluders occluders,
                           float3 localM, float3 n, float3 p, float3 albedo) {
  return shadeRow(surface, lights, occluders, localM, n, p, albedo, surface.metalness,
                  surface.roughness, float3(surface.f0), surface.specularWeight,
                  surface.emissive, float3(0.0));
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

/* THE LIT TEXTURED ARMS, split out for the same reason the emitted ones are: the sampled colour is
 * the BASE COLOUR the BRDF is evaluated with -- not a factor applied afterwards -- because glTF's
 * base colour feeds both halves of the model and multiplying a shaded result by it would tint the
 * specular lobe of a dielectric, which is a metal's behaviour. */
static const char *kSubjectLitTexturedMsl = R"(
/* glTF's EMISSION: `emissiveFactor` TIMES `emissiveTexture` (Specification.adoc:1436), and a slot
 * that declares no image binds one white texel so that the product is the factor alone. */
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

/* THE NORMAL-MAPPED ARM: the same BRDF over a normal the file states per texel instead of per
 * vertex, and the tangent basis is what makes the two the same quantity. THE BASIS ITSELF IS
 * `stages/NormalFromMap.h` -- spliced in below, stated there once in MSL and once in C++, and tied
 * by a shader test of its own. What this arm keeps is the SAMPLING: the
 * tap, the material's scale and the facing the fragment arrived with.
 *
 * ROUGHNESS AND METALNESS COME FROM THE FILE'S OWN IMAGE where it declares one, green and blue,
 * multiplied by the factors -- which is glTF's rule (Specification.adoc:1394) and is not an extra:
 * the alternative is the default factor of 1, and at roughness 1 the GGX distribution is the
 * constant 1/pi, so the lobe stops depending on the normal and a normal-mapped picture comes back
 * flat. */
static const char *kSubjectMappedMsl = R"(
struct MOut { float4 pos [[position]]; float2 uv; float2 uv1; float4 colour; float3 n; float3 p;
              float3 lp; float4 t; SUBJECT_MOTION_VARYINGS };

/* THE NORMAL-MAPPED VERTEX ARMS (board:1193). The first uv set, the normal and the tangent are in
 * every one of them -- that triple IS what mapped means -- so what varies is the second uv set and
 * the vertex colour. */
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
                                 texture2d<float> specularStrengthMap,
                                 sampler specularStrengthSampler,
                                 texture2d<float> specularTintMap, sampler specularTintSampler,
                                 MOut in, bool front) {
  float4 orm = SUBJECT_METALROUGH_TAP(SUBJECT_UVS(in));
  /* `KHR_materials_specular`'s TEXTURES MODULATE THE ROW'S F0 (board:1205). The strength image
   * carries a scalar in ALPHA and multiplies the specular factor; the tint image is sRGB and
   * multiplies the colour factor.
   *
   * THE ROW'S F0 IS ALREADY CAPPED AND THESE MULTIPLY AFTER IT, WHICH DIFFERS FROM THE FORMAT IN ONE
   * CORNER AND THE CORNER IS NAMED. The extension caps `((ior-1)/(ior+1))^2 * specularFactor *
   * strength` at 1 BEFORE the tint; this multiplies a capped product. The two differ only where the
   * uncapped product exceeds 1, which needs `specularFactor > 1 / ((ior-1)/(ior+1))^2` -- above 25
   * for a dielectric -- since the base term is below 1 for every ior. No asset in the corpus reaches
   * it: `SpecularTest`'s largest factor is 1. */
  const float3 specularF0 = SUBJECT_SPECULAR_F0(SUBJECT_UVS(in));
  float3 albedo = surface.base.rgb * SUBJECT_COLOUR_TAP(SUBJECT_UVS(in)).rgb * in.colour.rgb;
  const float3 shadingNormal = mappedNormal(surface, normalMap, normalSampler, in, front);
  /* THE SAME TAP THE NORMAL CAME FROM, FOR ITS LENGTH. Sampled here rather than returned from
   * `mappedNormal` because that function's pair -- the colour and the normal it was shaded with -- is
   * deliberately inseparable (board:1122) and a third member would loosen it; the texture and sampler
   * are the same pair of arguments, so this costs no second fetch after the compiler has seen both. */
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

/* THE FIVE VERTEX LAYOUTS, as the pipeline states them. Every attribute sits in a buffer of its own
 * and at offset 0: the subject's positions, its uvs, its normals, its tangents and its declared
 * radiance come out of the consumer as separate runs, and interleaving them here would be a copy
 * nobody asked for. The untextured pipeline has no uv slot at all rather than an empty one. */
struct VertexShape {
  /* SEVEN, BECAUSE THE SECOND UV SET AND THE VERTEX COLOUR ARE RUNS OF THEIR OWN (board:1182,
   * board:1193): the mapped arm already binds position, uv, normal and tangent, a pass that attaches
   * a velocity target binds the previous frame's positions beside them, `MultiUVTest`'s second uv set
   * is the sixth and `BoxVertexColors`'s `COLOR_0` is the seventh. */
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
  /* THE SECOND UV SET IMMEDIATELY BEHIND THE FIRST (board:1182), at its own attribute location: the
   * two are the same kind of quantity and a reader looking for one finds the other beside it. Its
   * SLOT is its place in this sequence, so the encoder's binding order and this list stay one
   * statement whichever runs a layout carries. */
  if (CarriesUv1(layout)) {
    shape.Buffers[shape.Count] = Run(shape.Count, 2);
    shape.Attributes[shape.Count] = At(6, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
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
  /* THE VERTEX COLOUR LAST OF WHAT THE LAYOUT CARRIES (board:1193), and four floats wide because its
   * alpha multiplies base colour's: a three-wide run here would leave `alphaMode` reading a coverage
   * the file's own COLOR_0 had already changed. */
  if (CarriesColour(layout)) {
    shape.Buffers[shape.Count] = Run(shape.Count, 4);
    shape.Attributes[shape.Count] = At(7, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
    ++shape.Count;
  }
  /* THE RUN IS BOUND EXACTLY WHERE THE SHADER DECLARES IT, which is where the pass attaches a
   * velocity target: the same one answer splices the attribute into the vertex struct and adds it
   * here, so a layout that carried a run no shader reads has no spelling. */
  if (writesVelocity) {
    shape.Buffers[shape.Count] = Run(shape.Count, 3);
    shape.Attributes[shape.Count] = At(5, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
    ++shape.Count;
  }
  return shape;
}

/* HOW MANY UNIFORM SLOTS EVERY FRAGMENT OF THIS UNIT DECLARES. The image count is `kSubjectImages`
 * in the header, beside the four sockets a surface names, because the row that carries their uv
 * matrices is sized from it too (board:1177). */
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
  /* EVERY VERTEX LAYOUT THE TABLE DECLARES, TWO FACINGS, THREE ALPHA MODES -- and the count is
   * COUNTED here rather than named, because `kPipelines` sizes the array and does not answer this
   * (board:1187): the array is indexed by the whole `SurfaceKind` enumeration, so it is 80 slots and
   * this loop fills 48 of them. The facing is the SLOT's
   * because glTF states it per material -- `TextureSettingsTest` hides a green checkmark behind a
   * polygon facing the wrong way and lets the flag decide which of the two is seen, and one cull
   * mode for the whole subject draws the wrong cell whichever way it is set. */
  /* THE TWO TRANSMISSIVE KINDS ARE BUILT ONLY WHERE THE UNIT WAS GIVEN A BACKGROUND (board:1386).
   * What passes through glass is the scene behind it, which is a texture the transmissive PASS binds
   * and the opaque one has not got -- so a plan that draws no glass builds 48 pipelines exactly as
   * before and pays nothing for a capability it did not ask for. `SetMaterials` still refuses a
   * transmissive slot when no background was given, and the refusal now names what is missing rather
   * than what cannot be done. */
  const bool glass = Behind != nullptr;
  for (const SurfaceKind kind : {SurfaceKind::Opaque, SurfaceKind::Masked, SurfaceKind::Blended,
                                 SurfaceKind::ThinTransmissive, SurfaceKind::Refractive}) {
    if (!glass && (kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive)) {
      continue;
    }
    /* A TRANSMISSIVE FRAGMENT ALREADY CARRIES WHAT IS BEHIND IT, so it is written straight rather
     * than composited: the arm samples the background, attenuates it and adds its own reflection,
     * and a blend on top of that would apply the coverage a second time. */
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
    /* THE IDENTITY TARGET IS NOT BLENDED EITHER, and for a stronger reason than the normal's: an
     * `over` composite of two slot indices is an index no surface has, so a blended pixel would name
     * a material that is not in the table. The last writer's slot is the only reading of this
     * attachment that is an identity at all. */
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

/* **EVERY STREAM OF ONE POSE, IN ONE COPY PASS, INTO BUFFERS THE TOPOLOGY ALREADY OWNS**
 * (board:1463).
 *
 * WHAT THIS REPLACES IS NOT ARITHMETIC, IT IS TEN DRIVER OBJECTS A FRAME. `Fill` creates a device
 * buffer, creates a transfer buffer, maps it, submits its own command buffer and releases the
 * transfer -- and a pose called it ten times, so an animated subject took and returned roughly
 * 1.7 kB of allocator memory and ten command submissions on every advance while nothing about its
 * SIZES had changed. [MEASURED] `submitting` moved the heap on 245 frames of 250 and `drawing` on
 * 243, netting only 4 pose-matched pairs a lap apart: a take-and-return pair straddling the
 * boundary, which is exactly what a per-frame creation looks like from the allocator's side.
 *
 * THE SHAPE IS THE SHIPPED ONE AND NOT AN INVENTION. A static index buffer with dynamic vertex
 * streams is what every engine that animates a mesh does; the buffer belongs to the topology and
 * the pose WRITES it. Here the sizes are a function of `NVerts` and the layout flags, neither of
 * which moves between two poses of one subject, so `Held_` compares an integer and the buffer
 * survives.
 *
 * CYCLING IS WHY THIS IS SAFE WITHOUT A FENCE. The device may still be reading last frame's
 * contents, so both the map and the upload cycle -- SDL's own rename mechanism, bounded by the
 * frames in flight rather than by the frame count, which is what makes the steady state take
 * nothing at all. */
bool SubjectDraw::Cross(Crossing *what, size_t count, std::string &error) {
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
    /* SIXTEEN, because a transfer offset is read by the copy engine and an unaligned one is a
     * portability question nobody should have to answer per backend. */
    total = (total + one.Bytes + 15u) & ~15u;
  }
  if (total == 0) { return true; }

  if (StagingBytes_ < total || !Staging_) {
    SDL_GPUTransferBufferCreateInfo wanted{};
    wanted.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    wanted.size = total;
    Staging_ = OwnedTransfer(Device, SDL_CreateGPUTransferBuffer(Device, &wanted));
    if (!Staging_) {
      StagingBytes_ = 0;
      error = std::string("the pose's staging buffer found no room on the device: ") + SDL_GetError();
      return false;
    }
    StagingBytes_ = total;
  }

  auto *const mapped = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(Device, Staging_.Get(), true));
  if (mapped == nullptr) {
    error = std::string("the pose's staging buffer did not map: ") + SDL_GetError();
    return false;
  }
  uint32_t at = 0;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || what[one].From == nullptr) { continue; }
    std::memcpy(mapped + at, what[one].From, what[one].Bytes);
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_UnmapGPUTransferBuffer(Device, Staging_.Get());

  SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(Device);
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  at = 0;
  for (size_t one = 0; one < count; ++one) {
    if (what[one].Bytes == 0 || what[one].From == nullptr) { continue; }
    const SDL_GPUTransferBufferLocation source{Staging_.Get(), at};
    const SDL_GPUBufferRegion into{what[one].Into->Get(), 0, what[one].Bytes};
    SDL_UploadToGPUBuffer(copy, &source, &into, true);
    at = (at + what[one].Bytes + 15u) & ~15u;
  }
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  return true;
}

/* ONE IMAGE ON THE DEVICE, IN LINEAR f32. One texel of white where the surface declares none, so
 * every slot's binding is complete -- it is never a stand-in for a missing texture, because the
 * pipeline that draws a surface declaring none does not sample it, and white is the multiplicative
 * identity of the two arms that do.
 *
 * WHETHER THE THREE COLOUR CHANNELS CARRY THE sRGB TRANSFER IS THE SOCKET'S QUESTION and is answered
 * by the caller: glTF puts base colour and emissive in sRGB and the normal, occlusion and
 * metallic-roughness maps in linear. Alpha never carries it on either arm. */

/* WHETHER A TEXTURE MAY CARRY MORE THAN ONE LEVEL AT ALL. `false` while `board:1130` is open: enabling
 * the chain moves `normal-tangent` from 229.330177 to a saturated 255 and turns five derived metrics
 * red, and the acceptance stated before that round -- ours inside the oracle's own 0.04859..0.08692 --
 * was not met. It is a named constant rather than a commented-out line so that the two questions stay
 * apart: WHICH FILTER (this file answers, from the file) and HOW MANY LEVELS (board:1130 answers). */
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
  /* A DIRECTION TEXTURE'S ALPHA IS REINTERPRETED, and that is said here rather than left to be
   * discovered (board:1130). glTF gives the normal texture's alpha no meaning and the shader samples
   * only `.xyz`, so the channel is free -- and it is where the mip chain keeps the MEAN RESULTANT
   * LENGTH of the normals it averaged, which the shader turns into roughness by Toksvig's factor. At
   * level 0 nothing has been averaged yet, so the value is exactly 1 whatever the file's own alpha
   * happened to be; taking the file's byte would make the correction a function of an undefined
   * channel. */
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
  /* THE WHOLE CHAIN. Without it a minified texture is point-sampled at level 0, which is aliasing
   * rather than fidelity: `normal-tangent-mirror` samples this map at 1.42 texels per screen pixel
   * (board:1130). */
  uint32_t levels = 1;
  for (uint32_t extent = width > height ? width : height; extent > 1u; extent /= 2u) { ++levels; }
  /* HOW MANY LEVELS THIS TEXTURE ACTUALLY HAS, which is where "no mipmaps" is expressed -- NOT at the
   * sampler's LOD clamp (board:1134). A file naming 9728 or 9729 asks for one level; and while
   * `board:1130` holds the chain back, every texture asks for one whatever it declared. */
  if (texture.Mip == SubjectMip::None || !kChainIsReadable) { levels = 1; }
  wantedTexture.num_levels = levels;
  wantedTexture.sample_count = SDL_GPU_SAMPLECOUNT_1;
  bound.Image = OwnedTexture(Device, SDL_CreateGPUTexture(Device, &wantedTexture));

  /* EVERY LEVEL IS BUILT AND UPLOADED HERE rather than generated on the device, because the device
   * generator has no way to be told that a texel is a direction -- and a normal chain that averaged
   * without renormalising would be a different picture that no flag records. */
  /* WHICH CHANNELS CARRY A CHOICE RATHER THAN AN AMOUNT, MEASURED FROM THIS TEXTURE'S OWN TEXELS. A
   * DIRECTION map is exempt because its first three channels are one vector: snapping a component and
   * then renormalising would be two rules fighting over the same texel. */
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
  /* EACH FILTER FROM ITS OWN DECLARATION (board:1134). These were both `texture.Magnify`, so a file
   * asking to point-sample when its texels outnumber the pixels got a linear blend -- and two corpus
   * subjects ask for exactly that with `NEAREST_MIPMAP_LINEAR`. */
  wantedSampler.min_filter = FilterOf(texture.Minify);
  wantedSampler.mag_filter = FilterOf(texture.Magnify);
  /* BETWEEN LEVELS, ALSO FROM THE FILE. This was a constant `LINEAR`, which happened to agree with
   * every corpus subject that declares a mip mode at all -- and agreeing by luck is not the same as
   * being told. `None` is a declaration too: 9728 and 9729 ask for ONE level. */
  wantedSampler.mipmap_mode = texture.Mip == SubjectMip::Nearest
                                  ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                                  : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  /* THE SELECTION IS STILL PINNED TO LEVEL 0, AND THE BLOCKER IS NOW NAMED RATHER THAN OPEN (board:1130).
   * Setting this to `levels - 1` makes `normal-tangent` and `normal-tangent-mirror` saturate at 255
   * codes, from 229.330177 and 184.356962. THE PICTURE BOUND DOES NOT MOVE -- 20 of 34 cases within it
   * either way, and no test changes verdict -- so what decides this is one metric, not the tail.
   *
   * A SWEEP SAYS THE CAUSE IS NOT DEPTH: level 1 ALONE saturates (0 -> 229.330177, 1 -> 255, and 2, 4, 8
   * and 20 all 255), so a single halving does it and no argument about reaching the top of the chain
   * survives. What one halving does is FLATTEN THE NORMAL MAP, and
   * `rowN_pairM_geometry_matches_normalmap_p95_relative` goes 0.13879225 -> 0.27571386 against a bound
   * of 0.1579751 on five rows.
   *
   * THAT METRIC IS NOT COLLATERAL AND IT IS NOT THE WRONG INSTRUMENT. It compares two regions of THIS
   * render -- a 516-triangle dome cell against the normal-mapped quad that imitates it -- and its bound
   * is the geometric mean of two measured populations, correct basis against flipped handedness,
   * separated by 4.53x. A filtered normal is flatter and the geometry it imitates does not flatten, so
   * the metric is reporting exactly what it was built to report.
   *
   * TOKSVIG'S TERM WAS THE NAMED BLOCKER AND IT IS LIVE -- this paragraph used to say it was
   * "deliberately not taken", which stopped being true at `board:1131` and was not re-read for several
   * rounds. `TexelChain.h` accumulates the resultant length into alpha and `roughenedBy` consumes it.
   * SO THE FLAG WAS FLIPPED AND THE CORPUS RUN TWICE, over one prepared population of 27 cases:
   * criteria met 23 -> 21, within the picture bound 11 -> 10, and FOUR cases worse with none better.
   * `NormalTangentTest` goes 5 failing metrics to 16 and both tangent cases saturate at 255 codes.
   *
   * AND THE FAILURE IS NOT FLATTENING, which is why the conclusion outlived its reason. A region whose
   * reference is BLACK -- 0 on ours and 0 on the oracle, an exact agreement -- comes back at 0.18065128
   * while its partner region stays at exactly 0. Flattening moves a value toward the geometric normal's
   * answer; it cannot light up a region that is black by construction. The candidate is a normal that
   * changed DIRECTION, and the paths read so far all bound magnitude and none bounds orientation. */
  /* THE LOD CLAMP IS LEFT OPEN, AND THAT IS A CORRECTION RATHER THAN A RELAXATION (board:1134).
   * `max_lod = 0` reads as "no mipmaps" and is not: lambda is clamped to `[min_lod, max_lod]` BEFORE
   * the magnification test, so `lambda <= 0` became true everywhere and the MAGNIFICATION filter was
   * used at every pixel of every texture. `min_filter` was unreachable by construction -- which is why
   * setting it from the file's own `minFilter` changed not one byte of `normal-tangent` until this
   * line moved. 1000 is Vulkan's `VK_LOD_CLAMP_NONE`, the spelling for "do not clamp"; the LEVEL
   * COUNT above is what bounds which levels exist, and that is the knob that means what it says. */
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
  /* THE TRANSFER IS THE EXTENSION'S OWN AND THE TWO DIFFER (board:1205): the strength image carries a
   * scalar factor in alpha and is LINEAR, the tint image carries an F0 colour and is sRGB-ENCODED.
   * Uploading either through the other's transfer is a picture and not an error. */
  slot.SpecularStrength = Upload(material.SpecularStrength, Transfer::Linear, TexelKind::Value);
  slot.SpecularTint = Upload(material.SpecularTint, Transfer::Srgb, TexelKind::Value);

  const Material &row = material.Row;
  /* THE SLOT'S IDENTITY IS ITS POSITION IN THIS TABLE, taken before the push (board:1138), so it is
   * the same number `DrawBatch::MaterialSlot` names and cannot be assigned a second time anywhere. */
  const float identity = (float)(Slots.size() + 1u);
  /* THE DIELECTRIC F0 IS COMPUTED WHERE IT IS DEFINED AND CARRIED HERE AS A NUMBER (board:1205).
   * `KHR_materials_ior` and `KHR_materials_specular` are two spellings of one quantity, so the row
   * holds the quantity and the shader holds no arithmetic that could disagree with `Material.h`'s. */
  float f0[3];
  DielectricF0(row, f0);
  /* THE COUNT AND THE LIST ARE HELD TOGETHER BY THE COMPILER AND NOT BY A COMMENT. `slot.Row` is
   * longer than this prefix, so a braced assignment straight into it would zero-fill a field somebody
   * forgot to append and the shader would read a plausible zero -- a roughness of 0, a strength of 0
   * -- with nothing anywhere to say a value went missing. Deduced here and checked against the
   * constant, so the mistake is a compile error in both directions. */
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
  /* THE FOUR uv MATRICES, IN THE ORDER THE FOUR IMAGES ARE BOUND (board:1177), narrowed to f32 here
   * and nowhere earlier -- the reader composes in its own width and the device is the boundary. The
   * table is walked rather than written out four times so that the row's order and the sampler's
   * order are one statement: a socket appended to one and not the other has no spelling. */
  const SubjectTexture *const images[kSubjectMaterialImages] = {&material.Colour, &material.Normal,
                                                        &material.MetalRough, &material.Emissive,
                                                        &material.SpecularStrength,
                                                        &material.SpecularTint};
  size_t at = (size_t)kSurfaceScalars;
  for (const SubjectTexture *image : images) {
    for (const double element : image->Uv.M) { slot.Row[at++] = (float)element; }
  }
  /* THE FOUR SELECTORS BEHIND THE FOUR MATRICES, IN THE SAME ORDER AND OUT OF THE SAME WALK
   * (board:1182), so a socket added to one and not the other has no spelling. Exactly 0 or 1, from
   * the enumeration and never from a number a call site chose, which is what makes the shader's
   * `mix` an exact selection rather than a blend of two coordinates. */
  for (const SubjectTexture *image : images) {
    slot.Row[at++] = image->Set == UvSet::Second ? 1.0f : 0.0f;
  }
  Slots.push_back(std::move(slot));
}

bool SubjectDraw::SetMaterials(const std::vector<SubjectMaterial> &materials, std::string &error) {
  /* A NEW TABLE RETIRES THE OLD LIST, because a batch names a slot INDEX and the indices it names
   * belonged to the table being replaced. `SetMesh` is what re-validates them, so until it runs
   * there is no list -- which is what makes "encode a batch against a table that no longer holds its
   * slot" unspellable rather than a bounds check nobody reaches. */
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
    /* THE REFUSAL SURVIVES AND ITS REASON NARROWED (board:1386). What is transmitted through a sheet
     * is the scene behind it, and this unit can now draw that -- but only in a pass that was GIVEN
     * that scene. On the opaque pass there is none, and the message names what is missing rather
     * than what cannot be done. */
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
  if (NVerts == 0 || NIdx == 0 || !Device || !mesh.Emitted || !mesh.Verts || !mesh.Indices ||
      !mesh.Draws) {
    NIdx = 0;
    return true;
  }
  /* A PREVIOUS POSE NOBODY READS IS A SETTING THAT SILENTLY DID NOT APPLY (board:1169), and that half
   * of the rule stands.
   *
   * THE OTHER HALF WAS TOO STRICT AND SAID SO IN ITS OWN WORDS (board:1413). It refused a velocity
   * target over a mesh with no previous pose because *the motion of every pixel would be a sentinel*
   * -- and that is not what happens. A screen-space motion vector has two terms, the vertex moved and
   * the camera moved, and a mesh with no previous pose is a mesh that DID NOT DEFORM: its previous
   * position is its current one, and the camera term is still whatever `PrevMvp16` says. So the
   * velocity over rigid geometry is exact rather than absent, and refusing it kept a temporal resolve
   * off every static subject there is -- which is most of them.
   *
   * `Prev` BINDS THE CURRENT POSITIONS IN THAT CASE, below, so the vertex term is identically zero by
   * arithmetic rather than by a branch in the shader. */
  if (mesh.PrevVerts != nullptr && !WritesVelocity) {
    NIdx = 0;
    error = "the mesh carries a previous pose and the pass attaches no velocity target, so the run "
            "would reach no shader";
    return false;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {
    /* A SURFACE THAT READS THE SECOND UV SET IS OWED THE RUN, AND THE ABSENCE IS A REFUSAL RATHER
     * THAN THE FIRST SET (board:1182). This is the one place both halves are known -- the table says
     * which sockets read it, the mesh and the draw's layout say whether it is bound -- and the
     * alternative is not a missing image but a plausible WRONG one: `MultiUVTest` writes "Multiple
     * UVs not supported in this viewer" exactly where the first set addresses. It is checked here
     * and not in the encoder because a batch dropped at encode time is a body missing from the
     * picture with nothing to attribute it to. */
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

  /* WHAT THE MESH ACTUALLY CARRIES DECIDES, NOT WHAT THE DRAW ASKED FOR, and it is decided here
   * rather than per frame in the encoder (board:1193): a list built against one subject and a mesh
   * set from another would otherwise bind a pipeline whose slot has no buffer behind it. The draw's
   * own layout is the ceiling and the mesh is the floor.
   *
   * THE SECOND UV SET IS NOT DEGRADED AND CANNOT BE (board:1182). Every other run falls back to a
   * layout without it, which costs an image nobody sampled; falling back on that one would keep
   * sampling and move the image, so the refusal above is what answers it. */
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

  /* THE INDEX RUN IS THE TOPOLOGY'S AND CROSSES ONCE (board:1464); the streams are the pose's. */
  {
    const Heap::Tagged uploading("mesh-upload");
    Idx = Fill(SDL_GPU_BUFFERUSAGE_INDEX, mesh.Indices, NIdx * (uint32_t)sizeof(uint32_t));
  }
  if (!Idx) {
    NIdx = 0;
    error = std::string("the subject's index run did not reach the device: ") + SDL_GetError();
    return false;
  }
  if (!HandStreams(mesh, error)) { return false; }

  {
    const Heap::Tagged building("mesh-bvh");
    /* THE VISIBILITY STRUCTURE IS BUILT OVER THE WHOLE INDEX RUN and not per batch, because a shadow
     * is cast by the subject and not by a draw: a body split into thirty primitives shadows itself
     * across every one of those seams, and thirty structures would each be blind to the other
     * twenty-nine. */
    Visibility_ = TriangleBvh::Over(Span<const float>(mesh.Verts, (size_t)NVerts * 3u),
                                    Span<const uint32_t>(mesh.Indices, (size_t)NIdx));
  }
  if (Visibility_.Empty()) {
    NIdx = 0;
    error = "the subject's " + std::to_string(NIdx / 3u) +
            " triangles built no visibility structure, so no light could be occluded by them";
    return false;
  }
  return HandVisibility(error);
}

/* **THE VERTEX STREAMS, WHICH ARE THE POSE'S AND CROSS ON EVERY ONE OF THEM** (board:1464). The index
 * run is not here: it belongs to the topology and went over once.
 *
 * A RIGID MESH IS ITS OWN PREVIOUS POSE, which makes the vertex half of the motion exactly zero and
 * leaves the camera half untouched (board:1413). */
bool SubjectDraw::HandStreams(const SubjectPose &pose, std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const uint32_t positionBytes = NVerts * 3u * (uint32_t)sizeof(float);
  const uint32_t pairBytes = NVerts * 2u * (uint32_t)sizeof(float);
  const uint32_t quadBytes = NVerts * 4u * (uint32_t)sizeof(float);
  const float *const previousPose = pose.PrevVerts != nullptr ? pose.PrevVerts : pose.Verts;
  const auto vertex = SDL_GPU_BUFFERUSAGE_VERTEX;
  /* A RUN THIS LAYOUT DOES NOT CARRY CROSSES AS ZERO BYTES, which `Cross` reads as *let this buffer
   * go*: the flags are the topology's, so it happens at the first pose and never again. */
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
  if (!Cross(streams, sizeof streams / sizeof streams[0], error)) {
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

/* THE STRUCTURE AS THE DEVICE READS IT, plus the one length the shadow ray starts from. It is the
 * same work whether the tree was built or refitted, so it is written once. */
bool SubjectDraw::HandVisibility(std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const auto storage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
  /* A REFIT MOVES THE BOXES AND NOT THEIR COUNT, so these two are sized by the topology exactly as
   * the vertex streams are, and a pose rewrites them in the same pass (board:1463). */
  Crossing structure[] = {
      {&BvhNodes, &Held_[(size_t)Stream::BvhNodes], storage, Visibility_.Nodes().Data(),
       (uint32_t)Visibility_.Nodes().Bytes()},
      {&BvhTris, &Held_[(size_t)Stream::BvhTriangles], storage, Visibility_.Triangles().Data(),
       (uint32_t)Visibility_.Triangles().Bytes()},
  };
  if (!Cross(structure, sizeof structure / sizeof structure[0], error)) {
    NIdx = 0;
    return false;
  }
  if (!BvhNodes || !BvhTris) {
    NIdx = 0;
    error = std::string("the subject's visibility structure did not reach the device: ") +
            SDL_GetError();
    return false;
  }
  /* THE RAY'S START, IN THE SUBJECT'S OWN METRES: a fixed fraction of the structure's root box,
   * which is the only length scale the subject has. A constant in metres would be four orders too
   * large on a 3 cm part and four too small on a city. */
  const BvhNode &root = Visibility_.Nodes()[0];
  float diagonal = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    const float span = root.MaxM[axis] - root.MinM[axis];
    diagonal += span * span;
  }
  ShadowNearM_ = std::sqrt(diagonal) * kShadowRayNearFraction;
  return true;
}

/* **THE SAME SUBJECT SOMEWHERE ELSE** (board:1464). The topology, the batches, the index buffer and the
 * tree's shape are all still the ones `SetMesh` established; what moved is where the corners are. The
 * vertex streams go over again and the tree is REFITTED, which touches no allocator. */
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
  if (!HandStreams(pose, error)) { return false; }
  {
    const Heap::Tagged refitting("mesh-bvh");
    if (!Visibility_.Refit(Span<const float>(pose.Verts, (size_t)NVerts * 3u))) {
      error = "the subject's visibility structure did not refit to this pose";
      return false;
    }
  }
  return HandVisibility(error);
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
  for (int i = 0; i < 16; i++) { uniform[20 + i] = ctx.PrevMvp16[i]; }
  for (int i = 0; i < 3; i++) { uniform[36 + i] = (float)(PrevAnchor[i] - ctx.PrevEye[i]); }
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
  for (size_t at = 0; at < Batches.size(); ++at) {
    const DrawBatch &batch = Batches[at];
    const SurfaceSlot &surface = Slots[batch.MaterialSlot];
    /* WHICH HALF OF THE SUBJECT THIS PASS DRAWS, SAID ONCE (board:1386). A pass given the scene
     * behind it draws the transmissive slots and nothing else; a pass without one draws everything
     * else. The two are the same unit over the same draw list, and stating the split here rather
     * than in two draw lists is what keeps one batching, one sort key and one layout decision --
     * a second list would be a second place for the order to be got wrong. */
    const bool glassSlot = surface.Kind == SurfaceKind::ThinTransmissive ||
                           surface.Kind == SurfaceKind::Refractive;
    if (glassSlot != (Behind != nullptr)) { continue; }
    /* WHICH LAYOUT THIS BATCH IS DRAWN THROUGH WAS DECIDED IN `SetMesh` (board:1193), where the mesh
     * and the list are both in hand and a combination the table does not carry can be refused by
     * name. What is left here is binding the runs that layout names, in the order `ShapeOf` put
     * them. */
    const VertexLayout wanted = BatchLayout[at];
    const bool textured = CarriesUv(wanted);
    const bool lit = CarriesNormal(wanted);
    const bool mapped = CarriesTangent(wanted);
    const bool secondUv = CarriesUv1(wanted);
    const bool tinted = CarriesColour(wanted);
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
      SDL_GPUBufferBinding runs[VertexShape::kRuns] = {};
      uint32_t count = 0;
      runs[count++] = SDL_GPUBufferBinding{Vtx.Get(), 0};
      if (textured) { runs[count++] = SDL_GPUBufferBinding{Uv.Get(), 0}; }
      if (secondUv) { runs[count++] = SDL_GPUBufferBinding{Uv1.Get(), 0}; }
      runs[count++] = SDL_GPUBufferBinding{lit ? Nrm.Get() : Emit.Get(), 0};
      if (mapped) { runs[count++] = SDL_GPUBufferBinding{Tan.Get(), 0}; }
      if (tinted) { runs[count++] = SDL_GPUBufferBinding{Col.Get(), 0}; }
      /* LAST, WHICH IS WHERE `ShapeOf` PUT IT: the slot indices are the order the shape declared
       * them in, so the previous pose is bound after whatever the layout carries and the two lists
       * are one statement read twice. */
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
          /* THE PASS'S OWN IMAGE IN THE SLOT AFTER THE MATERIAL'S SIX. On the opaque pass there is no
           * background and the slot takes the colour image again -- a binding the arms of that pass
           * never sample, which is the same shape the unit's own comment already defends for an
           * untextured arm that still declares its images. Leaving it unbound is not an option: the
           * contract every entry point compiles against declares seven. */
          {Behind != nullptr ? Behind : surface.Colour.Image.Get(),
           BehindSampler != nullptr ? BehindSampler : surface.Colour.Sample.Get()}};
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
