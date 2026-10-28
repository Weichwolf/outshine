"""Evaluate unchanged production GGX functions in the existing native GPU probe.

Python owns the experiment. The private shader only binds probe IO to existing
product functions; it is not a material/cache/backend implementation. No native
consumer is built here. Without --binary the result is preparation only.
"""

import argparse
import hashlib
import json
import os
import resource
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from material_filter import direct_ggx, fixtures, read_tables

SHADER = """#version 450
#extension GL_GOOGLE_include_directive : require
layout(local_size_x=64) in;
layout(std430,set=0,binding=0) readonly buffer Palette { float palette[]; };
struct Probe { vec4 PointDx; vec2 Dy; int Which; uint Mode; };
layout(std430,set=0,binding=1) readonly buffer Inputs { Probe probes[]; };
struct Result { vec4 Diffuse; vec4 Specular; };
layout(std430,set=1,binding=0) writeonly buffer Outputs { Result results[]; };
layout(std140,set=2,binding=0) uniform Pushed { uint count; uint unmapped; uvec2 padding; };
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "brdfTables.glsl"
#include "microfacetEnergy.glsl"
vec3 value(uint offset) { return vec3(palette[offset],palette[offset+1],palette[offset+2]); }
void main() {
  uint index=gl_GlobalInvocationID.x;
  if(index>=count) { return; }
  uint at=64+20*uint(probes[index].Which);
  vec3 n=normalize(value(at+8)), l=normalize(value(at+12)), v=normalize(value(at+16));
  float nl=max(dot(n,l),0.0), nv=max(dot(n,v),1e-6);
  vec3 diffuse=vec3(0.0), specular=vec3(0.0);
  if(nl>0.0) {
    vec3 h=normalize(l+v);
    float roughness=palette[at+3], a=roughness*roughness;
    Brdf response=metalRoughBrdf(value(at),value(at+4),palette[at+7],a*a,
                               nl,nv,max(dot(n,h),0.0),max(dot(v,h),0.0));
    diffuse=response.diffuse*nl;
    specular=response.specular*probeEnergyScale(value(at+4),roughness,nv)*nl;
  }
  results[index]=Result(vec4(diffuse,nl),vec4(specular,nv));
}
"""


def with_energy_scale(shader, source):
    if "vec3 ggxEnergyScaleFromAlbedo(" in source:
        expression = "ggxEnergyScaleFromAlbedo(f0,r,ggxEnvironmentBrdf(nv,r).x)"
    elif "vec3 ggxEnergyScale(" in source:
        expression = "ggxEnergyScale(f0,r,nv)"
    else:
        raise ValueError("the selected product source has no supported GGX energy contract")
    marker = '#include "microfacetEnergy.glsl"'
    if shader.count(marker) != 1:
        raise ValueError("the probe must include exactly one selected GGX energy source")
    helper = "\nvec3 probeEnergyScale(vec3 f0,float r,float nv) { return " + expression + "; }"
    return shader.replace(marker, marker + helper)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(argv, out, label):
    start = time.perf_counter()
    result = subprocess.run(
        argv, capture_output=True, text=True, timeout=90, check=False
    )
    log = result.stdout + result.stderr
    (out / (label + ".log")).write_text(log)
    if result.returncode or "Validation Error" in log or "VUID-" in log:
        raise RuntimeError(f"{label} failed ({result.returncode}); see log")
    return {
        "argv": argv,
        "exit": result.returncode,
        "wall_s": time.perf_counter() - start,
    }


