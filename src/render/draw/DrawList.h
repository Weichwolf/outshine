/* THE LIST OF DRAWS INSIDE ONE PASS. A stage declares a pass; this is what the pass draws, and it is
 * many primitives with their own materials, their own vertex layouts and their own places in the
 * order -- not one draw wearing a stage's name.
 *
 * COMPILING IS WHAT MAKES BATCHING TRUE RATHER THAN LUCKY. Sorting alone leaves two draws of one
 * material adjacent in the list and far apart in the index buffer, so they still cost two calls.
 * `Compile` sorts by `DrawKey` and then ASSIGNS each draw its place in the index run the consumer is
 * about to write, so draws that may be merged are contiguous by construction. `Runs()` is the
 * instruction for writing that buffer, and `Batches()` is what the encoder submits.
 *
 * THE SORT IS STABLE AND THE ORDER IS THEREFORE TOTAL. Two draws may carry the same key -- that is
 * what makes them batchable -- and a comparison sort that broke the tie by address would put a
 * pointer value in the picture. Insertion order breaks it instead, which is data the caller wrote
 * down (`CLAUDE.md`: the mathematics is deterministic).
 *
 * NO DEVICE TYPE APPEARS HERE, which is what lets a draw list be built, sorted and checked with no
 * device name in scope -- the same property that makes `render/plan/` checkable, one level down. */
#ifndef DRAWLIST_H
#define DRAWLIST_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "DrawKey.h"

namespace outshine::Render {

/* WHICH VERTEX ATTRIBUTES A DRAW READS, and it is a property of the DRAW rather than of the subject
 * it belongs to: a file may carry uvs on one primitive and none on the next, and drawing the second
 * through the first's pipeline with a zero coordinate samples the image's corner (`Enum.2`).
 *
 * THE NORMAL IS ALSO THE STATEMENT THAT THE DRAW IS LIT, and the two are one field on purpose. A
 * normal exists to be dotted with a direction; a draw with no light to face reads it for nothing,
 * and a draw that faces a light without one would have to substitute a direction -- which is the
 * black hemisphere or the flat plate that a lighting model invents when an attribute is missing.
 * Folding the two into one enumeration makes "lit without a normal" and "a normal nobody lights"
 * both unspellable, where two independent fields would spell each of them.
 *
 * THE TANGENT IS THE SAME ARGUMENT ONE ATTRIBUTE ALONG. It exists to turn a normal map's tangent
 * space into the world's, so a draw whose surface declares no normal map reads it for nothing, and a
 * draw that samples one without it would have to invent a basis -- which is the arbitrary uv-axis
 * frame that makes `NormalTangentTest`'s cells disagree with each other. It comes with the normal
 * and the uv set and never alone: the basis is the triple, and a layout that could spell two thirds
 * of it would be a normal map sampled against half a frame.
 *
 * THE SECOND UV SET IS THE FOURTH ATTRIBUTE AND IT KEEPS THIS AN ENUMERATION (board:1182). The
 * question board:1156 asks of every added value was answered here rather than inherited: a flag set
 * of four independent booleans spells sixteen layouts, and eight of them are not layouts at all --
 * a tangent with no normal, a tangent with no uv, a SECOND uv set with no first. Those are exactly
 * the spellings the three paragraphs above exist to remove, so a flag set would trade a type that
 * cannot express a mistake for a validator that reports one, which is the trade this repository's
 * own criterion refuses. It stays an enumeration and the eight named values ARE the valid set.
 *
 * WHAT THE MULTIPLICATION COSTS IS PAID DOWN INSTEAD, and that is the part board:1156 is right
 * about: five membership lists over eight values would be forty hand-written terms to keep in step.
 * The table below states each layout's attributes ONCE, by name, and every predicate, the pipeline
 * count and the build loop read it -- so a ninth value is one row and nothing to remember.
 *
 * THE VERTEX COLOUR IS THE FIFTH ATTRIBUTE AND IT DOUBLES THE TABLE RATHER THAN EXTENDING IT
 * (board:1193). glTF's `COLOR_0` is an additional linear multiplier on base colour, so it depends on
 * NOTHING: every one of the eight above is a layout with it and a layout without it, and the second
 * eight below are the first eight with the run added. That is what makes it a doubling where the
 * second uv set was one row -- the other four attributes constrain each other and this one does not.
 *
 * WHAT THE DOUBLING COSTS WAS MEASURED BEFORE IT WAS SPENT (board:1187): every subject pipeline is
 * built in `Configure`, which runs only from `Renderer::Init`, so 48 pipelines became 96 at about
 * 6.9 ms each on a cold init and nothing this tree's frame instrument can resolve inside a frame. */
enum class VertexLayout : uint8_t {
  Position,
  PositionUv,
  PositionUvUv1,
  PositionNormal,
  PositionNormalUv,
  PositionNormalUvUv1,
  PositionNormalUvTangent,
  PositionNormalUvUv1Tangent,
  PositionColour,
  PositionUvColour,
  PositionUvUv1Colour,
  PositionNormalColour,
  PositionNormalUvColour,
  PositionNormalUvUv1Colour,
  PositionNormalUvTangentColour,
  PositionNormalUvUv1TangentColour
};

/* THE ATTRIBUTES ONE ROW OF THE TABLE NAMES. They are flags and not four `bool` members on purpose:
 * a row of four positional booleans can be written with two of them swapped and the compiler has
 * nothing to say (`I.24`), where a row that names `Normal` names it. The flags are private to the
 * table -- no interface takes one -- so the invalid combinations remain unspellable as a layout. */
enum class VertexAttribute : uint8_t {
  None = 0,
  Uv = 1u << 0,
  Uv1 = 1u << 1,
  Normal = 1u << 2,
  Tangent = 1u << 3,
  Colour = 1u << 4
};

[[nodiscard]] constexpr VertexAttribute operator|(VertexAttribute a, VertexAttribute b) {
  return static_cast<VertexAttribute>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
[[nodiscard]] constexpr bool Holds(VertexAttribute set, VertexAttribute one) {
  return (static_cast<uint8_t>(set) & static_cast<uint8_t>(one)) != 0;
}

struct VertexLayoutRow {
  VertexLayout Layout = VertexLayout::Position;
  VertexAttribute Carries = VertexAttribute::None;
};

/* EVERY LAYOUT THIS ENGINE BUILDS, AND WHAT EACH ONE CARRIES. It is also the ORDER the pipeline
 * table is indexed in, which the `static_assert` below holds rather than a convention: a row out of
 * place would silently give one layout another's pipelines. */
inline constexpr std::array kVertexLayouts = {
    VertexLayoutRow{VertexLayout::Position, VertexAttribute::None},
    VertexLayoutRow{VertexLayout::PositionUv, VertexAttribute::Uv},
    VertexLayoutRow{VertexLayout::PositionUvUv1, VertexAttribute::Uv | VertexAttribute::Uv1},
    VertexLayoutRow{VertexLayout::PositionNormal, VertexAttribute::Normal},
    VertexLayoutRow{VertexLayout::PositionNormalUv, VertexAttribute::Uv | VertexAttribute::Normal},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal},
    VertexLayoutRow{VertexLayout::PositionNormalUvTangent,
                    VertexAttribute::Uv | VertexAttribute::Normal | VertexAttribute::Tangent},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1Tangent,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal |
                        VertexAttribute::Tangent},
    VertexLayoutRow{VertexLayout::PositionColour, VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionUvColour,
                    VertexAttribute::Uv | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionUvUv1Colour,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalColour,
                    VertexAttribute::Normal | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvColour,
                    VertexAttribute::Uv | VertexAttribute::Normal | VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1Colour,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal |
                        VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvTangentColour,
                    VertexAttribute::Uv | VertexAttribute::Normal | VertexAttribute::Tangent |
                        VertexAttribute::Colour},
    VertexLayoutRow{VertexLayout::PositionNormalUvUv1TangentColour,
                    VertexAttribute::Uv | VertexAttribute::Uv1 | VertexAttribute::Normal |
                        VertexAttribute::Tangent | VertexAttribute::Colour}};

