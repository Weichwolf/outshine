/* THE DECLARED SUBJECT OF A STUDIO: a DRAW LIST over one indexed mesh -- many primitives, each with
 * its own material, its own vertex layout and its own place in the order, drawn under one stage.
 *
 * ONE DRAW PER STAGE WAS THE SHAPE THAT BROKE. `SciFiHelmet` puts several primitives under one mesh
 * with different materials; `ABeautifulGame` has thirty-odd meshes; `NodePerformanceTest` has ten
 * thousand. A unit that draws one subject with one material cannot show any of them, and building
 * them one at a time would rediscover the same missing list per asset. The list is `render/draw/`,
 * it has no device type in it, and the stage row above stays a PASS declaration.
 *
 * TWO WAYS A SURFACE GETS ITS COLOUR AND THEY ARE DIFFERENT PIPELINES, not a branch.
 *
 * THE EMITTED ARM CARRIES THE RADIANCE THE DECLARATION DERIVED. Whether it came from a Lambertian
 * facet under a uniform environment (`rho*L`) or from an emissive surface (the colour itself) is a
 * property of the scene that was declared, and the two reduce to one number per surface; deriving it
 * here would put half a lighting model in a unit that has none. THAT ARM READS NO DIRECTION AT ALL,
 * and it is still the correct one under a uniform environment: irradiance is isotropic there, so a
 * Lambertian facet returns `rho*L` whichever way it faces and an `N.L` term would disagree with the
 * oracle on every face but one -- measured, 97 466 pixels across three normals, 2 ulps of f32.
 *
 * THE LIT ARM IS WHERE `N.L` BECOMES CORRECT, and a punctual light is what makes it so: a light with
 * no area delivers its irradiance from ONE direction, so the cosine is the whole of the geometry
 * term and there is no integral left to disagree about. It reads the surface's own row -- base
 * colour, metalness, roughness, emissive -- and the light list, and it evaluates glTF's own
 * metal-rough BRDF over it, because that is the model the Khronos corpus states its criteria in.
 *
 * THE LIT ARM COMPUTES VISIBILITY WITH AN EXACT RAY and the emitted arm has none to compute. Every
 * light that survives the cosine and the falloff is traced against the subject's own triangles
 * through the acceleration structure in `core/TriangleBvh.h`, so a body that occludes itself is
 * dark where it does. IT IS NOT SOMETHING A CALLER CAN SWITCH OFF: a shadow is not a quality
 * setting here, it is the difference between the light reaching a facet and not, and an arm that
 * could skip it would be a second picture of the same declaration.
 *
 * A UV IS NOT INVENTED EITHER. A primitive with no `TEXCOORD_0` is drawn by a different pipeline,
 * not by this one with a zero coordinate: a zero uv samples the image's corner and looks like a
 * texture that was authored flat. THE SECOND UV SET IS NOT SUBSTITUTED EITHER (board:1182): a
 * surface whose reference names it over a mesh that carries no run for it is a refusal in `SetMesh`,
 * and reading the first set in its place would be a plausible picture of the wrong thing.
 *
 * THE VERTEX COLOUR IS A MULTIPLIER ON BASE COLOUR AND REACHES BOTH ARMS (board:1193). glTF's
 * `COLOR_0` is "an additional linear multiplier to base color", so it enters the lit arm where the
 * albedo enters the BRDF -- multiplying a shaded result by it would tint a dielectric's specular
 * lobe, which is a metal's behaviour -- and the emitted arm where the declared radiance is, which is
 * what that arm's base colour is. ITS ALPHA MULTIPLIES BASE COLOUR'S, so it reaches `alphaMode` too:
 * a `MASK` surface cuts on the product and a `BLEND` one composites with it.
 *
 * A LAYOUT WITHOUT THE RUN IS NOT A SECOND SET OF FRAGMENT ARMS. Its vertex arm writes the
 * multiplicative identity, which is the same statement as the one white texel a surface with no
 * image binds. THAT IS A TRADE AND NOT A FREE ONE: every fragment then interpolates and multiplies a
 * varying, including the ones drawing a subject that declares no vertex colour at all, and the
 * alternative -- a second set of eighteen fragment entry points that do not read it -- is what
 * buying it back would cost. `test/frame/` prices it per arm on every run.
 *
 * THE WINDING IS TRUSTED, because the format defines one, and WHETHER BACK FACES ARE CULLED IS THE
 * MATERIAL'S: glTF states `doubleSided` per material and an asset uses it to hide one polygon
 * behind another. With culling off entirely, reversing every triangle of a subject moved no pixel of
 * either mask and three claims about winding stood on an instrument that could not see it; with one
 * cull mode for the whole subject, a double-sided surface vanished.
 *
 * ALPHA IS THREE ANSWERS AND THEY ARE THREE PIPELINES. `OPAQUE` ignores the number entirely, `MASK`
 * discards below the material's own cutoff and writes depth like any solid surface, `BLEND`
 * composites in the order the list decided and writes no depth at all. One arm dressed as three --
 * folding alpha into the colour, or cutting where the file said composite -- is exactly the set of
 * failures `AlphaBlendModeTest` was built to show, so the three have no shared spelling here.
 *
 * THE ORDER BLENDED SURFACES ARE DRAWN IN IS THE LIST'S, NOT THIS FILE'S. `DrawKey`'s translucency
 * field already sorts opaque before masked before blended and already complements the depth for a
 * blending surface, so back-to-front falls out of the one ascending sort the list performs and this
 * unit encodes the batches in the order it was handed. A second ordering pass here would be a second
 * opinion about the same question. */
