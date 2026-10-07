#include "CostReport.h"

#include <print>
#include <span>
#include <string_view>
#include <string>

namespace outshine::Client {
namespace {

constexpr double kBytesPerMiB = 1024.0 * 1024.0;

double Value(std::span<const DiagnosticSample> samples, std::string_view name) {
  for (const auto &sample : samples) {
    if (sample.Name == name) { return sample.Value; }
  }
  return 0.0;
}

void Group(std::string_view scene,
           std::span<const DiagnosticSample> samples,
           std::string_view prefix,
           double divisor) {
  std::print("PERF {} {} lifetime ms({}):",
             scene,
             prefix == "cost.stage." ? "stage CPU-encode" : prefix.substr(5, prefix.size() - 6),
             divisor > 0.0 ? "mean/max" : "total/max/calls");
  for (const auto &sample : samples) {
    if (!sample.Name.starts_with(prefix) || !sample.Name.ends_with(".total_ms") ||
        sample.Value == 0.0) {
      continue;
    }
    const auto base =
        sample.Name.substr(0, sample.Name.size() - std::string_view(".total_ms").size());
    const double count = divisor > 0.0 ? divisor : Value(samples, base + ".calls");
    const double value = divisor > 0.0 ? sample.Value / divisor : sample.Value;
    std::print(" {}={:.3f}/{:.3f}",
               std::string_view(base).substr(prefix.size()),
               value,
               Value(samples, base + ".max_ms"));
    if (divisor <= 0.0) { std::print("/{:.0f}", count); }
  }
  std::println("");
}

}

void PrintCostReport(std::string_view scene, std::span<const DiagnosticSample> samples) {
  const auto get = [samples](std::string_view key) { return Value(samples, key); };
  if (get("cost.render.frames") == 0.0) { return; }
  std::println("PERF {} native network: hit={:.0f} decoded={:.2f}MiB worker={:.1f}ms",
               scene,
               get("network: native cache hit"),
               get("network: native asset bytes") / kBytesPerMiB,
               get("network: worker elapsed"));
  for (const auto *const kind :
       {"terrain", "terrain_deformed", "buildings", "building_lod", "prototypes"}) {
    const std::string prefix = std::string("cost.assets.") + kind;
    std::println(
        "PERF {} native {}: hit/resident/miss/write={:.0f}/{:.0f}/{:.0f}/{:.0f} decoded={:.2f}MiB",
        scene,
        kind,
        get(prefix + ".hits"),
        get(prefix + ".resident"),
        get(prefix + ".misses"),
        get(prefix + ".writes"),
        get(prefix + ".read_bytes") / kBytesPerMiB);
  }
  std::println("PERF {} prototypes: generated={:.0f} worker={:.1f}ms read={:.1f}ms write={:.1f}ms",
               scene,
               get("cost.assets.prototypes.generated"),
               get("cost.assets.prototypes.generation_ms"),
               get("cost.assets.prototypes.read_ms"),
               get("cost.assets.prototypes.write_ms"));
  std::println(
      "PERF {} cache lifetime: read={:.1f}ms/{:.2f}MiB/{:.0f}calls write={:.1f}ms/{:.2f}MiB "
      "hits/miss={:.0f}/{:.0f} decoded={:.0f}/{:.0f} evict={:.0f}",
      scene,
      get("cost.cache.read_ms"),
      get("cost.cache.read_bytes") / kBytesPerMiB,
      get("cost.cache.read_calls"),
      get("cost.cache.write_ms"),
      get("cost.cache.write_bytes") / kBytesPerMiB,
      get("cost.cache.hits"),
      get("cost.cache.misses"),
      get("cost.cache.decoded_hits"),
      get("cost.cache.decoded_reads"),
      get("cost.cache.decoded_evictions"));
  std::print("PERF {} provider lifetime call_ms/calls:", scene);
  for (const auto &sample : samples) {
    if (!sample.Name.starts_with("cost.provider.") || !sample.Name.ends_with(".call_ms")) {
      continue;
    }
    const auto base =
        sample.Name.substr(0, sample.Name.size() - std::string_view(".call_ms").size());
    const double calls = get(base + ".calls");
    if (calls == 0.0) { continue; }
    std::print(" {}={:.2f}/{:.0f}", std::string_view(base).substr(14), sample.Value, calls);
  }
  std::println("");
  std::println("PERF {} worker lifetime ms/attempts: field={:.1f}/{:.0f} mesh={:.1f}/{:.0f}; "
               "fetch service={:.1f}ms blocked={:.1f}ms (overlaps)",
               scene,
               get("cost.field.work_ms"),
               get("cost.field.attempts"),
               get("cost.mesh.work_ms"),
               get("cost.mesh.attempts"),
               get("cost.fetch.service_ms"),
               get("cost.fetch.blocked_ms"));
  std::println("PERF {} osm lifetime ms: fetch={:.1f} parse={:.1f} capacity={:.1f} publish={:.1f}",
               scene,
               get("cost.osm.fetch_ms"),
               get("cost.osm.parse_ms"),
               get("cost.osm.capacity_ms"),
               get("cost.osm.publish_ms"));
  std::println(
      "PERF {} structures lifetime: work={:.1f}ms products={:.0f} ranges={:.0f} max_range={:.2f}ms",
      scene,
      get("cost.structures.work_ms"),
      get("cost.structures.products"),
      get("cost.structures.ranges"),
      get("cost.structures.max_range_ms"));
  Group(scene, samples, "cost.ground.", 0.0);
  const double frames = get("cost.render.frames");
  std::print("PERF {} host {}frames ms(mean/worst-frame):", scene, frames);
  for (const auto &sample : samples) {
    if (!sample.Name.starts_with("cost.host.") || !sample.Name.ends_with(".total_ms")) { continue; }
    const auto base =
        sample.Name.substr(0, sample.Name.size() - std::string_view(".total_ms").size());
    std::print(" {}={:.3f}/{:.3f}",
               std::string_view(base).substr(10),
               sample.Value / frames,
               get(base + ".worst_ms"));
  }
  std::println("; gpu_ms=unavailable(SDL_GPU timestamps)");
  Group(scene, samples, "cost.stage.", frames);
  std::println("PERF {} diagnostics lifetime: publication={:.2f}ms max={:.3f}ms; "
               "parents include children; output after measurement",
               scene,
               get("cost.diagnostics.total_ms"),
               get("cost.diagnostics.max_ms"));
}

}
