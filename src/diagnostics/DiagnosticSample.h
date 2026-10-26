#ifndef OUTSHINE_DIAGNOSTICS_DIAGNOSTICSAMPLE_H
#define OUTSHINE_DIAGNOSTICS_DIAGNOSTICSAMPLE_H

#include <string>

namespace outshine {

struct DiagnosticSample {
  std::string Name;
  double Value = 0.0;
  std::string Unit;
};

}

#endif