#ifndef SUBJECTDRAW_H
#define SUBJECTDRAW_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "TexelChain.h"
#include "PunctualLight.h"
#include "UvTransform.h"

#include "DrawList.h"
#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

/* ONE PLACED LIGHT AS THE RENDERER TAKES IT: the engine's light (`core/PunctualLight.h`) with its
 * position restated in the frame the renderer works in. The direction and the two cone angles stay
 * in the light itself because they are frame-free once rotated; the POSITION cannot be, because a
 * scene's metres are camera-relative floats and an ECEF position is a double.
 *
 * `PositionEcefM` IS THE ONE FIELD THAT OVERRIDES `Light.Position`, and the light's own position
 * field is left unread here for exactly that reason: a float ECEF metre is a 0.5 m quantum at the
 * Earth's radius, so a light placed through it would sit somewhere else. */
struct SubjectLight {
  outshine::PunctualLight Light;
  double PositionEcefM[3] = {0, 0, 0};
};

/* THE ENVIRONMENT A SUBJECT SITS IN: a constant radiance arriving from every direction, in the same
 * scene-referred units the punctual lights are in (board:1206).
 *
 * ZERO IS THE DEFAULT AND IT IS THE HONEST ONE. A subject nobody placed in an environment gathers
 * nothing from one, which is what every case in the corpus that gathers at all already declares --
 * `world: uniform` at strength 0. So this arriving changes no picture that exists, and a case that
 * wants an environment says what it IS rather than inheriting a renderer's idea of one.
 *
 * IT IS NOT THE ATMOSPHERE'S IRRADIANCE. `Resource::IrradianceBuffer` is derived from the sky chain
 * and is a world scene's answer; this is a declaration, and the two never become one field. */
struct SubjectEnvironment {
  double RadianceLinear[3] = {0, 0, 0};
};

/* HOW MANY LIGHTS ONE SUBJECT MAY BE LIT BY. [SET] 16: the corpus's own most-lit asset declares
 * eight (`PointLightIntensityTest`), and the list is a uniform buffer whose size is fixed at
 * pipeline creation. A seventeenth is a refusal that names the count, never a light silently
 * dropped -- a picture missing one of its lights looks like a shading bug rather than a limit. */
constexpr size_t kMaxSubjectLights = 16;

/* HOW MANY IMAGES ONE SURFACE CARRIES, and it is ONE number because two would be one number twice:
 * the binding contract every fragment declares, the samplers the encoder binds and the uv matrices
 * the row holds (board:1177) are three readings of the same four sockets, and a fifth socket that
 * reached two of the three would be a slot bound with nothing to sample it by. */
constexpr uint32_t kSubjectImages = 6;

/* HOW A BASE-COLOUR TEXTURE IS ADDRESSED, glTF's own two questions and nothing else. The wrap mode
 * and the filter are the FILE's -- `TextureSettingsTest` renders one cell per wrap mode and an
 * engine that collapses two of them shows the wrong picture in exactly two cells -- so they cross
 * here rather than being decided in this unit. */
