/* Build shim, force-included (emcc -include) ONLY for the JSBSim sources so the submodule stays
 * bit-vanilla: emscripten defines _GNU_SOURCE but musl ships the POSIX strerror_r, while simgear takes
 * the _GNU_SOURCE branch and expects the GNU `char*` return. The wrapper is defined BEFORE the macro so
 * its own call reaches the real libc function. */
#ifndef FB_EM_COMPAT_H
#define FB_EM_COMPAT_H
#ifdef __EMSCRIPTEN__
#include <string.h>
#include <stddef.h>
static inline char *fb_gnu_strerror_r(int e, char *b, size_t n) {
  (void)strerror_r(e, b, n);
  return b;
}
#define strerror_r(e, b, n) fb_gnu_strerror_r((e), (b), (n))
#endif /* __EMSCRIPTEN__ */
#endif /* FB_EM_COMPAT_H */
