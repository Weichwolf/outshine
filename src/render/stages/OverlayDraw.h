/* WHAT A CONSUMER DECLARED, DRAWN OVER THE FRAME: rectangles, and a rectangle of an atlas in each.
 *
 * **IT IS A MECHANISM AND THE MEANING IS THE CONSUMER'S** (board:1442). This stage spells one verb --
 * draw this rectangle with that patch of that image at that colour -- and knows nothing about lists,
 * glyphs, panels or cursors. A button, a name, a health bar and a page of a book are the same numbers
 * to it, which is exactly what keeps a content taxonomy out of the engine.
 *
 * **THE ATLAS IS THE CONSUMER'S IMAGE AND THE ENGINE ONLY HOLDS IT.** A font is not the engine's
 * business -- who makes an asset is not the engine's business, and text is an asset like bark or a
 * facade -- so the consumer hands over the bytes and gets back nothing but the ability to point into
 * them. A declaration that names no atlas draws from colour alone against a single white texel, so
 * *no atlas* is a value here rather than a branch.
 *
 * **IT CONTRIBUTES AND DOES NOT WRITE.** The frame exists without it; a plan that declares no overlay
 * is exactly the plan it was, which is what `Contributes` means in the catalogue and why this costs a
 * picture nothing that does not ask for it. It sits AFTER the display transfer because a consumer's
 * colour is display-referred: a panel put through the transfer would come out a colour nobody
 * declared.
 *
 * **THE CLIP IS PER QUAD AND NOT A SCISSOR.** Every rectangle carries the region it may touch, already
 * intersected by the layer that laid it out, so one pass draws the whole interface and no stack of
 * scissor changes has to be replayed -- which is what keeps this one bind, one buffer and one draw. */
#ifndef OVERLAYDRAW_H
#define OVERLAYDRAW_H

#include <cstdint>
#include <string>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

/* ONE RECTANGLE, IN THE TARGET'S OWN PIXELS AND THE ATLAS'S OWN UNIT SQUARE. A patch with no area is
 * drawn from the colour alone. **The layout is the vertex format** -- five `float4` in declaration
 * order -- so a field added here is a field added there, and the two cannot drift apart silently. */
struct OverlayQuad {
  float LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  float U0 = 0, V0 = 0, U1 = 0, V1 = 0;
  float Red = 1, Green = 1, Blue = 1, Alpha = 1;
  float ClipLeftPx = 0, ClipTopPx = 0, ClipWidthPx = 0, ClipHeightPx = 0;
  float RadiusPx = 0, Opacity = 1, Pad0 = 0, Pad1 = 0;
};

/* [SET] THE BOUND, AND IT IS A NUMBER SOMEBODY CHOSE. Sixteen thousand rectangles is a 720p screen
 * filled twice over with eight-pixel glyphs; a consumer that needs more is describing a texture rather
 * than an interface. It is the same number `Ui::kQuadBound` states, because the two ends of one path
 * must agree -- and `SetQuads` REFUSES beyond it and says by how much, since a silently truncated
 * interface draws a picture nobody declared. */
inline constexpr size_t kMaxOverlayQuads = 16384;

class OverlayDraw {
public:
  /* `smooth` is the sampler the atlas is read through -- a glyph sheet is minified and magnified like
   * any other image. `targetFormat` is the frame's, because a pipeline that guessed one is refused by
   * the driver and a refused pipeline encodes nothing, which reads as a missing interface rather than
   * as an error. */
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUSampler *smooth,
                               SDL_GPUTextureFormat targetFormat, std::string &error);
  /* The consumer's atlas, uploaded once and held until it is replaced. RGBA8, tightly packed. */
  [[nodiscard]] bool SetAtlas(const Gpu &gpu, const uint8_t *rgba, int width, int height,
                              std::string &error);
  /* The frame's rectangles, uploaded HERE and not inside the pass -- the same shape `SetMesh` has, so
   * the frame path binds and draws and touches no allocator. */
  [[nodiscard]] bool SetQuads(const Gpu &gpu, const OverlayQuad *quads, size_t count,
                              std::string &error);
  /* THE TARGET'S SIZE, because a rectangle is declared in the target's own pixels and the mapping to
   * clip space is the only thing this stage needs to know about where it is drawing. A surface that
   * resizes re-binds; nothing about the pipeline moves. */
  void Bind(int widthPx, int heightPx) {
    WidthPx = widthPx;
    HeightPx = heightPx;
  }
  /* `ctx` IS TAKEN AND NOT READ, AND THAT IS THE CLAIM. Every other raster stage places geometry
   * against the eye; an interface is in target pixels and knows no camera, no view and no previous
   * frame -- which is exactly what makes the same declaration land on a HUD and on a texture a wall
   * wears, with the client choosing and the stage unable to tell. */
  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] uint32_t Held(void) const { return Count; }

private:
  OwnedPipeline Pipe;
  OwnedBuffer Verts;
  OwnedTexture Atlas;
  SDL_GPUSampler *Smooth = nullptr;
  uint32_t Count = 0;     /* how many quads the buffer holds this frame */
  uint32_t Capacity = 0;  /* how many it can hold at all */
  int WidthPx = 0, HeightPx = 0;
};

} // namespace outshine::Render
#endif
