#include "SubjectDraw.h"

#include <cmath>
#include <string>

#include "MetalRoughBrdf.h"
#include "SceneTargets.h"
#include "SurfaceState.h"

namespace outshine::Render {

namespace {

/* WHICH SCREEN-SPACE ORIENTATION glTF's FRONT FACE ARRIVES IN. MEASURED, not derived: the chain is
 * two flips and not one -- clip-space +Y is up while the framebuffer's runs down, and the eye basis
 * puts the view along -Z so the projection's own w is -z_eye -- and they cancel, leaving glTF's
 * counter-clockwise front face counter-clockwise on the target. The derivation that counted one flip
 * was written here first and culled every pixel of `render/coverage/quad`. */
constexpr wgpu::FrontFace kGltfFrontFace = wgpu::FrontFace::CCW;

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

wgpu::AddressMode AddressOf(SubjectWrap wrap) {
  switch (wrap) {
    case SubjectWrap::ClampToEdge: return wgpu::AddressMode::ClampToEdge;
    case SubjectWrap::MirroredRepeat: return wgpu::AddressMode::MirrorRepeat;
    case SubjectWrap::Repeat: return wgpu::AddressMode::Repeat;
  }
  return wgpu::AddressMode::Repeat;
}

wgpu::FilterMode FilterOf(SubjectFilter filter) {
  return filter == SubjectFilter::Nearest ? wgpu::FilterMode::Nearest : wgpu::FilterMode::Linear;
}

/* SOURCE-OVER WITH STRAIGHT ALPHA, which is the one convention both sides of the comparison are
 * stated in. The alpha channel accumulates coverage the same way -- `a + dst*(1-a)` -- so a stack of
 * blended surfaces reports how much of the pixel they cover between them rather than only the last
 * one's contribution. */
wgpu::BlendState OverBlend() {
  wgpu::BlendState blend{};
  blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
  blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.color.operation = wgpu::BlendOperation::Add;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;
  blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.alpha.operation = wgpu::BlendOperation::Add;
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
static const char *kSubjectWGSL = R"(
struct S { mvp : mat4x4f, anc : vec4f };
@group(0) @binding(0) var<uniform> s : S;
/* THE SLOT'S SURFACE ROW. `factor` is glTF's `baseColorFactor.a` and `cut` its `alphaCutoff`, which
 * both arms read; the rest is the metal-rough row only the lit arm reads. */
struct M { factor : f32, cut : f32, metalness : f32, roughness : f32,
           base : vec4f, emissive : vec3f, normalScale : f32 };
@group(0) @binding(3) var<uniform> surface : M;

struct SOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f,
              @location(1) @interpolate(flat) emitted : vec3f };

@vertex fn vs(@location(0) p : vec3f, @location(2) emitted : vec3f) -> SOut {
  var o : SOut;
  o.pos = s.mvp * vec4f(p + s.anc.xyz, 1.0);
  o.uv = vec2f(0.0);
  o.emitted = emitted;
  return o;
}

@vertex fn vsTextured(@location(0) p : vec3f, @location(1) uv : vec2f,
                      @location(2) emitted : vec3f) -> SOut {
  var o : SOut;
  o.pos = s.mvp * vec4f(p + s.anc.xyz, 1.0);
  o.uv = uv;
  o.emitted = emitted;
  return o;
}

struct SFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
/* The declared radiance, not shaded. On an opaque arm the fourth channel is the direct fraction a
 * display transfer weights its occlusion by, and for a surface that emits what it was declared to
 * emit all of it is direct. */
@fragment fn fs(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsMasked(in : SOut) -> SFrag {
  if (surface.factor < surface.cut) { discard; }
  var o : SFrag;
  o.col = vec4f(in.emitted, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsBlended(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted, surface.factor);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

static const char *kSubjectTexturedWGSL = R"(
@group(0) @binding(1) var colourMap : texture_2d<f32>;
@group(0) @binding(2) var colourSampler : sampler;

@fragment fn fsTextured(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted * textureSample(colourMap, colourSampler, in.uv).rgb, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

/* Greater-or-equal is kept, less-than discards: glTF states the surviving side, not the cut side
 * ("If the alpha value is greater than or equal to the alphaCutoff value then it is rendered as
 * fully opaque"), and the two differ on exactly the texels a linear ramp puts at the cutoff. */
@fragment fn fsMaskedTextured(in : SOut) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  if (surface.factor * tap.a < surface.cut) { discard; }
  var o : SFrag;
  o.col = vec4f(in.emitted * tap.rgb, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

/* STRAIGHT, NOT PREMULTIPLIED: the colour goes out unweighted and the blend state applies the
 * weight, so the same fragment function would be correct under either convention only by accident.
 * The one convention this whole comparison is stated in is straight alpha. */
@fragment fn fsBlendedTextured(in : SOut) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  var o : SFrag;
  o.col = vec4f(in.emitted * tap.rgb, surface.factor * tap.a);
  o.vel = vec2f(kVelStatic);
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
 * NO VISIBILITY TERM. Nothing here traces a shadow, so every light reaches every facet it faces.
 * That is stated in the header as this unit's limit; a surface that occludes another is lit through
 * it and the picture shows it. */
static const char *kSubjectLitWGSL = R"(
/* `tint` is colour times intensity with the kind in `w` -- 0 directional, 1 point, 2 spot -- so the
 * multiplier and the shape travel together and a light cannot be half-declared. `place` is the
 * camera-relative position with the RECIPROCAL of the declared range in `w` -- zero where the file
 * declares none, so the range window costs a multiply and "no cutoff" is the same expression rather
 * than a branch. `beam` is the unit direction the light points. `cone` carries cos(outerConeAngle)
 * and the reciprocal of `cos(inner) - cos(outer)`, precomputed because the shader would otherwise
 * divide by it per fragment per light. */

struct Light { tint : vec4f, place : vec4f, beam : vec4f, cone : vec4f };
struct Lights { count : vec4f, items : array<Light, 16> };
@group(0) @binding(4) var<uniform> lights : Lights;

struct LOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f,
              @location(1) n : vec3f, @location(2) p : vec3f };

@vertex fn vsLit(@location(0) p : vec3f, @location(3) n : vec3f) -> LOut {
  var o : LOut;
  let placed = p + s.anc.xyz;
  o.pos = s.mvp * vec4f(placed, 1.0);
  o.uv = vec2f(0.0);
  o.n = n;
  o.p = placed;
  return o;
}

@vertex fn vsLitTextured(@location(0) p : vec3f, @location(1) uv : vec2f,
                         @location(3) n : vec3f) -> LOut {
  var o : LOut;
  let placed = p + s.anc.xyz;
  o.pos = s.mvp * vec4f(placed, 1.0);
  o.uv = uv;
  o.n = n;
  o.p = placed;
  return o;
}

/* THE ROUGHNESS AND THE METALNESS ARE ARGUMENTS AND NOT READS OF THE ROW, because a normal-mapped
 * surface states both per texel and the untextured arm states both per material -- one function
 * that read the row would force the mapped arm to write the loop over the lights a second time. */
fn shadeRow(n : vec3f, p : vec3f, albedo : vec3f, metalness : f32, roughness : f32) -> vec3f {
  let v = normalize(-p);
  let a = roughness * roughness;
  let a2 = a * a;
  let diffuseColour = albedo * (1.0 - metalness);
  let f0 = mix(vec3f(kDielectricF0), albedo, metalness);
  let nv = max(dot(n, v), 1.0e-6);
  var sum = vec3f(0.0);
  let count = i32(lights.count.x);
  for (var at = 0; at < count; at = at + 1) {
    let light = lights.items[at];
    var toward = -light.beam.xyz;
    var attenuation = 1.0;
    if (light.tint.w > 0.5) {
      let offset = light.place.xyz - p;
      let square = dot(offset, offset);
      if (square <= 0.0) { continue; }
      toward = offset * inverseSqrt(square);
      /* THE INVERSE-SQUARE LAW, WINDOWED BY THE DECLARED RANGE. `intensity` is candela, so a facet
       * facing the light receives `intensity / d^2`; the window is `KHR_lights_punctual`'s own
       * recommended function, `max(min(1 - (d/range)^4, 1), 0)`, and it is applied rather than
       * dropped because the corpus asset that declares a range depends on it -- six panels 2.25 m
       * apart, eight scene-wide lights, and a range of exactly half the spacing is what makes each
       * panel see only its own. `place.w` is 1/range, zero where the file declared none, and then
       * the fourth power is zero and the window is 1. */
      let reach = square * light.place.w * light.place.w;
      attenuation = clamp(1.0 - reach * reach, 0.0, 1.0) / square;
    }
    if (light.tint.w > 1.5) {
      attenuation = attenuation *
                    clamp((dot(light.beam.xyz, -toward) - light.cone.x) * light.cone.y, 0.0, 1.0);
    }
    let nl = dot(n, toward);
    if (nl <= 0.0 || attenuation <= 0.0) { continue; }
    let h = normalize(toward + v);
    let reflected = metalRoughBrdf(diffuseColour, f0, a2, nl, nv,
                                   max(dot(n, h), 0.0), max(dot(v, h), 0.0));
    sum = sum + (reflected.diffuse + reflected.specular) * nl * attenuation * light.tint.rgb;
  }
  return sum + surface.emissive;
}

fn shade(n : vec3f, p : vec3f, albedo : vec3f) -> vec3f {
  return shadeRow(n, p, albedo, surface.metalness, surface.roughness);
}

/* A DOUBLE-SIDED FACET HIT FROM BEHIND IS LIT BY ITS OTHER FACE, which is what the flip is: the
 * file's normal describes the front, and a back-facing fragment only exists at all because the
 * material said the surface has two sides. Without it such a facet faces away from every light and
 * comes back black -- and glTF's own `doubleSided` assets are exactly the ones that show it. */
fn facing(n : vec3f, front : bool) -> vec3f {
  return select(-normalize(n), normalize(n), front);
}

@fragment fn fsLit(in : LOut, @builtin(front_facing) front : bool) -> SFrag {
  var o : SFrag;
  o.col = vec4f(shade(facing(in.n, front), in.p, surface.base.rgb), 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsLitMasked(in : LOut, @builtin(front_facing) front : bool) -> SFrag {
  if (surface.factor < surface.cut) { discard; }
  var o : SFrag;
  o.col = vec4f(shade(facing(in.n, front), in.p, surface.base.rgb), 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsLitBlended(in : LOut, @builtin(front_facing) front : bool) -> SFrag {
  var o : SFrag;
  o.col = vec4f(shade(facing(in.n, front), in.p, surface.base.rgb), surface.factor);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

/* THE LIT TEXTURED ARMS, split out for the same reason the emitted ones are: the sampler and its
 * image are a binding the plain pipelines do not declare, and one shader that read a white stand-in
 * would make "no texture" and "a white texture" the same picture. The sampled colour is the BASE
 * COLOUR the BRDF is evaluated with -- not a factor applied afterwards -- because glTF's base colour
 * feeds both halves of the model and multiplying a shaded result by it would tint the specular
 * lobe of a dielectric, which is a metal's behaviour. */
static const char *kSubjectLitTexturedWGSL = R"(
@fragment fn fsLitTextured(in : LOut, @builtin(front_facing) front : bool) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  var o : SFrag;
  o.col = vec4f(shade(facing(in.n, front), in.p, surface.base.rgb * tap.rgb), 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsLitMaskedTextured(in : LOut, @builtin(front_facing) front : bool) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  if (surface.factor * tap.a < surface.cut) { discard; }
  var o : SFrag;
  o.col = vec4f(shade(facing(in.n, front), in.p, surface.base.rgb * tap.rgb), 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsLitBlendedTextured(in : LOut, @builtin(front_facing) front : bool) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  var o : SFrag;
  o.col = vec4f(shade(facing(in.n, front), in.p, surface.base.rgb * tap.rgb),
                surface.factor * tap.a);
  o.vel = vec2f(kVelStatic);
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
 * the frame, which is what `select` does to all three below.
 *
 * ROUGHNESS AND METALNESS COME FROM THE FILE'S OWN IMAGE where it declares one, green and blue,
 * multiplied by the factors -- which is glTF's rule (Specification.adoc:1394) and is not an extra:
 * the alternative is the default factor of 1, and at roughness 1 the GGX distribution is the
 * constant 1/pi, so the lobe stops depending on the normal and a normal-mapped picture comes back
 * flat. */
static const char *kSubjectMappedWGSL = R"(
@group(0) @binding(5) var normalMap : texture_2d<f32>;
@group(0) @binding(6) var normalSampler : sampler;
@group(0) @binding(7) var metalRoughMap : texture_2d<f32>;
@group(0) @binding(8) var metalRoughSampler : sampler;

struct MOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f,
              @location(1) n : vec3f, @location(2) p : vec3f, @location(3) t : vec4f };

@vertex fn vsMapped(@location(0) p : vec3f, @location(1) uv : vec2f,
                    @location(3) n : vec3f, @location(4) t : vec4f) -> MOut {
  var o : MOut;
  let placed = p + s.anc.xyz;
  o.pos = s.mvp * vec4f(placed, 1.0);
  o.uv = uv;
  o.n = n;
  o.p = placed;
  o.t = t;
  return o;
}

fn mappedNormal(in : MOut, front : bool) -> vec3f {
  let facing = select(-1.0, 1.0, front);
  let n = normalize(in.n) * facing;
  let raw = in.t.xyz * facing;
  let t = normalize(raw - n * dot(n, raw));
  let b = cross(n, t) * in.t.w;
  let tap = textureSample(normalMap, normalSampler, in.uv).xyz * 2.0 - 1.0;
  let scaled = vec3f(tap.xy * surface.normalScale, tap.z);
  return normalize(t * scaled.x + b * scaled.y + n * scaled.z);
}

/* The metal-rough row the mapped arm shades with: the surface's own factors times the file's image.
 * A slot that declares no image binds one white texel, and white is the multiplicative identity
 * here, so "no image" and "the factors alone" are the same statement rather than two arms. */
fn mappedShade(in : MOut, front : bool) -> vec3f {
  let orm = textureSample(metalRoughMap, metalRoughSampler, in.uv);
  let albedo = surface.base.rgb * textureSample(colourMap, colourSampler, in.uv).rgb;
  return shadeRow(mappedNormal(in, front), in.p, albedo,
                  surface.metalness * orm.b, surface.roughness * orm.g);
}

@fragment fn fsMapped(in : MOut, @builtin(front_facing) front : bool) -> SFrag {
  var o : SFrag;
  o.col = vec4f(mappedShade(in, front), 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsMappedMasked(in : MOut, @builtin(front_facing) front : bool) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  if (surface.factor * tap.a < surface.cut) { discard; }
  var o : SFrag;
  o.col = vec4f(mappedShade(in, front), 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsMappedBlended(in : MOut, @builtin(front_facing) front : bool) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  var o : SFrag;
  o.col = vec4f(mappedShade(in, front), surface.factor * tap.a);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

void SubjectDraw::Configure(const Gpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  FiltersFloat32 = gpu.FiltersFloat32;

  const std::string src = std::string(kVelocityWGSL) + kSubjectWGSL + kSubjectTexturedWGSL +
                          MetalRoughBrdfWGSL() + kSubjectLitWGSL + kSubjectLitTexturedWGSL +
                          kSubjectMappedWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute position{};
  position.format = wgpu::VertexFormat::Float32x3;
  position.offset = 0;
  position.shaderLocation = 0;
  wgpu::VertexAttribute coordinate{};
  coordinate.format = wgpu::VertexFormat::Float32x2;
  coordinate.offset = 0;
  coordinate.shaderLocation = 1;
  wgpu::VertexAttribute radiance{};
  radiance.format = wgpu::VertexFormat::Float32x3;
  radiance.offset = 0;
  radiance.shaderLocation = 2;
  wgpu::VertexAttribute normal{};
  normal.format = wgpu::VertexFormat::Float32x3;
  normal.offset = 0;
  normal.shaderLocation = 3;
  wgpu::VertexAttribute tangent{};
  tangent.format = wgpu::VertexFormat::Float32x4;
  tangent.offset = 0;
  tangent.shaderLocation = 4;
  /* ONE BUFFER PER ATTRIBUTE AND NOT ONE INTERLEAVED STRIDE, because the subject's positions, its
   * uvs and its declared radiance come out of the consumer as separate runs and interleaving them
   * here would be a copy nobody asked for. The untextured pipeline has no uv slot at all rather than
   * an empty one -- a declared slot must be bound, and binding a buffer nothing reads is the kind of
   * placeholder this unit refuses everywhere else. */
  wgpu::VertexBufferLayout position_[1] = {};
  position_[0].arrayStride = 3 * sizeof(float);
  position_[0].attributeCount = 1;
  position_[0].attributes = &position;
  wgpu::VertexBufferLayout radiance_ = {};
  radiance_.arrayStride = 3 * sizeof(float);
  radiance_.attributeCount = 1;
  radiance_.attributes = &radiance;
  wgpu::VertexBufferLayout uv_ = {};
  uv_.arrayStride = 2 * sizeof(float);
  uv_.attributeCount = 1;
  uv_.attributes = &coordinate;
  wgpu::VertexBufferLayout normal_ = {};
  normal_.arrayStride = 3 * sizeof(float);
  normal_.attributeCount = 1;
  normal_.attributes = &normal;
  /* THE LIT ARMS BIND NO RADIANCE RUN AT ALL, which is the vertex-buffer half of what the two arms
   * are: a lit surface's colour comes from its own row and the light list, and a per-vertex radiance
   * beside it would be a second answer to the same question. */
  wgpu::VertexBufferLayout tangent_ = {};
  tangent_.arrayStride = 4 * sizeof(float);
  tangent_.attributeCount = 1;
  tangent_.attributes = &tangent;
  const wgpu::VertexBufferLayout plainLayouts[2] = {position_[0], radiance_};
  const wgpu::VertexBufferLayout texturedLayouts[3] = {position_[0], uv_, radiance_};
  const wgpu::VertexBufferLayout litLayouts[2] = {position_[0], normal_};
  const wgpu::VertexBufferLayout litTexturedLayouts[3] = {position_[0], uv_, normal_};
  const wgpu::VertexBufferLayout mappedLayouts[4] = {position_[0], uv_, normal_, tangent_};

  /* THE BIND GROUP LAYOUT IS WRITTEN DOWN AND NOT DERIVED, because every pipeline must share ONE:
   * an auto-derived layout reflects the entry point's own uses, so the untextured shader would get
   * a two-entry layout and the bind group built for the others would be rejected against it. */
  wgpu::BindGroupLayoutEntry bindings[9] = {};
  bindings[0].binding = 0;
  bindings[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  bindings[0].buffer.type = wgpu::BufferBindingType::Uniform;
  bindings[0].buffer.minBindingSize = kUniFloats * sizeof(float);
  bindings[1].binding = 1;
  bindings[1].visibility = wgpu::ShaderStage::Fragment;
  bindings[1].texture.sampleType = wgpu::TextureSampleType::Float;
  bindings[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  bindings[2].binding = 2;
  bindings[2].visibility = wgpu::ShaderStage::Fragment;
  bindings[2].sampler.type = wgpu::SamplerBindingType::Filtering;
  bindings[3].binding = 3;
  bindings[3].visibility = wgpu::ShaderStage::Fragment;
  bindings[3].buffer.type = wgpu::BufferBindingType::Uniform;
  bindings[3].buffer.minBindingSize = kSurfaceFloats * sizeof(float);
  /* THE LIGHT LIST IS ONE BUFFER FOR THE WHOLE SUBJECT and it sits in the per-slot bind group only
   * because there is one group: it is the same buffer in every slot's group, so no surface can be
   * lit by a list another surface does not see. */
  bindings[4].binding = 4;
  bindings[4].visibility = wgpu::ShaderStage::Fragment;
  bindings[4].buffer.type = wgpu::BufferBindingType::Uniform;
  bindings[4].buffer.minBindingSize = kLightFloats * sizeof(float);
  /* THE OTHER TWO IMAGES OF ONE SURFACE. They sit in the same group as the colour because a surface
   * is one thing: three groups would let a draw bind a normal map from one material over a base
   * colour from another, and there is no draw that wants that. */
  for (uint32_t at = 0; at < 2; ++at) {
    wgpu::BindGroupLayoutEntry &image = bindings[5 + at * 2];
    image.binding = 5 + at * 2;
    image.visibility = wgpu::ShaderStage::Fragment;
    image.texture.sampleType = wgpu::TextureSampleType::Float;
    image.texture.viewDimension = wgpu::TextureViewDimension::e2D;
    wgpu::BindGroupLayoutEntry &sampler = bindings[6 + at * 2];
    sampler.binding = 6 + at * 2;
    sampler.visibility = wgpu::ShaderStage::Fragment;
    sampler.sampler.type = wgpu::SamplerBindingType::Filtering;
  }
  wgpu::BindGroupLayoutDescriptor bgl{};
  bgl.entryCount = 9;
  bgl.entries = bindings;
  Layout = Device.CreateBindGroupLayout(&bgl);
  wgpu::PipelineLayoutDescriptor pld{};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &Layout;
  const wgpu::PipelineLayout pipeline = Device.CreatePipelineLayout(&pld);

  wgpu::RenderPipelineDescriptor rp{};
  rp.layout = pipeline;
  rp.vertex.module = m;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 2;
  rp.fragment = &fs;
  rp.primitive.frontFace = kGltfFrontFace;

  /* THIRTY PIPELINES: five vertex layouts, two facings, three alpha modes. The facing is the SLOT's
   * because glTF states it per material -- `TextureSettingsTest` hides a green checkmark behind a
   * polygon facing the wrong way and lets the flag decide which of the two is seen, and one cull
   * mode for the whole subject draws the wrong cell whichever way it is set. */
  for (const SurfaceKind kind : {SurfaceKind::Opaque, SurfaceKind::Masked, SurfaceKind::Blended}) {
    const bool blends = kind == SurfaceKind::Blended;
    wgpu::ColorTargetState ct{};
    ct.format = gpu.HdrFormat;
    const wgpu::BlendState over = OverBlend();
    if (blends) { ct.blend = &over; }
    /* A BLENDED SURFACE WRITES NO VELOCITY, and that follows from its writing no depth rather than
     * being a second decision: the temporal resolve reprojects a pixel through the depth that was
     * written there, so a surface that left none has no motion to claim and would overwrite the
     * motion of whatever did. */
    wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(!blends)};
    wgpu::DepthStencilState ds{};
    ds.format = wgpu::TextureFormat::Depth32Float;
    ds.depthCompare = wgpu::CompareFunction::Greater;
    /* `MASK` writes depth like any solid surface -- Khronos says so in the asset's own words,
     * "Depth buffering with writes enabled may be used in MASK mode" -- because a kept fragment is
     * fully opaque. `BLEND` cannot: it composites with what is behind it, so writing the depth that
     * would hide that is the one thing it must not do (`core/SurfaceState.h` states the same pair). */
    ds.depthWriteEnabled = !blends;
    rp.depthStencil = &ds;
    fs.targets = cts;

    for (const VertexLayout layout : {VertexLayout::Position, VertexLayout::PositionUv,
                                      VertexLayout::PositionNormal,
                                      VertexLayout::PositionNormalUv,
                                      VertexLayout::PositionNormalUvTangent}) {
      const bool textured = CarriesUv(layout);
      const bool lit = CarriesNormal(layout);
      const bool mapped = CarriesTangent(layout);
      rp.vertex.entryPoint = VertexEntryPoint(layout);
      rp.vertex.bufferCount = mapped ? 4u : (textured ? 3u : 2u);
      rp.vertex.buffers = mapped ? mappedLayouts
                                 : (lit ? (textured ? litTexturedLayouts : litLayouts)
                                        : (textured ? texturedLayouts : plainLayouts));
      fs.entryPoint = FragmentEntryPoint(kind, layout);
      for (const bool cullsBack : {false, true}) {
        rp.primitive.cullMode = cullsBack ? wgpu::CullMode::Back : wgpu::CullMode::None;
        Pipelines[PipelineAt(layout, kind, cullsBack)] = Device.CreateRenderPipeline(&rp);
      }
    }
  }

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  /* THE LIGHT BUFFER EXISTS BEFORE ANY LIGHT DOES, because every bind group names it and a subject
   * that is lit by nothing is still a subject with a list -- an empty one, whose count is zero. */
  bd.size = kLightFloats * sizeof(float);
  Lights = Device.CreateBuffer(&bd);
  const std::vector<float> dark((size_t)kLightFloats, 0.0f);
  Queue.WriteBuffer(Lights, 0, dark.data(), dark.size() * sizeof(float));
}

size_t SubjectDraw::PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack) {
  const size_t at = static_cast<size_t>(kind);
  return (static_cast<size_t>(layout) * 2u + (cullsBack ? 1u : 0u)) * kSurfaceKinds + at;
}

/* ONE IMAGE ON THE DEVICE, IN LINEAR f32. One texel of white where the surface declares none, so
 * every slot's bind group is complete -- it is never a stand-in for a missing texture, because the
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
  wgpu::TextureDescriptor td{};
  td.size = {width, height, 1};
  td.format = wgpu::TextureFormat::RGBA32Float;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  bound.Image = Device.CreateTexture(&td);
  wgpu::TexelCopyTextureInfo destination{};
  destination.texture = bound.Image;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = width * 4u * sizeof(float);
  layout.rowsPerImage = height;
  wgpu::Extent3D extent{width, height, 1};
  Queue.WriteTexture(&destination, linear.data(), linear.size() * sizeof(float), &layout, &extent);

  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = AddressOf(texture.WrapU);
  sd.addressModeV = AddressOf(texture.WrapV);
  sd.magFilter = FilterOf(texture.Magnify);
  sd.minFilter = FilterOf(texture.Magnify);
  bound.Sample = Device.CreateSampler(&sd);
  bound.View = bound.Image.CreateView();
  return bound;
}

void SubjectDraw::BindSurface(const SubjectMaterial &material) {
  SurfaceSlot slot;
  slot.Kind = material.State().Kind();
  slot.CullsBack = CullsBackFaces(material.State(), kSubjectWinding);
  slot.Colour = Upload(material.Colour, Transfer::Srgb);
  slot.Normal = Upload(material.Normal, Transfer::Linear);
  slot.MetalRough = Upload(material.MetalRough, Transfer::Linear);

  const Material &row = material.Row;
  const float surface[kSurfaceFloats] = {
      material.Coverage(), material.State().CoverageCut(),
      row.Metalness,       row.Roughness,
      row.BaseColour[0],   row.BaseColour[1], row.BaseColour[2], row.BaseColour[3],
      row.Emission[0],     row.Emission[1],   row.Emission[2],   material.NormalScale};
  wgpu::BufferDescriptor ad{};
  ad.size = sizeof surface;
  ad.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  slot.Surface = Device.CreateBuffer(&ad);
  Queue.WriteBuffer(slot.Surface, 0, surface, sizeof surface);

  wgpu::BindGroupEntry entries[9] = {};
  entries[0].binding = 0;
  entries[0].buffer = Uni;
  entries[0].size = kUniFloats * sizeof(float);
  entries[1].binding = 1;
  entries[1].textureView = slot.Colour.View;
  entries[2].binding = 2;
  entries[2].sampler = slot.Colour.Sample;
  entries[3].binding = 3;
  entries[3].buffer = slot.Surface;
  entries[3].size = sizeof surface;
  entries[4].binding = 4;
  entries[4].buffer = Lights;
  entries[4].size = kLightFloats * sizeof(float);
  entries[5].binding = 5;
  entries[5].textureView = slot.Normal.View;
  entries[6].binding = 6;
  entries[6].sampler = slot.Normal.Sample;
  entries[7].binding = 7;
  entries[7].textureView = slot.MetalRough.View;
  entries[8].binding = 8;
  entries[8].sampler = slot.MetalRough.Sample;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Layout;
  bg.entryCount = 9;
  bg.entries = entries;
  slot.Bind = Device.CreateBindGroup(&bg);

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

  wgpu::BufferDescriptor vd{};
  vd.size = (uint64_t)NVerts * 3 * sizeof(float);
  vd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  Vtx = Device.CreateBuffer(&vd);
  Queue.WriteBuffer(Vtx, 0, mesh.Verts, (size_t)NVerts * 3 * sizeof(float));

  Emit = Device.CreateBuffer(&vd);
  Queue.WriteBuffer(Emit, 0, mesh.Emitted, (size_t)NVerts * 3 * sizeof(float));

  if (HasNormal) {
    Nrm = Device.CreateBuffer(&vd);
    Queue.WriteBuffer(Nrm, 0, mesh.Normals, (size_t)NVerts * 3 * sizeof(float));
  } else {
    Nrm = wgpu::Buffer();
  }

  if (HasTangent) {
    wgpu::BufferDescriptor td{};
    td.size = (uint64_t)NVerts * 4 * sizeof(float);
    td.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Tan = Device.CreateBuffer(&td);
    Queue.WriteBuffer(Tan, 0, mesh.Tangents, (size_t)NVerts * 4 * sizeof(float));
  } else {
    Tan = wgpu::Buffer();
  }

  if (HasUv) {
    wgpu::BufferDescriptor ud{};
    ud.size = (uint64_t)NVerts * 2 * sizeof(float);
    ud.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Uv = Device.CreateBuffer(&ud);
    Queue.WriteBuffer(Uv, 0, mesh.Uv, (size_t)NVerts * 2 * sizeof(float));
  } else {
    Uv = wgpu::Buffer();
  }

  /* WriteBuffer copies in 4-byte units, so an odd index count is padded to keep the queue's own
   * alignment rule -- the draws still submit exactly what their batches say. */
  const uint64_t indexBytes = ((uint64_t)NIdx * sizeof(uint32_t) + 3u) & ~uint64_t{3};
  wgpu::BufferDescriptor id{};
  id.size = indexBytes;
  id.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
  Idx = Device.CreateBuffer(&id);
  Queue.WriteBuffer(Idx, 0, mesh.Indices, (size_t)NIdx * sizeof(uint32_t));
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
void SubjectDraw::WriteLights(const FrameContext &ctx) {
  float packed[kLightFloats] = {};
  packed[0] = (float)Placed.size();
  for (size_t at = 0; at < Placed.size(); ++at) {
    const PunctualLight &light = Placed[at].Light;
    float *entry = packed + 4 + at * 4u * (size_t)kLightVec4s;
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
  Queue.WriteBuffer(Lights, 0, packed, sizeof packed);
}

uint32_t SubjectDraw::DrawCount() const {
  uint32_t drawn = 0;
  for (const DrawBatch &batch : Batches) { drawn += batch.Draws; }
  return drawn;
}

void SubjectDraw::Encode(const FrameContext &ctx, ClusterCut &, wgpu::RenderPassEncoder &pass) {
  if (NIdx == 0 || Batches.empty() || !Vtx || !Idx || !Emit) { return; }
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  for (int i = 0; i < 3; i++) u[16 + i] = (float)(Anchor[i] - ctx.Eye[i]);
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
  WriteLights(ctx);
  pass.SetVertexBuffer(0, Vtx);
  pass.SetIndexBuffer(Idx, wgpu::IndexFormat::Uint32);

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
      pass.SetPipeline(Pipelines[wantedPipeline]);
      /* The two layouts put the radiance in different slots, so every slot the incoming layout
       * declares is rebound: leaving the other one's buffer where it was is how a uv slot ends up
       * holding radiance. */
      if (textured) { pass.SetVertexBuffer(1, Uv); }
      pass.SetVertexBuffer(textured ? 2u : 1u, lit ? Nrm : Emit);
      if (mapped) { pass.SetVertexBuffer(3, Tan); }
      bound = wantedPipeline;
    }
    if (!slotBound || boundSlot != batch.MaterialSlot) {
      pass.SetBindGroup(0, surface.Bind);
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    pass.DrawIndexed(batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

} // namespace outshine::Render
