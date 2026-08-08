/* THE PEDESTRIAN'S FRAME, native — the entry point and the output medium, and nothing else. What it
 * shows is Outshine's; how it is asked for is WalkBench's. */
#include "Log.h"
#include "LogSinks.h"
#include "WalkBench.h"

using namespace outshine;

int main(int argc, char **argv) {
  static Clients::StdoutLogSink sink;
  Log::SetSink(&sink);
  Log::SetLevel(LogLevel::Debug);

  Clients::WalkBench bench;
  if (!bench.Parse(argc, argv)) return 1;
  return bench.Run();
}
