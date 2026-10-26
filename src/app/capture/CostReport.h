#ifndef OUTSHINE_APP_CAPTURE_COSTREPORT_H
#define OUTSHINE_APP_CAPTURE_COSTREPORT_H

#include <span>
#include <string_view>
#include <DiagnosticSample.h>

namespace outshine::App {
void PrintCostReport(std::string_view scene, std::span<const DiagnosticSample> samples);
void PrintGpuRow(std::string_view scene, std::span<const DiagnosticSample> samples);
}
#endif