enum class SubjectWrap { ClampToEdge, MirroredRepeat, Repeat };
enum class SubjectFilter { Nearest, Linear };

/* WHICH LEVELS THE MINIFICATION FILTER MAY READ, glTF's other half of `minFilter`. `None` is a
 * declaration and not an absence: a file naming 9728 or 9729 asks for ONE level, and answering it
 * with a chain is answering a different question. */
enum class SubjectMip { None, Nearest, Linear };

/* THE DECODED COLOUR IMAGE, RGBA8 sRGB-ENCODED, straight alpha, top row first -- the one convention
 * the image boundary states. It is decoded to linear f32 HERE, on the CPU, and uploaded as floats,
 * so the sampler filters exact linear values: hardware sRGB sampling carries about twelve bits of
 * the transfer where the formula carries twenty-four, and that residual was 12 833 differing pixels
 * of `simple-texture`. Decoding after the tap is not the repair -- `TextureLinearInterpolationTest`
 * requires the filter to run on linear values.
 *
 * ITS ALPHA IS NOT sRGB-ENCODED and crosses unchanged, which is glTF's own rule: only the three
 * colour channels carry the transfer. */
struct SubjectTexture {
  const uint8_t *Rgba = nullptr;
  uint32_t Width = 0;
  uint32_t Height = 0;
  SubjectWrap WrapU = SubjectWrap::Repeat;
  SubjectWrap WrapV = SubjectWrap::Repeat;
  SubjectFilter Magnify = SubjectFilter::Linear;
  /* MINIFICATION IS A SEPARATE DECLARATION AND WAS NOT CARRIED AT ALL (board:1134). The sampler took
   * the magnification filter for both, so a file asking for point sampling when its texels outnumber
   * the pixels got a linear blend -- and two corpus subjects ask for exactly that. `Mip` is the second
   * half of the same glTF field and belongs beside it rather than at the sampler as a constant. */
  SubjectFilter Minify = SubjectFilter::Linear;
  SubjectMip Mip = SubjectMip::Linear;
  /* WHERE THIS IMAGE IS READ FROM, per texture reference and never per surface (board:1177). It is
   * `core/UvTransform.h`'s composed matrix, the identity where the file declares no
   * `KHR_texture_transform` -- so the fragment's expression is one multiply-add whatever the file
   * said, and "no transform" is not a second arm. */
  outshine::UvTransform Uv;
  /* WHICH OF THE SUBJECT'S UV SETS THIS REFERENCE READS (board:1182), per texture reference for the
   * same reason the matrix above is: glTF states `texCoord` inside each `textureInfo`, so
   * `MultiUVTest` puts its base colour on the first set and its emissive on the second within ONE
   * material. It is beside the transform because both answer "where is this image read from", and
   * the transform applies to whichever set this names.
   *
   * `Second` IS OWED A RUN AND `SetMesh` REFUSES WITHOUT ONE. A default of `First` here is the
   * absence of a declaration and never a fall-back from `Second`: nothing anywhere quietly turns the
   * second set into the first. */
  outshine::UvSet Set = outshine::UvSet::First;
};

/* ONE SURFACE OF THE SUBJECT: what a draw binds when its key names this slot. The surface state is
 * the material's own (`core/SurfaceState.h`) and decides the pipeline; a texture with no texels is a
 * surface that declares none, which is a different pipeline and not a white stand-in.
 *
 * `Colour` IS WHATEVER IMAGE THE DECLARED RADIANCE IS A FUNCTION OF, and it is deliberately not
 * named for one glTF slot: this unit multiplies a sampled colour into a radiance the declaration
 * derived, and whether that image was the file's `baseColorTexture` or its `emissiveTexture` is the
 * declaration's business. `TextureLinearInterpolationTest` carries its colour in the emissive slot
 * and `[0,0,0,1]` in the base one, so a unit that could only sample base colour would draw it black.
 *
 * `Coverage()` IS THE DECLARED CONSTANT HALF OF ALPHA -- glTF's `baseColorFactor.a` -- and the
 * image's own alpha is the other half, multiplied in the shader. It is read as a separate number
 * from the radiance because `OPAQUE` ignores it entirely (`Specification.adoc:2178`) while `MASK`
 * cuts on it and `BLEND` composites with it: folding it into the colour is the premultiplication
 * `AlphaBlendModeTest` fails an engine for. */
