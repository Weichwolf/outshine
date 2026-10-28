"""Compare the shared lookup against the prior product on finite GGX boundaries."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from probe_moment_codec import capture


SHADER = """#version 450
#extension GL_GOOGLE_include_directive : require
layout(local_size_x=64) in;
layout(std430,set=0,binding=0) readonly buffer Palette { float words[]; };
struct Probe { vec4 PointDx; vec2 Dy; int Which; uint Mode; };
layout(std430,set=0,binding=1) readonly buffer Inputs { Probe probes[]; };
struct Result { vec4 Scale; vec4 Environment; };
layout(std430,set=1,binding=0) writeonly buffer Outputs { Result results[]; };
layout(std140,set=2,binding=0) uniform Pushed { uint count; uint unmapped; uvec2 padding; };
#include "brdfTables.glsl"
#include "microfacetEnergy.glsl"
void main() {
  uint index=gl_GlobalInvocationID.x;
  if(index>=count) { return; }
  uint at=64+20*uint(probes[index].Which);
  float nv=words[at], roughness=words[at+1], materialWeight=words[at+2];
  bool thinSheet=materialWeight<=0.0;
  vec3 f0=vec3(words[at+4],words[at+5],words[at+6]);
  ENERGY_PATH
  results[index]=Result(vec4(scale, environmentBrdf.x),
      vec4(environmentBrdf.y, materialWeight*scale.x, injected, float(thinSheet)));
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--tables", type=Path, required=True)
    parser.add_argument("--baseline-commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    views = (np.nextafter(np.float32(0), np.float32(-1)), 0, 1e-6, 0.5,
             1, np.nextafter(np.float32(1), np.float32(2)))
    for nv in views:
        for roughness in (0, 1e-6, 0.5, 1):
            for weight in (0, 0.25, 1):
                for injected in (-1e-6, 0, 1, 1.000001):
                    row = np.zeros(20, dtype="<f4")
                    row[:7] = (nv, roughness, weight, injected, 0, 0.04, 1)
                    rows.append(row)
    payload = np.asarray(rows, dtype="<f4")
    np.save(args.output / "inputs.npy", payload, allow_pickle=False)
    results, receipts, sources = {}, {}, {}
    for name in ("baseline", "shared"):
        includes = args.output / (name + "-source")
        includes.mkdir()
        source = (subprocess.check_output(
            ["git", "show", args.baseline_commit + ":src/render/shaders/microfacetEnergy.glsl"],
            cwd=root) if name == "baseline" else
            (root / "src/render/shaders/microfacetEnergy.glsl").read_bytes())
        (includes / "microfacetEnergy.glsl").write_bytes(source)
        (includes / "brdfTables.glsl").write_bytes(args.tables.read_bytes())
        sources[name] = hashlib.sha256(source).hexdigest()
        if name == "baseline":
            path = """vec3 scale=thinSheet ? vec3(0) : ggxEnergyScale(f0,roughness,nv);
  vec2 environmentBrdf=thinSheet ? vec2(0) : ggxEnvironmentBrdf(clamp(nv,0,1),roughness);
  float injected=1.0;"""
        else:
            path = """vec2 environmentBrdf=thinSheet ? vec2(0) : ggxEnvironmentBrdf(nv,roughness);
  vec3 scale=thinSheet ? vec3(0) : ggxEnergyScaleFromAlbedo(f0,roughness,environmentBrdf.x);
  float injected=ggxEnergyScaleFromAlbedo(f0,roughness,words[at+3]).x;"""
        results[name], receipts[name] = capture(
            payload, SHADER.replace("ENERGY_PATH", path), args.output / name,
            args.binary, includes)
    old, new = results["baseline"], results["shared"]
    finite = bool(np.isfinite(old).all() and np.isfinite(new).all())
    exact = bool(np.array_equal(old, new))
    zero = payload[:, 2] == 0
    pure_sheet_zero = bool((new[zero, :6] == 0).all())
    injected_guard = bool((new[:, 6] == 1).all())
    report = {"cases": len(payload), "finite": finite, "exact": exact,
              "pure_sheet_zero": pure_sheet_zero, "injected_e_guard": injected_guard,
              "maximum_absolute_difference": float(np.abs(old-new).max()),
              "sources": sources, "tables_sha256": hashlib.sha256(args.tables.read_bytes()).hexdigest(),
              "baseline_commit": args.baseline_commit, "receipts": receipts}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k not in ("receipts", "sources")}))
    raise SystemExit(0 if finite and exact and pure_sheet_zero and injected_guard else 1)


if __name__ == "__main__":
    main()
