/* THE DRAW LIST'S TWO CLAIMS, AND THEY ARE IN TENSION -- which is why the key's field order is the
 * whole design. Correctness first: a blended surface is composited over what is behind it, so it may
 * never sort ahead of an opaque one, and among blended surfaces the farther one goes first.
 * Performance second, out of what correctness leaves: draws that agree on pipeline and surface slot
 * are laid out contiguously and merged into one call.
 *
 * NO DEVICE HERE. The whole point of putting the list in `src/render/draw/` is that ordering,
 * refusal and batching are decidable with no `wgpu::` name in scope, so this test links `src/core`
 * and the list and nothing else. */
#include <string>
#include <vector>

#include "Check.h"

#include "DrawList.h"

using outshine::AlphaMode;
using outshine::Material;
using outshine::StateOf;
using outshine::SurfaceKind;
using outshine::Render::DrawItem;
using outshine::Render::DrawKey;
using outshine::Render::DrawList;
using outshine::Render::DrawOrder;
using outshine::Render::kMaterialSlots;
using outshine::Render::VertexLayout;
using outshine::Render::ViewLayer;

namespace {

DrawItem Draw(uint32_t slot, double depth, AlphaMode alpha, uint32_t firstIndex,
              uint32_t indexCount) {
  Material material;
  material.Alpha = alpha;
  DrawItem item;
  item.Order.Layer = ViewLayer::World;
  item.Order.Surface = StateOf(material);
  item.Order.DepthFraction = depth;
  item.Order.MaterialSlot = slot;
  item.SourceFirstIndex = firstIndex;
  item.IndexCount = indexCount;
  return item;
}

uint64_t KeyOf(uint32_t slot, double depth, AlphaMode alpha) {
  return DrawKey::Of(Draw(slot, depth, alpha, 0, 3).Order).Bits();
}

} // namespace

