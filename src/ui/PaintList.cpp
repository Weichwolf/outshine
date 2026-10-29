#include "Utf8.h"
#include "PaintList.h"

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Ui {

constexpr uint32_t kByteMask = 0xFFu;

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
  return (colour & kByteMask) != 0u;
}

class Painter {
public:
  Painter(const Layout &layout, const Font &font, std::vector<Quad> &into)
      : Boxes(layout.Boxes()), Face(font), Out(into) {}

  [[nodiscard]] bool Overflowed() const { return Overflowed_; }

  void Walk(int index, const Clip &clip, double opacity) {
    if (Overflowed_ || index < 0 || static_cast<size_t>(index) >= Boxes.size()) { return; }
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
    for (const int child : box.Children) {
      Walk(child, inner, here);
      if (Overflowed_) { return; }
    }
  }

private:
  void Add(Quad quad) {
    if (quad.Width <= 0 || quad.Height <= 0 || quad.ClipWidth <= 0 || quad.ClipHeight <= 0) {
      return;
    }

    if (quad.X >= quad.ClipX + quad.ClipWidth || quad.X + quad.Width <= quad.ClipX ||
        quad.Y >= quad.ClipY + quad.ClipHeight || quad.Y + quad.Height <= quad.ClipY) {
      return;
    }
    if (!Reaches(quad.Colour)) { return; }
    if (Out.size() >= kQuadBound) {
      Overflowed_ = true;
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
    const FontMetrics metrics = Face.At(FontFace{.Name = run.Face, .SizePx = run.FontSize});

    const double leading = (run.Height - (metrics.Ascent + metrics.Descent)) / 2.0;
    double pen = run.X;
    for (size_t at = 0; at < run.Text.size() && !Overflowed_;) {
      char32_t code = 0;
      at += ReadUtf8(run.Text, at, code);
      const Glyph glyph = Face.Shape(code, FontFace{.Name = run.Face, .SizePx = run.FontSize});
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
  bool Overflowed_ = false;
};

}

bool PaintList::Build(const Layout &layout, const Font &font, std::string &error) {
  Quads_.clear();
  if (layout.Boxes().empty()) {
    error = "the layout holds no box, so there is nothing to paint and a painting of nothing would "
            "be indistinguishable from one that failed";
    return false;
  }

  Painter painter(layout, font, Quads_);
  for (int at = 0; std::cmp_less(at, layout.Boxes().size()); ++at) {
    if (layout.Boxes()[static_cast<size_t>(at)].Parent != -1) { continue; }
    painter.Walk(
        at,
        {.X = 0, .Y = 0, .Width = layout.ViewportWidth(), .Height = layout.ViewportHeight()},
        1.0);
    if (painter.Overflowed()) {
      Quads_.clear();
      error = "UI paint exceeds its quad budget";
      return false;
    }
  }
  return true;
}

}
