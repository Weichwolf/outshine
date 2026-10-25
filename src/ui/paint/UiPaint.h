#ifndef OUTSHINE_UI_PAINT_UIPAINT_H
#define OUTSHINE_UI_PAINT_UIPAINT_H

#include <cstdint>

namespace outshine::Ui {

struct PaintQuad {
  float LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  float U0 = 0, V0 = 0, U1 = 0, V1 = 0;
  float Red = 1, Green = 1, Blue = 1, Alpha = 1;
  float ClipLeftPx = 0, ClipTopPx = 0, ClipWidthPx = 0, ClipHeightPx = 0;
  float RadiusPx = 0, Opacity = 1, Pad0 = 0, Pad1 = 0;
};

struct PaintAtlasView {
  const uint8_t *Rgba = nullptr;
  int Width = 0;
  int Height = 0;
};

}

#endif
