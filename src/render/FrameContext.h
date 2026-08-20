#ifndef FRAMECONTEXT_H
#define FRAMECONTEXT_H

namespace outshine::Render {

struct FrameContext {

  double Eye[3];
  float Mvp16[16];

  double PrevEye[3];
  float PrevMvp16[16];
};

}
#endif
