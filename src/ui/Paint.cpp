#include "Paint.h"

#include <algorithm>
#include <cmath>

namespace outshine::Ui {
namespace {

struct Clip {
  double X = 0, Y = 0, Width = 0, Height = 0;
};

Clip Intersected(const Clip &a, const Clip &b) {
  const double left = std::fmax(a.X, b.X);
  const double top = std::fmax(a.Y, b.Y);
  const double right = std::fmin(a.X + a.Width, b.X + b.Width);
  const double bottom = std::fmin(a.Y + a.Height, b.Y + b.Height);
  return {left, top, std::fmax(0.0, right - left), std::fmax(0.0, bottom - top)};
}

/* A COLOUR WITH NO ALPHA REACHES NO PIXEL, and asking the consumer to draw it anyway spends a quad on
 * nothing. This is the one place transparency is decided, so a later reader cannot find a second. */
bool Reaches(uint32_t colour) { return (colour & 0xFFu) != 0u; }

/* THE CODE POINTS OF A RUN, one at a time. The layout measured with a fixed advance, so what the
 * painter must agree with is the COUNT of glyphs -- decoding utf-8 here and counting bytes there
 * would place the second glyph of an accented word in the wrong column. */
size_t NextCodePoint(const std::string &text, size_t at, char32_t &code) {
  const unsigned char lead = (unsigned char)text[at];
  size_t length = 1;
  code = lead;
  if ((lead & 0xE0u) == 0xC0u) {
    length = 2;
    code = lead & 0x1Fu;
  } else if ((lead & 0xF0u) == 0xE0u) {
    length = 3;
    code = lead & 0x0Fu;
  } else if ((lead & 0xF8u) == 0xF0u) {
    length = 4;
    code = lead & 0x07u;
  }
  if (at + length > text.size()) { return text.size() - at; }
  for (size_t i = 1; i < length; ++i) {
    code = (code << 6) | ((unsigned char)text[at + i] & 0x3Fu);
  }
  return length;
}

class Painter {
public:
  Painter(const Layout &layout, const Font &font, std::vector<Quad> &into, size_t &beyond,
          double shift)
      : Boxes(layout.Boxes()), Face(font), Out(into), Beyond(beyond), Shift(shift) {}

  void Walk(int index, const Clip &clip, double opacity) {
    if (index < 0 || (size_t)index >= Boxes.size()) { return; }
    const Box &box = Boxes[(size_t)index];
    const double here = opacity * box.Opacity;

    if (!box.Text.empty()) {
      Glyphs(box, clip, here);
      return;
    }

    /* THE BACKGROUND COVERS THE BORDER BOX, WHICH IS CSS'S OWN DEFAULT and not a simplification: the
     * border is painted OVER it, so a translucent border shows the background behind itself. */
    Add({box.X, box.Y, box.Width, box.Height, 0, 0, 0, 0, box.Background, box.Radius, here, clip.X,
         clip.Y, clip.Width, clip.Height, box.Node});
    Edges(box, clip, here);

    /* A CLIPPING BOX BOUNDS WHAT IS INSIDE IT AND NOT ITSELF -- its own border is what the clip is
     * drawn against, so clipping the box by itself would eat its own frame. */
    Clip inner = clip;
    if (box.Clips) {
      inner = Intersected(clip, {box.X + box.Border.Left, box.Y + box.Border.Top,
                                 std::fmax(0.0, box.Width - box.Border.Left - box.Border.Right),
                                 std::fmax(0.0, box.Height - box.Border.Top - box.Border.Bottom)});
    }
    for (const int child : box.Children) { Walk(child, inner, here); }
  }

private:
  void Add(Quad quad) {
    /* THE PAGE'S OFFSET IS TAKEN OFF HERE AND NOWHERE ELSE, so page four lands in the rectangle page
     * one did and a consumer needs no transform of its own. */
    quad.Y -= Shift;
    quad.ClipY -= Shift;
    if (quad.Width <= 0 || quad.Height <= 0 || quad.ClipWidth <= 0 || quad.ClipHeight <= 0) { return; }
    /* A QUAD ITS CLIP EXCLUDES ENTIRELY IS NOT EMITTED. The clip still TRAVELS, because a quad the
     * clip cuts in half must be cut by whoever draws it -- but one that reaches no pixel at all is a
     * rectangle on the frame path doing nothing, and this is the layer that can tell. It is what makes
     * a page of a long document cost the page rather than the document. */
    if (quad.X >= quad.ClipX + quad.ClipWidth || quad.X + quad.Width <= quad.ClipX ||
        quad.Y >= quad.ClipY + quad.ClipHeight || quad.Y + quad.Height <= quad.ClipY) {
      return;
    }
    if (!Reaches(quad.Colour)) { return; }
    if (Out.size() >= kQuadBound) {
      ++Beyond;
      return;
    }
    Out.push_back(quad);
  }