[[nodiscard]] constexpr bool VertexLayoutsIndexThemselves() {
  for (size_t at = 0; at < kVertexLayouts.size(); ++at) {
    if (static_cast<size_t>(kVertexLayouts[at].Layout) != at) { return false; }
  }
  return true;
}
static_assert(VertexLayoutsIndexThemselves(),
              "a layout's place in the table is the index its pipelines are built at");

[[nodiscard]] constexpr VertexAttribute AttributesOf(VertexLayout layout) {
  return kVertexLayouts[static_cast<size_t>(layout)].Carries;
}

/* Whether a layout carries the first uv set, the second one, the normal that makes it lit, the
 * tangent that makes it normal-mapped, and the vertex colour that multiplies its base colour. Stated
 * once, in the table above, so the encoder and the pipeline table cannot disagree about what a
 * layout is. */
[[nodiscard]] constexpr bool CarriesUv(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Uv);
}
[[nodiscard]] constexpr bool CarriesUv1(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Uv1);
}
[[nodiscard]] constexpr bool CarriesNormal(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Normal);
}
[[nodiscard]] constexpr bool CarriesTangent(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Tangent);
}
[[nodiscard]] constexpr bool CarriesColour(VertexLayout layout) {
  return Holds(AttributesOf(layout), VertexAttribute::Colour);
}

/* WHICH RUNS A DRAW ACTUALLY HAS TO DRAW WITH, as a named record rather than five positional
 * booleans (`I.23`, `I.24`): every one of them is a `bool` and a call that swapped two of them would
 * compile and draw a normal-mapped surface through the uv slot. */