struct SubjectMaterial {
  /* THE MATERIAL ROW ITSELF, and the pipeline state is DERIVED from it rather than stored beside it:
   * two fields would spell a slot whose declared alpha mode and whose pipeline disagree, and the
   * coverage factor kept as its own float was already a second copy of `BaseColour[3]`. */
  Material Row;
  SubjectTexture Colour;
  /* `KHR_materials_specular`'s TWO IMAGES (board:1205), named for the same reason the three below
   * are: a table would spell binding the tint where the strength goes.
   *
   * `SpecularStrength` IS A SCALAR IN THE ALPHA CHANNEL, which the extension states outright, and it
   * multiplies `specularFactor`. `SpecularTint` IS sRGB-ENCODED and multiplies `specularColorFactor`
   * -- so the transfer differs between the two and that is exactly why they are separate fields
   * rather than one array with a flag. */
  SubjectTexture SpecularStrength;
  SubjectTexture SpecularTint;
  /* THE OTHER THREE IMAGES glTF PUTS ON ONE SURFACE, named rather than indexed: the mistake a table
   * of four would spell is binding the normal map where the colour goes, and a name cannot spell it.
   *
   * THE TRANSFER IS A PROPERTY OF WHICH SOCKET AN IMAGE SITS IN, which is why these are separate
   * fields rather than one array with a flag: `Emissive` carries the sRGB transfer exactly as the
   * colour does and the other two do not, so a slot whose normal map was decoded as colour has no
   * spelling here.
   *
   * `Emissive` IS THE FILE'S `emissiveTexture`, and its absence was a WRONG PICTURE rather than a
   * missing feature: glTF multiplies `emissiveFactor` by it, `BoomBox`, `Lantern` and `WaterBottle`
   * all state that factor as `[1, 1, 1]`, and a surface that dropped the image emitted the factor
   * over its whole area -- a body uniformly glowing white where the asset draws one lit panel.
   *
   * `MetalRough` IS THE FILE'S `metallicRoughnessTexture`, green for roughness and blue for
   * metalness. It is here because the alternative is not "one image fewer" but a WRONG PICTURE:
   * glTF's `roughnessFactor` defaults to 1, and at roughness 1 the GGX distribution is exactly
   * 1/pi at every half-vector, so the whole specular lobe stops depending on the normal -- which
   * would make a normal-mapped subject whose file states its roughness in a texture render as if it
   * had no normal map at all. */
  SubjectTexture Normal;
  SubjectTexture MetalRough;
  SubjectTexture Emissive;
  /* glTF's `normalTexture.scale`, which multiplies the sampled tangent-space x and y only
   * (Specification.adoc:1416) -- so 0 is a flat surface and 1 is the map as it was baked. */
  float NormalScale = 1.0f;

  [[nodiscard]] SurfaceState State() const { return StateOf(Row); }
  [[nodiscard]] float Coverage() const { return Row.BaseColour[3]; }
  /* WHETHER ANY OF THIS SURFACE'S FOUR SOCKETS READS THE SECOND UV SET (board:1182). It is asked of
   * the whole surface because the vertex layout is what carries the run and a draw wears one
   * surface: a slot with the emissive on the second set needs the run bound however its base colour
   * reads. It walks the four named sockets rather than a table, for the reason they are named. */
  [[nodiscard]] bool ReadsSecondUv() const {
    return Colour.Set == outshine::UvSet::Second || Normal.Set == outshine::UvSet::Second ||
           MetalRough.Set == outshine::UvSet::Second || Emissive.Set == outshine::UvSet::Second ||
           SpecularStrength.Set == outshine::UvSet::Second ||
           SpecularTint.Set == outshine::UvSet::Second;
  }
  /* WHETHER ANY SOCKET OF THIS SURFACE NEEDS A uv AT ALL (board:1205). The consumer decides whether
   * to carry the uv run, and it used to ask only whether the COLOUR image existed -- so a material
   * whose only image is `KHR_materials_specular`'s took the layout with no uv, entered the arm that
   * cannot sample, and its two textures changed nothing. **`SpecularTest`'s picture was identical to
   * the digit through three separate repairs because of this one predicate.** Asking every socket is
   * what stops the seventh image repeating it. */
  [[nodiscard]] bool ReadsAnyImage() const {
    return Colour.Rgba || Normal.Rgba || MetalRough.Rgba || Emissive.Rgba ||
           SpecularStrength.Rgba || SpecularTint.Rgba;
  }
};