int main() {
  using namespace outshine::Test;

  /* ERICSON'S ORDER, READ OFF THE KEY ITSELF. The translucency field sits above the depth field, so
   * no depth and no material can lift a blended draw above an opaque one. */
  CHECK(KeyOf(0, 0.99, AlphaMode::Opaque) < KeyOf(0, 0.01, AlphaMode::Blended),
        "the farthest opaque draw still sorts before the nearest blended one");
  CHECK(KeyOf(kMaterialSlots - 1, 0.5, AlphaMode::Opaque) < KeyOf(0, 0.5, AlphaMode::Masked),
        "the material field cannot lift an opaque draw past a masked one, whatever its slot");
  CHECK(KeyOf(0, 0.25, AlphaMode::Opaque) < KeyOf(0, 0.75, AlphaMode::Opaque),
        "opaque draws sort front to back");
  CHECK(KeyOf(0, 0.75, AlphaMode::Blended) < KeyOf(0, 0.25, AlphaMode::Blended),
        "blended draws sort back to front, out of the same ascending comparison");
  CHECK(KeyOf(1, 0.5, AlphaMode::Opaque) < KeyOf(2, 0.5, AlphaMode::Opaque),
        "at one depth and one kind the surface slot is what orders, which is what groups a batch");

  /* THE LIST REFUSES WHAT IT CANNOT ORDER, AND NAMES IT. */
  {
    DrawList list;
    std::string why;
    CHECK(!list.Add(Draw(kMaterialSlots, 0.5, AlphaMode::Opaque, 0, 3), why),
          "a surface slot beyond the key's material field is refused");
    CHECK(why.find(std::to_string(kMaterialSlots)) != std::string::npos,
          "the refusal names the slot count the key addresses");
    CHECK(!list.Add(Draw(0, 0.5, AlphaMode::Opaque, 0, 0), why),
          "a draw covering no index is refused rather than silently dropped");
    CHECK(!list.Add(Draw(0, 0.5, AlphaMode::Opaque, 0, 4), why),
          "a draw whose index count is not a whole number of triangles is refused");
    CHECK(list.Empty(), "nothing refused entered the list");
  }

  /* THE COMPILED RUN COVERS EVERY DRAW EXACTLY ONCE, in the sorted order, and that is what makes the
   * batcher's contiguity test true rather than hopeful. */
  {
    DrawList list;
    std::string why;
    bool added = list.Add(Draw(2, 0.80, AlphaMode::Opaque, 300, 3), why);
    added = list.Add(Draw(1, 0.20, AlphaMode::Opaque, 100, 6), why) && added;
    added = list.Add(Draw(1, 0.20, AlphaMode::Opaque, 200, 9), why) && added;
    added = list.Add(Draw(0, 0.10, AlphaMode::Blended, 400, 3), why) && added;
    CHECK(added, "four well-formed draws enter the list");
    list.Compile();

    CHECK(list.IndexCount() == 21, "the compiled run is as long as the draws it lays out");
    CHECK(list.Runs().size() == 4, "every draw contributes exactly one source run");
    uint32_t at = 0;
    bool contiguous = true;
    for (size_t which = 0; which < list.Draws().size(); ++which) {
      contiguous = contiguous && list.Draws()[which].FirstIndex == at &&
                   list.Runs()[which].Count == list.Draws()[which].IndexCount;
      at += list.Draws()[which].IndexCount;
    }
    CHECK(contiguous, "each draw's compiled place follows the one before it with no gap");

    /* THE TWO DRAWS OF SLOT 1 AT ONE DEPTH ARE ADJACENT AFTER THE SORT, so they cost one call; the
     * blended draw is last whatever its depth. */
    CHECK(list.Batches().size() == 3, "four draws over three distinct states cost three calls");
    CHECK(list.Batches()[0].Draws == 2 && list.Batches()[0].IndexCount == 15,
          "the two draws of one surface slot at one depth merged into one call");
    CHECK(list.Batches().back().Kind == SurfaceKind::Blended,
          "the blended draw is the last call of the pass");
    Note("draws in the list", (double)list.Draws().size(), "draws");
    Note("draw calls after merging", (double)list.Batches().size(), "calls");

    /* INSERTION ORDER BREAKS EVERY TIE, so the same declaration compiles to the same order twice --
     * no address, no container order, no hash decides a pixel. */
    DrawList again;
    added = again.Add(Draw(2, 0.80, AlphaMode::Opaque, 300, 3), why);
    added = again.Add(Draw(1, 0.20, AlphaMode::Opaque, 100, 6), why) && added;
    added = again.Add(Draw(1, 0.20, AlphaMode::Opaque, 200, 9), why) && added;
    added = again.Add(Draw(0, 0.10, AlphaMode::Blended, 400, 3), why) && added;
    again.Compile();
    bool same = added && again.Draws().size() == list.Draws().size();
    for (size_t which = 0; same && which < again.Draws().size(); ++which) {
      same = again.Draws()[which].SourceFirstIndex == list.Draws()[which].SourceFirstIndex;
    }
    CHECK(same, "one declaration compiles to one order, twice");
  }

  /* A DIFFERENT VERTEX LAYOUT IS A DIFFERENT PIPELINE, so it breaks a batch even where the surface
   * slot agrees -- which is the case a file with uvs on one primitive and none on the next is. */
  {
    DrawList list;
    std::string why;
    DrawItem plain = Draw(0, 0.5, AlphaMode::Opaque, 0, 3);
    DrawItem textured = Draw(0, 0.5, AlphaMode::Opaque, 3, 3);
    textured.Layout = VertexLayout::PositionUv;
    bool added = list.Add(plain, why) && list.Add(textured, why);
    CHECK(added, "two draws of one slot and two layouts enter the list");
    list.Compile();
    CHECK(list.Batches().size() == 2, "two vertex layouts are two calls whatever the slot says");
  }

  /* THE LAYOUT TABLE IS THE VALID SET, AND A SET OF RUNS THAT IS NOT IN IT IS A REFUSAL
   * (board:1193). The vertex colour doubles the table because it depends on nothing -- every layout
   * has one with it and one without -- while the other four constrain each other, and it is exactly
   * those constraints that leave combinations with no row: a tangent with no normal, a second uv set
   * with no first. `LayoutOf` answers the row or says there is none; the nearest row is never an
   * answer, because a draw bound through a layout it did not ask for is the silent substitution the
   * enumeration exists to prevent. */
  {
    using outshine::Render::CarriesColour;
    using outshine::Render::kVertexLayouts;
    using outshine::Render::LayoutOf;
    using outshine::Render::VertexRunsCarried;
    CHECK(kVertexLayouts.size() == 16,
          "sixteen layouts: eight without a vertex colour run and the same eight with one");
    size_t tinted = 0;
    for (const auto &row : kVertexLayouts) { tinted += CarriesColour(row.Layout) ? 1u : 0u; }
    CHECK(tinted == 8, "and exactly half of them carry it, which is what a free axis looks like");

    VertexLayout layout = VertexLayout::Position;
    VertexRunsCarried bare;
    CHECK(LayoutOf(bare, layout) && layout == VertexLayout::Position,
          "no run at all is the position-only layout");
    VertexRunsCarried tintedOnly;
    tintedOnly.Colour = true;
    CHECK(LayoutOf(tintedOnly, layout) && layout == VertexLayout::PositionColour,
          "a vertex colour needs nothing else, so position plus colour is a layout");
    VertexRunsCarried whole;
    whole.Uv = whole.Uv1 = whole.Normal = whole.Tangent = whole.Colour = true;
    CHECK(LayoutOf(whole, layout) && layout == VertexLayout::PositionNormalUvUv1TangentColour,
          "and every run at once is the widest one");
    VertexRunsCarried tangentWithoutNormal;
    tangentWithoutNormal.Uv = true;
    tangentWithoutNormal.Tangent = true;
    CHECK(!LayoutOf(tangentWithoutNormal, layout),
          "a tangent with no normal is not a layout, with or without a colour, and the nearest row "
          "is not an answer");
    tangentWithoutNormal.Colour = true;
    CHECK(!LayoutOf(tangentWithoutNormal, layout),
          "and the colour axis does not make one of it");
    VertexRunsCarried secondWithoutFirst;
    secondWithoutFirst.Uv1 = true;
    CHECK(!LayoutOf(secondWithoutFirst, layout),
          "and neither is a second uv set with no first");
  }

  Covers("board:1193 the vertex colour is a free axis over the layout table: sixteen rows, and a "
         "set of runs the table does not carry is refused rather than rounded to the nearest one");
  Covers("I.27 the draw list is the pass's, not the stage's: it carries the sort key, the batching "
         "and the material state, and the sort key is per draw with Ericson's field order -- "
         "viewport, viewport layer, translucency type, depth, material");
  return Report();
}