struct VertexRunsCarried {
  bool Uv = false;
  bool Uv1 = false;
  bool Normal = false;
  bool Tangent = false;
  bool Colour = false;
};

/* THE LAYOUT A SET OF RUNS NAMES, LOOKED UP IN THE TABLE ABOVE (board:1193). It exists because the
 * answer had TWO spellings -- a nested conditional in the compositor that builds the list and
 * another in the encoder that checks it against the mesh -- and the vertex colour turns each of them
 * into a sixteen-way expression that the other can drift away from.
 *
 * A COMBINATION THE TABLE DOES NOT CARRY IS A REFUSAL AND NOT THE NEAREST ROW. A tangent with no
 * normal is not a layout, and answering one anyway is exactly the silent substitution the
 * enumeration exists to prevent; both callers derive their flags by conjunction -- a tangent only
 * where the normal and the first uv set are there -- so what this returns false for is a caller that
 * stopped doing that. */
[[nodiscard]] constexpr bool LayoutOf(const VertexRunsCarried &carried, VertexLayout &out) {
  VertexAttribute wanted = VertexAttribute::None;
  if (carried.Uv) { wanted = wanted | VertexAttribute::Uv; }
  if (carried.Uv1) { wanted = wanted | VertexAttribute::Uv1; }
  if (carried.Normal) { wanted = wanted | VertexAttribute::Normal; }
  if (carried.Tangent) { wanted = wanted | VertexAttribute::Tangent; }
  if (carried.Colour) { wanted = wanted | VertexAttribute::Colour; }
  for (const VertexLayoutRow &row : kVertexLayouts) {
    if (row.Carries != wanted) { continue; }
    out = row.Layout;
    return true;
  }
  return false;
}

/* ONE DRAW: where its triangles are in the consumer's own index run, what surface it wears, and
 * where it sorts. `SourceFirstIndex` is what the consumer handed over; `FirstIndex` is where
 * `Compile` decided it goes, and before `Compile` it means nothing. */
struct DrawItem {
  DrawOrder Order;
  uint32_t SourceFirstIndex = 0;
  uint32_t IndexCount = 0;
  VertexLayout Layout = VertexLayout::Position;
  uint32_t FirstIndex = 0;
  /* **WHICH DRAW THIS WAS, so the compiled order is TOTAL and the sort needs no stability**
   * (board:1463). Two draws that agree on every field of the key have to land in one order and not
   * either, or the same declaration draws two pictures wherever they overlap. Carrying the ordinal
   * here says that in the DATA; leaving it to `std::stable_sort` said it in the ALGORITHM, and paid
   * for it with a temporary buffer on the frame path -- which is the one thing a frame path may not
   * do. `Add` assigns it and nobody else touches it. */
  uint32_t Submitted = 0;
};

/* WHAT ONE `DrawIndexed` COVERS after merging: a contiguous index run drawn with one pipeline and
 * one material. Two draws merge when they agree on everything the encoder would otherwise have to
 * change between them, which is exactly what the key's material field groups. */
struct DrawBatch {
  uint32_t FirstIndex = 0;
  uint32_t IndexCount = 0;
  uint32_t MaterialSlot = 0;
  VertexLayout Layout = VertexLayout::Position;
  SurfaceKind Kind = SurfaceKind::Opaque;
  /* How many draws were merged into this one call. 1 means nothing merged; it is published because a
   * batching claim nobody counts is a claim. */
  uint32_t Draws = 1;
};

/* Where one draw's indices are to be copied from, in compiled order. Walking this and appending
 * `Count` indices from `SourceFirst` builds exactly the run `Batches()` addresses. */
struct IndexRun {
  uint32_t SourceFirst = 0;
  uint32_t Count = 0;
};

class DrawList {
public:
  /* Refuses a material slot the key cannot address and a draw of no triangles, naming both numbers.
   * A draw that cannot be ordered is not a draw that is quietly dropped. */
  [[nodiscard]] bool Add(const DrawItem &item, std::string &error);

  void Clear();

  /* Sorts by key, assigns each draw its place in the index run, and merges what may be merged. */
  void Compile();

  [[nodiscard]] const std::vector<DrawItem> &Draws() const { return Draws_; }
  [[nodiscard]] const std::vector<IndexRun> &Runs() const { return Runs_; }
  [[nodiscard]] const std::vector<DrawBatch> &Batches() const { return Batches_; }
  /* The length of the index run `Compile` laid out, which is the sum of every draw's own. */
  [[nodiscard]] uint32_t IndexCount() const { return IndexCount_; }
  [[nodiscard]] bool Empty() const { return Draws_.empty(); }

private:
  std::vector<DrawItem> Draws_;
  std::vector<IndexRun> Runs_;
  std::vector<DrawBatch> Batches_;
  uint32_t IndexCount_ = 0;
};

} // namespace outshine::Render
#endif