/* THE WHOLE OF WHAT IS DRAWN, as one parameter object rather than seven arguments (`I.23`). The
 * three vertex runs are parallel and cover the same vertices; `Indices` is already in the list's
 * COMPILED order, because that is what makes two draws of one material one call.
 *
 * `Uv` is null exactly when no part of the subject carried `TEXCOORD_0`; where some did and some did
 * not, it covers every vertex and the parts that carried none are drawn by the layout that has no uv
 * slot. `Emitted` is 3 floats per vertex: the scene-referred linear radiance that vertex's surface
 * leaves, which the declaration derived and this unit neither shades nor scales. IT IS PER VERTEX
 * AND NOT PER DRAW so that a subject built from several nodes cannot be drawn in one colour -- a
 * single flat value over touching bodies hides their internal silhouettes, and no rule against it is
 * needed when the array has no shorter spelling. It is carried flat to the fragment, so a face's
 * value is the declared one bit for bit and not an interpolation of three equal numbers. */
struct SubjectMesh {
  const float *Verts = nullptr;      /* 3 floats per vertex, ECEF offsets from `Anchor`, metres */
  const float *Uv = nullptr;         /* 2 floats per vertex, or null */
  /* THE SECOND UV SET, 2 floats per vertex, or null (board:1182). It is a run of its own and never a
   * second reading of `Uv`: `MultiUVTest` declares both on one primitive out of two buffer views
   * whose coordinates place the same face a quarter of the image apart. */
  const float *Uv1 = nullptr;
  const float *Normals = nullptr;    /* 3 floats per vertex, unit, ECEF axes, or null */
  /* 4 floats per vertex -- a unit tangent in ECEF axes and glTF's handedness, so that the bitangent
   * is `cross(normal, tangent.xyz) * w` -- or null. IT IS FOUR AND NOT SIX, because the bitangent is
   * derived and a run that carried it would be a second answer to the same question that a mirrored
   * body puts out of step with the first. */
  const float *Tangents = nullptr;
  /* glTF's `COLOR_0`, 4 floats per vertex in LINEAR RGBA, or null (board:1193). It multiplies base
   * colour -- the rgb into whatever the arm shades with and the alpha into the coverage the surface
   * row declares -- so it reaches `alphaMode` as well as the colour, which is why it is four wide
   * and not three.
   *
   * A DRAW WHOSE LAYOUT CARRIES NO COLOUR RUN IS NOT DEGRADED, IT IS THE ORDINARY CASE: the vertex
   * arm of such a pipeline writes the multiplicative identity, exactly as a surface that declares no
   * image binds one white texel, so "no vertex colour" and "the factors alone" are one statement
   * rather than two fragment arms. */
  const float *Colours = nullptr;
  const float *Emitted = nullptr;    /* 3 floats per vertex */
  /* WHERE THE SAME VERTICES WERE AT THE PREVIOUS FRAME (board:1169), 3 floats each, offsets from
   * `PrevAnchor`. It is what makes `SceneVelocity` a motion and not a sentinel: the pose is baked
   * into the position run, so the previous pose is not recoverable from a transform and has to
   * arrive as its own run.
   *
   * IT IS REQUIRED EXACTLY WHERE THE PASS ATTACHES A VELOCITY TARGET, and refused where it does not
   * -- both directions, because a mesh that carried one into a plan with no velocity attachment
   * would be a run nobody reads, and a plan with the attachment and no run would write a sentinel
   * into a target something asked for. */
  const float *PrevVerts = nullptr;
  uint32_t VertexCount = 0;
  const uint32_t *Indices = nullptr;
  uint32_t IndexCount = 0;
  double Anchor[3] = {0, 0, 0};
  double PrevAnchor[3] = {0, 0, 0};
  const DrawList *Draws = nullptr;
};

class SubjectDraw {
public:
  /* WHETHER A SLOT'S TEXELS ARE DIRECTIONS OR VALUES, and it decides what averaging one mip level
   * into the next MEANS (board:1130). A colour, a roughness and an emission average; a normal does
   * not -- the mean of two unit vectors is shorter than either, and the lost length is lost
   * perturbation. An enumeration rather than a bool because the call site has to say which it is
   * (`Enum.2`), and a flag at four call sites is four chances to say it wrong. It is public because
   * the level builder is a free function beside the stage rather than a member of it. */

