#ifndef OUTSHINE_DIAGNOSTICS_DIAGNOSTICSAMPLE_H
#define OUTSHINE_DIAGNOSTICS_DIAGNOSTICSAMPLE_H

#include <string>

namespace outshine {

/// Owned named diagnostic value with an explicit display unit.
struct DiagnosticSample {
  std::string Name;   ///< Human-readable metric name; not a stable identifier.
  double Value = 0.0; ///< Numeric sample interpreted in Unit.
  std::string Unit;   ///< Display unit; no automatic conversion.
};

}

#endif
