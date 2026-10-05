#ifndef OUTSHINE_CLIENT_COSTREPORT_H
#define OUTSHINE_CLIENT_COSTREPORT_H

#include <span>
#include <string_view>
#include <diagnostics/DiagnosticSample.h>

namespace outshine::Client {
void PrintCostReport(std::string_view scene, std::span<const DiagnosticSample> samples);
}
#endif
