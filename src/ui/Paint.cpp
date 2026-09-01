#include "Paint.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

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
  return {.X = left,
          .Y = top,
          .Width = std::fmax(0.0, right - left),
          .Height = std::fmax(0.0, bottom - top)};
}

bool Reaches(uint32_t colour) {
  return (colour & 0xFFu) != 0u;
}

size_t NextCodePoint(const std::string &text, size_t at, char32_t &code) {
  const auto lead = static_cast<unsigned char>(text[at]);
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
    code = (code << 6u) | (static_cast<unsigned char>(text[at + i]) & 0x3Fu);
  }
  return length;
}

class Painter {
public:
  Painter(
      const Layout &layout, const Font &font, std::vector<Quad> &into, size_t &beyond, double shift)
      : Boxes(layout.Boxes()), Face(font), Out(into), Beyond(beyond), Shift(shift) {}

  void Walk(int index, const Clip &clip, double opacity) {
    if (index < 0 || static_cast<size_t>(index) >= Boxes.size()) { return; }
    const Box &box = Boxes[static_cast<size_t>(index)];
    const double here = opacity * box.Opacity;

    if (!box.Text.empty()) {
      Glyphs(box, clip, here);
      return;
    }

    Add({.X = box.X,
         .Y = box.Y,
         .Width = box.Width,
         .Height = box.Height,
         .U0 = 0,
         .V0 = 0,
         .U1 = 0,
         .V1 = 0,
         .Colour = box.Background,
         .Radius = box.Radius,
         .Opacity = here,
         .ClipX = clip.X,
         .ClipY = clip.Y,
         .ClipWidth = clip.Width,
         .ClipHeight = clip.Height,
         .Node = box.Node});
    Edges(box, clip, here);

    Clip inner = clip;
    if (box.Clips) {
      inner =
          Intersected(clip,
                      {.X = box.X + box.Border.Left,
                       .Y = box.Y + box.Border.Top,
                       .Width = std::fmax(0.0, box.Width - box.Border.Left - box.Border.Right),
                       .Height = std::fmax(0.0, box.Height - box.Border.Top - box.Border.Bottom)});
    }
    for (const int child : box.Children) { Walk(child, inner, here); }
  }

private:
  void Add(Quad quad) {
    quad.Y -= Shift;
    quad.ClipY -= Shift;
    if (quad.Width <= 0 || quad.Height <= 0 || quad.ClipWidth <= 0 || quad.ClipHeight <= 0) {
      return;
    }

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

  void Edges(const Box &box, const Clip &clip, double opacity) {
    const uint32_t colour = box.BorderColour;
    const double right = box.X + box.Width;
    const double bottom = box.Y + box.Height;
    const auto edge = [&](double x, double y, double w, double h) {
      Add({.X = x,
           .Y = y,
           .Width = w,
           .Height = h,
           .U0 = 0,
           .V0 = 0,
           .U1 = 0,
           .V1 = 0,
           .Colour = colour,
           .Radius = 0,
           .Opacity = opacity,
           .ClipX = clip.X,
           .ClipY = clip.Y,
           .ClipWidth = clip.Width,
           .ClipHeight = clip.Height,
           .Node = box.Node});
    };
    edge(box.X, box.Y, box.Width, box.Border.Top);
    edge(box.X, bottom - box.Border.Bottom, box.Width, box.Border.Bottom);
    edge(box.X,
         box.Y + box.Border.Top,
         box.Border.Left,
         box.Height - box.Border.Top - box.Border.Bottom);
    edge(right - box.Border.Right,
         box.Y + box.Border.Top,
         box.Border.Right,
         box.Height - box.Border.Top - box.Border.Bottom);
  }

  void Glyphs(const Box &run, const Clip &clip, double opacity) {
    const FontMetrics metrics = Face.At(run.FontSize, run.Face);

    const double leading = (run.Height - (metrics.Ascent + metrics.Descent)) / 2.0;
    double pen = run.X;
    for (size_t at = 0; at < run.Text.size();) {
      char32_t code = 0;
      at += NextCodePoint(run.Text, at, code);
      const Glyph glyph = Face.Shape(code, run.FontSize, run.Face);
      if (glyph.Drawn) {
        Add({.X = pen + glyph.LeftPx,
             .Y = run.Y + leading + glyph.TopPx,
             .Width = glyph.WidthPx,
             .Height = glyph.HeightPx,
             .U0 = glyph.U0,
             .V0 = glyph.V0,
             .U1 = glyph.U1,
             .V1 = glyph.V1,
             .Colour = run.Colour,
             .Radius = 0,
             .Opacity = opacity,
             .ClipX = clip.X,
             .ClipY = clip.Y,
             .ClipWidth = clip.Width,
             .ClipHeight = clip.Height,
             .Node = run.Node});
      }
      pen += glyph.AdvancePx > 0 ? glyph.AdvancePx : metrics.Advance;
    }
  }

  const std::vector<Box> &Boxes;
  const Font &Face;
  std::vector<Quad> &Out;
  size_t &Beyond;
  double Shift = 0;
};

} // namespace

std::vector<double>
PageBreaks(const Layout &layout, double pageHeightPx, size_t &linesTallerThanThePage) {
  linesTallerThanThePage = 0;
  std::vector<double> starts{0.0};
  if (pageHeightPx <= 0) { return starts; }

  std::vector<const Box *> lines;
  for (const Box &box : layout.Boxes()) {
    if (!box.Text.empty()) { lines.push_back(&box); }
  }
  std::ranges::sort(lines, [](const Box *a, const Box *b) { return a->Y < b->Y; });

  double start = 0;
  for (const Box *line : lines) {
    if (line->Y < start) { continue; }
    if (line->Y + line->Height <= start + pageHeightPx) { continue; }
    if (line->Y <= start) {
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

  const double window = page.HeightPx > 0 ? page.HeightPx : layout.ViewportHeight();
  Painter painter(layout, font, Quads_, Beyond_, page.OffsetY);
  for (int at = 0; std::cmp_less(at, layout.Boxes().size()); ++at) {
    if (layout.Boxes()[static_cast<size_t>(at)].Parent != -1) { continue; }
    painter.Walk(
        at, {.X = 0, .Y = page.OffsetY, .Width = layout.ViewportWidth(), .Height = window}, 1.0);
  }
  return true;
}

} // namespace outshine::Ui