  /* FOUR EDGES AND NOT ONE FRAME, because the four widths are four declarations and a single rectangle
   * behind the background cannot spell a border that is thick on one side. */
  void Edges(const Box &box, const Clip &clip, double opacity) {
    const uint32_t colour = box.BorderColour;
    const double right = box.X + box.Width, bottom = box.Y + box.Height;
    const auto edge = [&](double x, double y, double w, double h) {
      Add({x, y, w, h, 0, 0, 0, 0, colour, 0, opacity, clip.X, clip.Y, clip.Width, clip.Height,
           box.Node});
    };
    edge(box.X, box.Y, box.Width, box.Border.Top);
    edge(box.X, bottom - box.Border.Bottom, box.Width, box.Border.Bottom);
    edge(box.X, box.Y + box.Border.Top, box.Border.Left,
         box.Height - box.Border.Top - box.Border.Bottom);
    edge(right - box.Border.Right, box.Y + box.Border.Top, box.Border.Right,
         box.Height - box.Border.Top - box.Border.Bottom);
  }

  /* ONE QUAD PER GLYPH THAT COVERS SOMETHING. The advance is the layout's, so the painter walks the
   * same columns the measurement did and a space costs no quad. */
  void Glyphs(const Box &run, const Clip &clip, double opacity) {
    const FontMetrics metrics = Face.At(run.FontSize);
    /* THE RUN'S BOX IS A LINE BOX AND THE BASELINE SITS INSIDE IT, so a glyph is placed from the
     * line's top plus the leading above the ascent -- half the difference between the line and the
     * face, which is what makes `line-height` move text rather than only move the box under it. */
    const double leading = (run.Height - (metrics.Ascent + metrics.Descent)) / 2.0;
    double pen = run.X;
    for (size_t at = 0; at < run.Text.size();) {
      char32_t code = 0;
      at += NextCodePoint(run.Text, at, code);
      const Glyph glyph = Face.Shape(code, run.FontSize);
      if (glyph.Drawn) {
        Add({pen + glyph.LeftPx, run.Y + leading + glyph.TopPx, glyph.WidthPx, glyph.HeightPx,
             glyph.U0, glyph.V0, glyph.U1, glyph.V1, run.Colour, 0, opacity, clip.X, clip.Y,
             clip.Width, clip.Height, run.Node});
      }
      pen += metrics.Advance;
    }
  }

  const std::vector<Box> &Boxes;
  const Font &Face;
  std::vector<Quad> &Out;
  size_t &Beyond;
  double Shift = 0;
};

} // namespace

std::vector<double> PageBreaks(const Layout &layout, double pageHeightPx,
                               size_t &linesTallerThanThePage) {
  linesTallerThanThePage = 0;
  std::vector<double> starts{0.0};
  if (pageHeightPx <= 0) { return starts; }

  std::vector<const Box *> lines;
  for (const Box &box : layout.Boxes()) {
    if (!box.Text.empty()) { lines.push_back(&box); }
  }
  std::sort(lines.begin(), lines.end(),
            [](const Box *a, const Box *b) { return a->Y < b->Y; });

  double start = 0;
  for (const Box *line : lines) {
    if (line->Y < start) { continue; }
    if (line->Y + line->Height <= start + pageHeightPx) { continue; }
    if (line->Y <= start) {
      /* THE LINE IS TALLER THAN THE PAGE. Breaking before it cannot advance, so it takes this page,
       * overflows it, and is counted -- a break that does not move is a loop with no bound, and
       * dropping the line is the other way to make it terminate. */
      ++linesTallerThanThePage;
      start = line->Y + line->Height;
    } else {
      start = line->Y;
    }
    starts.push_back(start);
  }
  return starts;
}

bool Painting::Build(const Layout &layout, const Font &font, std::string &error, const Page &page) {
  Quads_.clear();
  Beyond_ = 0;
  if (layout.Boxes().empty()) {
    error = "the layout holds no box, so there is nothing to paint and a painting of nothing would "
            "be indistinguishable from one that failed";
    return false;
  }
  /* THE PAGE IS A CLIP IN THE DOCUMENT'S OWN COORDINATES and a shift applied on the way out, which is
   * why a page needs no second layout: the document is laid out once and read through a window. */
  const double window = page.HeightPx > 0 ? page.HeightPx : layout.ViewportHeight();
  Painter painter(layout, font, Quads_, Beyond_, page.OffsetY);
  painter.Walk(0, {0, page.OffsetY, layout.ViewportWidth(), window}, 1.0);
  return true;
}

} // namespace outshine::Ui