  [[nodiscard]] bool Configure(const Gpu &gpu, std::string &error);

  /* Replaces the surface table. A slot's index is what a draw key's material field names, so the
   * table and the list are written together or not at all. Refuses a surface kind this unit has no
   * pipeline for, naming it -- an unbuilt closure is a sentence here rather than a silently opaque
   * picture there. `OPAQUE`, `MASK` and `BLEND` are built; a transmissive sheet and a refracting
   * volume are not, and both need a scene the surface can see through rather than a blend factor. */
  [[nodiscard]] bool SetMaterials(const std::vector<SubjectMaterial> &materials,
                                  std::string &error);

  /* A mesh with no draw list, no vertices or no indices retires the unit, which is the state every
   * client that never declares a subject stays in for the whole of its life. Refuses a list naming a
   * surface slot the table does not hold, so `Encode` has no arm for one -- a draw silently skipped
   * in the encoder is a body missing from the picture with nothing to attribute it to. */
  [[nodiscard]] bool SetMesh(const SubjectMesh &mesh, std::string &error);

  /* Replaces the light list the lit arm reads. An empty list is a subject nothing lights, which is
   * what every case outside `KHR_lights_punctual` declares; more than `kMaxSubjectLights` is a
   * refusal naming both counts. */
  [[nodiscard]] bool SetLights(const std::vector<SubjectLight> &lights, std::string &error);
  /* The environment this subject gathers, declared and never discovered. */
  void SetEnvironment(const SubjectEnvironment &environment) { Environment = environment; }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  uint32_t VertexCount() const { return NVerts; }
  long TriangleCount() const { return (long)NIdx / 3; }
  /* How many `DrawIndexed` calls the last Encode submitted, and how many draws they covered: the
   * batching instrument, because a batching claim nobody counts is a claim. */
  uint32_t BatchCount() const { return (uint32_t)Batches.size(); }
  uint32_t DrawCount() const;
  /* HOW MANY PIPELINES `Configure` ACTUALLY BUILT (board:1187), which is not the size of the array
   * they sit in: the table is indexed by the whole `SurfaceKind` enumeration and two of its five
   * kinds have no closure, so `kPipelines` is 80 slots and 48 of them hold a pipeline. An instrument
   * asked whether a pipeline count cost anything has to read the count rather than be told it, and
   * the two numbers are far enough apart that being told the wrong one is the ordinary outcome. */
  uint32_t PipelineCount() const { return Built; }
  /* Where a shadow ray starts, in the subject's own metres -- readable because a bound on the
   * visibility estimator's displacement is derived from it (board:0089). */
  float ShadowNearM() const { return ShadowNearM_; }

private:
  static constexpr int kUniFloats = 40;   /* two of mat4 + anc -- the MSL struct `S` verbatim */
  /* THE SURFACE ROW ONE SLOT BINDS, the MSL struct `M` verbatim: the coverage factor and the mask
   * cutoff the emitted arm reads, then the metal-rough row and the emissive the lit arm reads. ONE
   * BUFFER AND NOT TWO because it is one surface: a second binding for the lit half would let a slot
   * be bound with a coverage from one material and a roughness from another. */
  /* THE THIRTEENTH IS THE SLOT'S OWN INDEX AND IT IS NOT A SURFACE PROPERTY (board:1138). It rides
   * here because it is what a fragment needs to say WHICH surface it wore and the row is the only
   * thing already bound per slot -- a second per-slot uniform would be a second binding to keep in
   * step with the first. It is written one higher than the slot so the identity attachment's clear
   * is distinguishable from the first slot. */
  static constexpr int kSurfaceScalars = 16; /* factor, cut, metalness, roughness, base4, emissive3,
                                              * normal scale, slot + 1, f0 3 (board:1205) */
  /* AND ONE uv MATRIX PER IMAGE (board:1177). `KHR_texture_transform` is stated inside each
   * `textureInfo`, so four references means four matrices and there is no per-material transform to
   * spell: an engine carrying one would have to choose which reference's it kept. Six floats each
   * because the third row of an affine 3x3 is `(0, 0, 1)` and storing it would be a number the
   * shader is free to read.
   *
   * THE COUNT IS DERIVED HERE rather than written as 37, so the cost of a socket appearing or
   * leaving cannot be stated in one place and paid in another. */
  static constexpr int kUvMatrixFloats = 6;
  /* AND ONE UV-SET SELECTOR PER IMAGE (board:1182), 0 for the first set and 1 for the second, so the
   * fragment's tap is `mix(uv0, uv1, k)` and the second set costs no branch, no second fragment arm
   * and no pipeline permutation of its own -- the permutation the second set does cost is the VERTEX
   * layout, because the run has to be bound.
   *
   * IT IS WRITTEN FROM `SubjectTexture::Set` AND NOWHERE ELSE, so the float is a narrowing of an
   * enumeration rather than a number a call site chose -- the same shape the slot index has, which
   * is the row's precedent for carrying a non-radiometric quantity. */
  static constexpr int kUvSetFloats = 1;
  static constexpr int kSurfaceFloats =
      kSurfaceScalars + (kUvMatrixFloats + kUvSetFloats) * (int)kSubjectImages;
  /* The light list as the shader reads it: a count, then `kMaxSubjectLights` entries of four
   * `float4` -- colour times intensity with the kind, the camera-relative position with the
   * reciprocal of the range, the beam, and the cone's two precomputed numbers. */
  static constexpr int kLightVec4s = 4;
  /* count(4) + the environment radiance(4) + the list. THE ENVIRONMENT IS PART OF THE LIGHT UNIFORM
   * AND NOT A SECOND ONE (board:1206): it IS a light -- an area source covering every direction --
   * and a surface gathers it in the same loop it gathers the others, so a binding of its own would
   * be a second place for a light to come from. */
  static constexpr int kLightFloats = 8 + 4 * kLightVec4s * (int)kMaxSubjectLights;
  /* EVERYTHING ONE SURFACE SLOT OWNS ON THE DEVICE, in one object rather than in five vectors that
   * have to be pushed in step (`C.1`). The five parallel runs were `Binds`, `Images`, `Views`,
   * `Samplers` and a `std::vector<bool>` of cull modes: a slot appended to four of them was a
   * mismatch nothing could catch until an encoder read past the end of the short one, and the
   * bool proxy specialisation (`SL.con.2`) meant the cull run alone had no address to take.
   *
   * ONE IMAGE AS THE DEVICE HOLDS IT. Four of these sit in a slot, and they are one type rather
   * than eight members with a prefix each: the five parallel runs this struct replaced are the same
   * defect one level up. */
  struct BoundImage {
    OwnedTexture Image;
    OwnedSampler Sample;
  };

