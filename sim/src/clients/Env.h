#ifndef ENV_H
#define ENV_H

#include <cstdlib>
#include <string>

namespace outshine::Clients {

/* WHAT A DECLARATION CANNOT KNOW. Everything about the world, the camera and the recording is in
 * the mod; what is left is where THIS MACHINE keeps things — the tile server's address, the mod
 * root, the artifact root, the collector's address, the build's hash. Each one differs between a
 * laptop, a container and a browser while the scene stays the same word for word, which is exactly
 * the test for "is this environment". doc/build-and-ops.md lists them and justifies each. */
inline std::string Env(const char *name, const char *fallback) {
  const char *v = getenv(name);
  return v && *v ? std::string(v) : std::string(fallback);
}

} // namespace outshine::Clients
#endif
