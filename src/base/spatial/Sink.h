#ifndef OUTSHINE_BASE_SPATIAL_SINK_H
#define OUTSHINE_BASE_SPATIAL_SINK_H

#include <cstddef>
#include <string>

namespace outshine {

class Sink {
public:
  virtual ~Sink() = default;
  virtual void Number(const char *what, double value, const char *unit) = 0;
  virtual void Claim(bool held, const char *why) = 0;
  virtual void Near(double got, double want, double within, const char *unit, const char *why) = 0;
  virtual void Say(const std::string &line) = 0;

  virtual void Refuse(const std::string &why) { Say("REFUSED " + why); }
};

inline std::string Line(const char *shape, const std::string &one) {
  std::string out = shape;
  const size_t at = out.find("%s");
  if (at != std::string::npos) { out.replace(at, 2, one); }
  return out;
}

inline std::string Line(const char *shape, const char *one) {
  return Line(shape, std::string(one));
}

}

#endif
