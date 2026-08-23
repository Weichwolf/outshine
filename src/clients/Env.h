#ifndef OUTSHINE_CLIENTS_ENV_H
#define OUTSHINE_CLIENTS_ENV_H

#include <cstdlib>
#include <string>

namespace outshine::Clients {

inline std::string Env(const char *name, const char *fallback) {
  const char *v = getenv(name);
  return v && *v ? std::string(v) : std::string(fallback);
}

}
#endif