  struct SurfaceSlot {
    BoundImage Colour;
    BoundImage Normal;
    BoundImage MetalRough;
    BoundImage Emissive;
    /* `KHR_materials_specular`'s two, in the order the sampler table binds them (board:1205). */
    BoundImage SpecularStrength;
    BoundImage SpecularTint;
    /* The slot's own surface row, as the numbers rather than as a buffer: SDL_GPU pushes uniform
     * data onto the command buffer, so a per-slot buffer would be a second copy of these twelve
     * floats with nothing reading it. Per SLOT because glTF states all of them per material, and
     * `AlphaBlendModeTest` renders three cutoffs in one file. */
    std::array<float, kSurfaceFloats> Row{};
    SurfaceKind Kind = SurfaceKind::Opaque;
    bool CullsBack = true;
    /* Whether any of this slot's four sockets reads the second uv set (board:1182), kept beside the
     * two other facts derived from the material at bind time rather than read back out of the row's
     * float selectors: the row is the device's wire form and a decision taken from it would be a
     * decision taken from a narrowing. */
    bool ReadsSecondUv = false;
  };

  /* One slot's four images and its surface row, appended to the table. */
  void BindSurface(const SubjectMaterial &material);
  /* One image on the device. `Decode` says whether the three colour channels carry the sRGB
   * transfer, which is glTF's per-socket rule and never a per-image guess. */
  enum class Transfer { Srgb, Linear };
  [[nodiscard]] BoundImage Upload(const SubjectTexture &texture, Transfer decode, TexelKind kind);
  /* One vertex or index buffer, filled from host memory through an upload transfer buffer. */
  [[nodiscard]] OwnedBuffer Fill(SDL_GPUBufferUsageFlags usage, const void *from, uint32_t bytes);
  /* The light list in the shader's own alphabet, restated camera-relative for this frame. */
  [[nodiscard]] std::array<float, kLightFloats> PackedLights(const FrameContext &ctx) const;
  /* Which of the built pipelines a draw of this layout, kind and facing takes. */
  [[nodiscard]] static size_t PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack);

  /* EVERY VERTEX LAYOUT TIMES THE TWO ANSWERS `doubleSided` CAN GIVE TIMES THE SURFACE KIND, all
   * built at configure time. A single pipeline with a white one-texel stand-in would make "no
   * texture declared" and "a white texture declared" the same picture; a single cull mode would make
   * `doubleSided` a property the reader carries and the picture ignores; and one alpha arm for all
   * three kinds is the failure `AlphaBlendModeTest` is built to show -- `OPAQUE` must ignore alpha
   * where `MASK` cuts on it and `BLEND` composites with it.
   *
   * THE KIND INDEXES THE WHOLE `SurfaceKind` ENUMERATION and only three of its five are built.
   * `SetMaterials` refuses the other two by name, so a slot naming one never reaches the table and
   * an unbuilt entry has no draw that can select it -- the refusal is the guard, not a check here. */
  static constexpr size_t kSurfaceKinds = 5;
  /* DERIVED FROM THE ONE TABLE THAT SAYS WHAT A LAYOUT IS (`draw/DrawList.h`), never written here as
   * a number: a count stated in one place and paid in another is how a layout gets a pipeline slot
   * nothing builds. */
  static constexpr size_t kVertexLayoutCount = kVertexLayouts.size();
  static constexpr size_t kPipelines = kVertexLayoutCount * 2 * kSurfaceKinds;

  SDL_GPUDevice *Device = nullptr;
  std::array<OwnedPipeline, kPipelines> Pipelines;
  uint32_t Built = 0;
  /* ONE SLOT PER SURFACE: that surface's four images with their samplers and its own row. The
   * encoder rebinds only where the batch's slot changes. */
  std::vector<SurfaceSlot> Slots;
  std::vector<DrawBatch> Batches;
  /* WHICH LAYOUT EACH BATCH IS ACTUALLY DRAWN THROUGH, decided in `SetMesh` and read in `Encode`
   * (board:1193). The draw's own layout is the ceiling and the mesh is the floor -- a list built
   * against one subject and a mesh set from another must not bind a slot with no buffer behind it --
   * and the degradation lands where a refusal can be spelled: `Encode` returns void, so a
   * combination the table does not carry had nowhere to be reported from there. It is also one fewer
   * derivation per batch per frame. */
  std::vector<VertexLayout> BatchLayout;
  /* The colour targets the scene pass attaches, as the compiled plan resolved them: the pipelines
   * and the fragment's output set are both built from this (board:1121). */
  std::vector<Resource> Colours;
  OwnedBuffer Vtx, Uv, Uv1, Nrm, Tan, Col, Emit, Idx, Prev;
  /* THE SUBJECT'S OWN GEOMETRY AS THE VISIBILITY TERM READS IT (`core/TriangleBvh.h`): the
   * acceleration structure's nodes and the triangles its leaves name, in the same anchor-relative
   * metres `Vtx` holds. They are built at `SetMesh` and not per frame, because nothing in them
   * depends on where the eye is. */
  OwnedBuffer BvhNodes, BvhTris;
  /* Where a shadow ray starts, so a surface does not shadow itself: a fixed fraction of the
   * structure's root box (`stages/ShadowRay.h`), in the subject's own metres. */
  float ShadowNearM_ = 0.0f;
  std::vector<SubjectLight> Placed;
  SubjectEnvironment Environment;
  uint32_t NVerts = 0, NIdx = 0;
  bool HasUv = false;
  bool HasUv1 = false;
  bool HasNormal = false;
  bool HasTangent = false;
  bool HasColour = false;
  bool FiltersFloat32 = false;
  double Anchor[3] = {0, 0, 0};
  double PrevAnchor[3] = {0, 0, 0};
  /* Whether the compiled plan attaches `SceneVelocity` to this pass, read once at `Configure` from
   * the plan's own colour set: it decides the fragment's output set, the vertex layout and whether
   * a mesh owes a previous-pose run, and all three come from this one answer. */
  bool WritesVelocity = false;
};

} // namespace outshine::Render
#endif
