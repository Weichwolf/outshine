/* THE DECLARED SUBJECT OF A STUDIO: one indexed triangle mesh, drawn where the declaration puts it,
 * emitting what the declaration says it emits.
 *
 * POSITION AND, WHERE THE FILE CARRIES ONE, THE FIRST UV. NO NORMAL, and that is still the unit's
 * own limit rather than an omission: this draw emits a radiance the DECLARATION derived, so nothing
 * here reads a direction and a normal would be a number nobody uses. Inventing one for a subject
 * whose file carries none is what I.26 forbids; a punctual light is where `N.L` becomes correct, and
 * that is a different unit and a different round.
 *
 * THE RADIANCE IS THE DECLARATION'S AND NOT THIS FILE'S. Whether it came from a Lambertian facet
 * under a uniform environment (`rho*L`) or from an emissive surface (the colour itself) is a
 * property of the scene that was declared, and the two reduce to one number per surface; deriving it
 * here would put half a lighting model in a unit that has none.
 *
 * A UV IS NOT INVENTED EITHER. A subject with no `TEXCOORD_0` is drawn by a different pipeline, not
 * by this one with a zero coordinate: a zero uv samples the image's corner and looks like a texture
 * that was authored flat.
 *
 * BACK FACES ARE CULLED AND THE WINDING IS TRUSTED, because the format defines one. A path tracer
 * hits a single-sided triangle from either side and still shades it, so the two renderers disagree
 * about a REVERSED subject and agree about a correct one -- which is the point: with culling off,
 * reversing every triangle of a subject moved no pixel of either mask, and three claims about
 * winding stood on an instrument that could not see it. */
#ifndef SUBJECTDRAW_H
#define SUBJECTDRAW_H

#include <cstdint>

#include "GeometryUnit.h"
#include "Gpu.h"

namespace outshine::Render {

/* HOW A BASE-COLOUR TEXTURE IS ADDRESSED, glTF's own two questions and nothing else. The wrap mode
 * and the filter are the FILE's -- `TextureSettingsTest` renders one cell per wrap mode and an
 * engine that collapses two of them shows the wrong picture in exactly two cells -- so they cross
 * here rather than being decided in this unit. */
enum class SubjectWrap { ClampToEdge, MirroredRepeat, Repeat };
enum class SubjectFilter { Nearest, Linear };

/* THE DECODED BASE COLOUR, RGBA8, straight alpha, top row first -- the one convention the image
 * boundary states. Uploaded into an sRGB-VIEWED texture, so the transfer is undone by the SAMPLER
 * before it filters, which is what `TextureLinearInterpolationTest` decides: filtering must run on
 * linear values, and a shader that decoded after the tap fails by construction. */
struct SubjectTexture {
  const uint8_t *Rgba = nullptr;
  uint32_t Width = 0;
  uint32_t Height = 0;
  SubjectWrap WrapU = SubjectWrap::Repeat;
  SubjectWrap WrapV = SubjectWrap::Repeat;
  SubjectFilter Magnify = SubjectFilter::Linear;
};

class SubjectDraw : public GeometryUnit {
public:
  void Configure(const Gpu &gpu);

  /* Replaces the base-colour texture. A texture with no texels retires the one that was there, so
   * "the case before this one had a texture" is not a state a case can inherit. */
  void SetTexture(const SubjectTexture &texture);

  /* `verts` is 3 floats per vertex, ECEF offsets from `anchor` in metres; `idx` indexes them.
   * `uv` is 2 floats per vertex or null, and null is what selects the untextured pipeline -- the
   * two are different shaders and neither stands in for the other with a substituted constant.
   *
   * `emitted` is 3 floats per vertex: the scene-referred linear radiance that vertex's surface
   * leaves, which the declaration derived and this unit neither shades nor scales. IT IS PER VERTEX
   * AND NOT PER SUBJECT so that a subject built from several nodes cannot be drawn in one colour --
   * a single flat value over touching bodies hides their internal silhouettes, and no rule against
   * it is needed when the array has no shorter spelling. It is carried flat to the fragment, so a
   * face's value is the declared one bit for bit and not an interpolation of three equal numbers.
   *
   * A zero count retires the unit, which is the state every client that never declares a subject
   * stays in for the whole of its life. */
  void SetMesh(const float *verts, const float *uv, const float *emitted, uint32_t nverts,
               const uint32_t *idx, uint32_t nidx, const double anchor[3]);

  void Encode(const FrameContext &ctx, ClusterCut &cut, wgpu::RenderPassEncoder &pass) override;

  uint32_t VertexCount() const { return NVerts; }
  long TriangleCount() const { return (long)NIdx / 3; }

private:
  static constexpr int kUniFloats = 20; /* mat4 + anc -- the WGSL struct `S` verbatim */

  void Rebind();

  wgpu::Device Device;
  wgpu::Queue Queue;
  /* TWO PIPELINES, ONE PER VERTEX LAYOUT, both built at configure time. A single pipeline with a
   * white one-texel stand-in would make "no texture declared" and "a white texture declared" the
   * same picture, and the first is a subject this engine must be able to draw. */
  wgpu::RenderPipeline Plain, Textured;
  wgpu::BindGroupLayout Layout;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni, Vtx, Uv, Emit, Idx;
  wgpu::Texture BaseColour;
  wgpu::TextureView BaseColourView;
  wgpu::Sampler Samp;
  uint32_t NVerts = 0, NIdx = 0;
  bool HasUv = false;
  double Anchor[3] = {0, 0, 0};
};

} // namespace outshine::Render
#endif
