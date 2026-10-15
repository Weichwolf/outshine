#include <fenv.h>

typedef void *(*UnaryGeometry)(void *, const void *);

int outshine_fenv_mask(int index) {
  switch (index) {
    case 0: return FE_ALL_EXCEPT;
    case 1: return FE_DIVBYZERO;
    case 2: return FE_INVALID;
    case 3: return FE_OVERFLOW;
    case 4: return FE_UNDERFLOW;
    case 5: return FE_INEXACT;
    default: return -1;
  }
}

int outshine_fenv_flags(void) {
  return fetestexcept(FE_ALL_EXCEPT);
}

int outshine_fenv_rounding(void) {
  return fegetround();
}

int outshine_fenv_control(void) {
  fenv_t original;
  if (feholdexcept(&original) != 0) { return -1; }
  const int raised = feraiseexcept(FE_DIVBYZERO | FE_INVALID);
  const int flags = fetestexcept(FE_ALL_EXCEPT);
  const int restored = fesetenv(&original);
  return raised == 0 && restored == 0 ? flags : -2;
}

int outshine_fenv_geometry(UnaryGeometry operation,
                           void *context,
                           const void *geometry,
                           void **result) {
  if (!operation || !context || !geometry || !result) { return -1; }
  if (feclearexcept(FE_ALL_EXCEPT) != 0) { return -2; }
  *result = operation(context, geometry);
  return fetestexcept(FE_ALL_EXCEPT);
}
