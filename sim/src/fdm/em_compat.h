/* Build-time compatibility shim for compiling the pinned, read-only JSBSim submodule under
 * emscripten/musl. Force-included (emcc -include) ONLY for the JSBSim sources — JSBSim itself stays
 * bit-vanilla (CLAUDE.md Prinzip 1); this is glue at the seam, not a patch.
 *
 * strerror_r: emscripten defines _GNU_SOURCE (libc++ needs it for strtof_l/strtod_l), but musl ships
 * the POSIX `int strerror_r(int,char*,size_t)`, while simgear/misc/strutils.cxx takes the _GNU_SOURCE
 * branch and does `std::string(strerror_r(...))`, expecting the GNU `char*` return. Wrap it so the
 * call yields the buffer. The wrapper is defined BEFORE the macro, so its own call reaches the real
 * libc function (no recursion); every use after expands to the wrapper. Only the _GNU_SOURCE branch
 * compiles (that macro stays defined), so the POSIX `int retcode = strerror_r(...)` path is never hit.
 */
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
