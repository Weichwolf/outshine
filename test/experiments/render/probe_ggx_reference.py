"""Run unchanged product functions via the existing four-buffer native consumer."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from ggx_reference import CHANNELS, compare, fixtures, row_reference
from material_filter import energy_scale, read_tables
from probe_material_filter import sha
from probe_moment_codec import capture

SHADER = """#version 450
#extension GL_GOOGLE_include_directive : require
layout(local_size_x=64) in;
layout(std430,set=0,binding=0) readonly buffer Palette { float words[]; };
struct Probe { vec4 PointDx; vec2 Dy; int Which; uint Mode; };
layout(std430,set=0,binding=1) readonly buffer Inputs { Probe probes[]; };
struct Result { vec4 Factors; vec4 Terms; };
layout(std430,set=1,binding=0) writeonly buffer Outputs { Result results[]; };
layout(std140,set=2,binding=0) uniform Pushed { uint count; uint unmapped; uvec2 padding; };
const float kPi=acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "brdfTables.glsl"
#include "microfacetEnergy.glsl"
void main() {
  uint index=gl_GlobalInvocationID.x;
  if(index>=count) { return; }
  uint at=64+8*uint(probes[index].Which);
  float r=words[at], nh=words[at+1], nl=words[at+2], nv=words[at+3];
  float vh=words[at+4], f0=words[at+5], a=r*r;
  float q=words[at+7]>0.0 ? words[at+6] : a*a;
  float d=q>0.0 ? brdfDistribution(nh,q) : 0.0;
  float v=brdfVisibility(nl,nv,q), lobe=brdfLobe(q,nl,nv,nh);
  float e=ggxEnergyScale(vec3(f0),r,nv).x;
  Brdf response=metalRoughBrdf(vec3(0.0),vec3(f0),1.0,q,nl,nv,nh,vh);
  results[index]=Result(vec4(d,v,lobe,q),
      vec4(response.specular.x*e*nl,e,brdfFresnel(vec3(f0),1.0,vh).x,a));
}
"""


def shader_for(weighted_product=False):
    if not weighted_product:
        return SHADER
    return SHADER.replace(
        "Brdf response=metalRoughBrdf(vec3(0.0),vec3(f0),1.0,q,nl,nv,nh,vh);",
        "float specular=brdfWeightedSpecular(brdfFresnel(vec3(f0),1.0,vh),q,nl,nv,nh,nl).x*e;",
    ).replace("response.specular.x*e*nl", "specular")


def analyse(rows, groups, values, tables):
    reference, source_reference = [], []
    for row, value in zip(rows, values, strict=True):
        reference.append(row_reference(row, value[3], value[5]))
        source_reference.append(row_reference(row, energy=value[5]))
    reference = np.array(reference, dtype=float)
    source_reference = np.array(source_reference, dtype=float)
    actual = values[:, [0, 1, 2, 4]].astype(float)
    expected_energy = energy_scale(
        tables,
        rows[:, 0].astype(float),
        rows[:, 3].astype(float),
        np.repeat(rows[:, 5:6].astype(float), 3, axis=1),
    )[:, 0]
    np.testing.assert_allclose(
        values[:, 5], expected_energy, rtol=64 * np.finfo(np.float32).eps
    )
    expected_fresnel = (
        rows[:, 5].astype(float)
        + (1 - rows[:, 5].astype(float)) * (1 - rows[:, 4].astype(float)) ** 5
    )
    np.testing.assert_allclose(
        values[:, 6], expected_fresnel, rtol=16 * np.finfo(np.float32).eps
    )
    result = {
        "scope": "Scalar product functions; original energy table. Not texture/raster/world or device frame admission.",
        "error_budget": "64 Float32 eps relative for normal/zero reference outputs; arithmetic diagnostic, not image tolerance.",
        "cosine_rounding": "nh above one is evaluated against physical nh=1, grouped explicitly.",
        "channels": {
            name: compare(actual[:, i], reference[:, i])
            for i, name in enumerate(CHANNELS)
        },
        "exact_r4_channels": {
            name: compare(actual[:, i], source_reference[:, i])
            for i, name in enumerate(CHANNELS)
        },
        "moment_classes": {
            name: {
                channel: compare(actual[mask, i], reference[mask, i])
                for i, channel in enumerate(CHANNELS)
            }
            for name, mask in (
                ("normal", values[:, 3] >= np.finfo(np.float32).tiny),
                (
                    "subnormal",
                    (values[:, 3] > 0) & (values[:, 3] < np.finfo(np.float32).tiny),
                ),
                ("zero", values[:, 3] == 0),
            )
        },
        "groups": {
            group: {
                name: compare(actual[mask, i], reference[mask, i])
                for i, name in enumerate(CHANNELS)
            }
            for group in sorted(set(groups))
            if (mask := np.array(groups) == group).any()
        },
        "source_r4_became_zero": int(
            ((rows[:, 0] > 0) & (rows[:, 7] == 0) & (values[:, 3] == 0)).sum()
        ),
        "source_r4_nonzero_subnormal": int(
            ((values[:, 3] > 0) & (values[:, 3] < np.finfo(np.float32).tiny)).sum()
        ),
        "energy_cpu_max_absolute": float(abs(values[:, 5] - expected_energy).max()),
        "captured_a2_numeric_pass": all(
            compare(actual[:, i], reference[:, i])["bad_representable"] == 0
            for i in range(4)
        ),
        "exact_r4_numeric_pass": all(
            compare(actual[:, i], source_reference[:, i])["bad_representable"] == 0
            for i in range(4)
        ),
    }
    return result, reference, source_reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--tables", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--weighted-product",
        action="store_true",
        help="consume brdfWeightedSpecular with nl exactly once, as in the #829 fixed light path",
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    includes = args.output / "shader"
    includes.mkdir()
    for name in ("metalRoughBrdf.glsl", "microfacetEnergy.glsl"):
        (includes / name).write_bytes(
            subprocess.check_output(
                ["git", "show", args.source_commit + ":src/render/shaders/" + name]
            )
        )
    (includes / "brdfTables.glsl").write_bytes(args.tables.read_bytes())
    rows, groups = fixtures()
    np.save(args.output / "fields.npy", rows, allow_pickle=False)
    values, cost = capture(
        rows,
        shader_for(args.weighted_product),
        args.output / "native",
        args.binary,
        includes,
    )
    result, reference, source_reference = analyse(
        rows, groups, values, read_tables(args.tables)
    )
    np.save(args.output / "reference.npy", reference, allow_pickle=False)
    np.save(
        args.output / "exact-r4-reference.npy", source_reference, allow_pickle=False
    )
    result.update(
        {
            "source_commit": args.source_commit,
            "response_call": "brdfWeightedSpecular(F,q,nl,nv,nh,nl)*energy"
            if args.weighted_product
            else "metalRoughBrdf(...).specular*energy*nl",
            "consumer_sha256": sha(args.binary),
            "rows": len(rows),
            "groups_by_row": groups,
            "cost": cost,
            "sources_sha256": {p.name: sha(p) for p in sorted(includes.iterdir())},
            "artifacts_sha256": {
                str(p.relative_to(args.output)): sha(p)
                for p in sorted(args.output.rglob("*"))
                if p.is_file()
            },
        }
    )
    (args.output / "report.json").write_text(
        json.dumps(result, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                "rows": len(rows),
                "channels": {
                    name: {k: v for k, v in values.items() if k != "bad_indices"}
                    for name, values in result["channels"].items()
                },
                "source_r4_became_zero": result["source_r4_became_zero"],
                "captured_a2_numeric_pass": result["captured_a2_numeric_pass"],
                "exact_r4_numeric_pass": result["exact_r4_numeric_pass"],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