def prepare(args, out):
    shader = out / "shader"
    shader.mkdir()
    sources = {}
    for name in ("metalRoughBrdf.glsl", "microfacetEnergy.glsl"):
        data = subprocess.check_output(
            ["git", "show", args.source_commit + ":src/render/shaders/" + name]
        )
        p = shader / name
        p.write_bytes(data)
        sources[name] = sha(p)
    (shader / "brdfTables.glsl").write_bytes(args.tables.read_bytes())
    sources["brdfTables.glsl"] = sha(args.tables)
    rows, descriptions = fixtures()
    flat = rows.reshape(-1, 20)
    np.save(out / "fields.npy", rows, allow_pickle=False)
    probes = np.zeros((len(flat), 8), dtype="<u4")
    probes[:, 6] = np.arange(len(flat))
    probes.tofile(out / "probes.bin")
    palette = np.zeros(64 + flat.size, dtype="<f4")
    palette.view("<u4")[0] = 1
    palette[64:] = flat.ravel()
    palette.tofile(out / "palette.f32")
    reference = direct_ggx(flat, read_tables(args.tables))
    if not np.isfinite(reference).all():
        raise ValueError("nonfinite CPU control")
    np.save(out / "reference.npy", reference, allow_pickle=False)
    (shader / "probe.comp").write_text(
        with_energy_scale(SHADER, (shader / "microfacetEnergy.glsl").read_text())
    )
    compile_result = run(
        [
            "nice",
            "-n",
            "10",
            os.environ.get("GLSLANG", "glslangValidator"),
            "-V",
            "--target-env",
            "vulkan1.0",
            "-I" + str(shader),
            str(shader / "probe.comp"),
            "-o",
            str(out / "probe.spv"),
        ],
        out,
        "compile",
    )
    return (
        rows,
        descriptions,
        {
            "status": "prepared_only",
            "gpu_executed": False,
            "scope": "Controlled opaque isotropic dielectric direct GGX with actual production energy tables. No visibility/cache change, environment/shadows/extensions/full lit world/device admission. Byte variant isolates roughness-moment quantisation, not all atlas channels.",
            "source_commit": args.source_commit,
            "sources_sha256": sources,
            "table_input": str(args.tables.resolve()),
            "cases": descriptions,
            "shape": list(rows.shape),
            "variants": [
                "fine_a",
                "fine_b",
                "mean",
                "ideal_atlas_moments",
                "byte_roughness_moment",
                "byte_perceptual_roughness",
            ],
            "compile": compile_result,
            "bytes": {
                "palette": palette.nbytes,
                "probes": probes.nbytes,
                "output": len(flat) * 32,
                "requested_probe_buffers": 2 * palette.nbytes + 4 * probes.nbytes,
                "copied_probe_bytes": palette.nbytes + 2 * probes.nbytes,
            },
        },
    )


def compare(actual, reference, rows, descriptions):
    values = actual.reshape(*rows.shape[:-1], 8)
    radiance = values[..., :3] + values[..., 4:7]
    errors, endpoints = [], 0
    for i, description in enumerate(descriptions):
        weight = description["weight"]
        fine = (1 - weight) * radiance[i, :, 0] + weight * radiance[i, :, 1]
        entry = dict(description)
        for j, name in (
            (2, "mean"),
            (3, "ideal_atlas_moments"),
            (4, "byte_roughness_moment"),
            (5, "byte_perceptual_roughness"),
        ):
            delta = radiance[i, :, j] - fine
            entry[name] = {
                "linear_rgb_rmse": float(np.sqrt(np.mean(delta**2))),
                "max_absolute_rgb": float(abs(delta).max()),
            }
        if weight in (0, 1):
            for j in (2, 3):
                np.testing.assert_array_equal(
                    values[i, :, j], values[i, :, int(weight)]
                )
                endpoints += 1
        if description["kind"] == "albedo":
            np.testing.assert_allclose(
                radiance[i, :, 2],
                fine,
                rtol=64 * np.finfo(np.float32).eps,
                atol=64 * np.finfo(np.float32).eps,
            )
        errors.append(entry)
    delta = actual.astype(float) - reference
    return {
        "cases": errors,
        "bit_exact_endpoint_controls": endpoints,
        "cpu_reference_max_absolute": float(abs(delta).max()),
        "cpu_reference_scaled_error_max": float(
            (abs(delta) / (1 + abs(reference))).max()
        ),
        "cpu_reference_scope": "Independent Float64 numerical control, not a GPU precision theorem or visual tolerance. All candidate errors compare GPU with GPU.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tables", type=Path, required=True)
    parser.add_argument(
        "--source-commit", default="0eb2e739d009cf12df5855f9f688dd0169a6de1c"
    )
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh explicit system-temp output required")
    out.mkdir(parents=True, exist_ok=False)
    rows, descriptions, report = prepare(args, out)
    if args.binary:
        report["consumer"] = {
            "path": str(args.binary.resolve()),
            "sha256": sha(args.binary),
        }
        report["execution"] = run(
            [
                str(args.binary.resolve()),
                str(out / "probe.spv"),
                str(out / "palette.f32"),
                str(out / "probes.bin"),
                str(out / "native.f32"),
                "0",
            ],
            out,
            "native",
        )
        actual = np.fromfile(out / "native.f32", dtype="<f4").reshape(-1, 8)
        if actual.shape != (rows.size // 20, 8) or not np.isfinite(actual).all():
            raise ValueError("invalid native output")
        report["comparison"] = compare(
            actual,
            np.load(out / "reference.npy", allow_pickle=False),
            rows,
            descriptions,
        )
        report.update(status="compared", gpu_executed=True)
    report["code_sha256"] = {
        p.name: sha(p)
        for p in (Path(__file__), Path(__file__).with_name("material_filter.py"))
    }
    report["process_peak_rss_linux_kib"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss
    report["files_sha256"] = {p.name: sha(p) for p in out.iterdir() if p.is_file()}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(report["status"], flush=True)


if __name__ == "__main__":
    main()
