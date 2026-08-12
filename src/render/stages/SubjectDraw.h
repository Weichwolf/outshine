/* THE DECLARED SUBJECT OF A STUDIO: a DRAW LIST over one indexed mesh -- many primitives, each with
 * its own material, its own vertex layout and its own place in the order, drawn under one stage.
 *
 * ONE DRAW PER STAGE WAS THE SHAPE THAT BROKE. `SciFiHelmet` puts several primitives under one mesh
 * with different materials; `ABeautifulGame` has thirty-odd meshes; `NodePerformanceTest` has ten
 * thousand. A unit that draws one subject with one material cannot show any of them, and building
 * them one at a time would rediscover the same missing list per asset. The list is `render/draw/`,
 * it has no device type in it, and the stage row above stays a PASS declaration.
 *
 * POSITION AND, WHERE THE PRIMITIVE CARRIES ONE, THE FIRST UV. NO NORMAL, and that is still the
 * unit's own limit rather than an omission: this draw emits a radiance the DECLARATION derived, so
 * nothing here reads a direction and a normal would be a number nobody uses. Inventing one for a
 * subject whose file carries none is what I.26 forbids; a punctual light is where `N.L` becomes
 * correct, and that is a different unit and a different round.
 *
 * THE RADIANCE IS THE DECLARATION'S AND NOT THIS FILE'S. Whether it came from a Lambertian facet
 * under a uniform environment (`rho*L`) or from an emissive surface (the colour itself) is a
 * property of the scene that was declared, and the two reduce to one number per surface; deriving it
 * here would put half a lighting model in a unit that has none.
 *
 * A UV IS NOT INVENTED EITHER. A primitive with no `TEXCOORD_0` is drawn by a different pipeline,
 * not by this one with a zero coordinate: a zero uv samples the image's corner and looks like a
 * texture that was authored flat.
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

#include "DrawList.h"
#include "GeometryUnit.h"
#include "Gpu.h"

namespace outshine::Render {

/* HOW A BASE-COLOUR TEXTURE IS ADDRESSED, glTF's own two questions and nothing else. The wrap mode
 * and the filter are the FILE's -- `TextureSettingsTest` renders one cell per wrap mode and an
 * engine that collapses two of them shows the wrong picture in exactly two cells -- so they cross
 * here rather than being decided in this unit. */
enum class SubjectWrap { ClampToEdge, MirroredRepeat, Repeat };
enum class SubjectFilter { Nearest, Linear };

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
 * `Coverage` IS THE DECLARED CONSTANT HALF OF ALPHA -- glTF's `baseColorFactor.a` -- and the
 * image's own alpha is the other half, multiplied in the shader. It is a separate number from the
 * radiance because `OPAQUE` ignores it entirely (`Specification.adoc:2178`) while `MASK` cuts on it
 * and `BLEND` composites with it: folding it into the colour is the premultiplication
 * `AlphaBlendModeTest` fails an engine for. */
struct SubjectMaterial {
  SurfaceState Surface = StateOf(Material{});
  SubjectTexture Colour;
  float Coverage = 1.0f;
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
  const float *Emitted = nullptr;    /* 3 floats per vertex */
  uint32_t VertexCount = 0;
  const uint32_t *Indices = nullptr;
  uint32_t IndexCount = 0;
  double Anchor[3] = {0, 0, 0};
  const DrawList *Draws = nullptr;
};

class SubjectDraw : public GeometryUnit {
public:
  void Configure(const Gpu &gpu);

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

  void Encode(const FrameContext &ctx, ClusterCut &cut, wgpu::RenderPassEncoder &pass) override;

  uint32_t VertexCount() const { return NVerts; }
  long TriangleCount() const { return (long)NIdx / 3; }
  /* How many `DrawIndexed` calls the last Encode submitted, and how many draws they covered: the
   * batching instrument, because a batching claim nobody counts is a claim. */
  uint32_t BatchCount() const { return (uint32_t)Batches.size(); }
  uint32_t DrawCount() const;

private:
  /* EVERYTHING ONE SURFACE SLOT OWNS ON THE DEVICE, in one object rather than in five vectors that
   * have to be pushed in step (`C.1`). The five parallel runs were `Binds`, `Images`, `Views`,
   * `Samplers` and a `std::vector<bool>` of cull modes: a slot appended to four of them was a
   * mismatch nothing could catch until an encoder read past the end of the short one, and the
   * bool proxy specialisation (`SL.con.2`) meant the cull run alone had no address to take. */
  struct SurfaceSlot {
    wgpu::Texture Image;
    wgpu::TextureView View;
    wgpu::Sampler Sample;
    /* The two per-slot numbers the fragment reads: the declared coverage factor and, under `MASK`,
     * the alpha below which a fragment is discarded. Per SLOT because glTF states both per
     * material, and `AlphaBlendModeTest` renders three cutoffs in one file. */
    wgpu::Buffer Alpha;
    wgpu::BindGroup Bind;
    SurfaceKind Kind = SurfaceKind::Opaque;
    bool CullsBack = true;
  };

  /* One slot's texture, sampler, alpha uniform and bind group, appended to the table. */
  void BindSurface(const SubjectMaterial &material);
  /* Which of the built pipelines a draw of this layout, kind and facing takes. */
  [[nodiscard]] static size_t PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack);

  static constexpr int kUniFloats = 20;   /* mat4 + anc -- the WGSL struct `S` verbatim */
  static constexpr int kAlphaFloats = 4;  /* factor, cut, and the pad a uniform binding wants */
  /* TWO VERTEX LAYOUTS TIMES THE TWO ANSWERS `doubleSided` CAN GIVE TIMES THE SURFACE KIND, all
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
  static constexpr size_t kPipelines = 2 * 2 * kSurfaceKinds;

  wgpu::Device Device;
  wgpu::Queue Queue;
  std::array<wgpu::RenderPipeline, kPipelines> Pipelines;
  wgpu::BindGroupLayout Layout;
  /* ONE BIND GROUP PER SURFACE SLOT: the shared uniform plus that surface's own texture, sampler
   * and alpha. The encoder rebinds only where the batch's slot changes. */
  std::vector<SurfaceSlot> Slots;
  std::vector<DrawBatch> Batches;
  wgpu::Buffer Uni, Vtx, Uv, Emit, Idx;
  uint32_t NVerts = 0, NIdx = 0;
  bool HasUv = false;
  bool FiltersFloat32 = false;
  double Anchor[3] = {0, 0, 0};
};

} // namespace outshine::Render
#endif
