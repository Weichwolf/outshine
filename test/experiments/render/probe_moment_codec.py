"""Numeric #824 alternatives through the unchanged four-buffer native consumer.

This is channel arithmetic followed by unchanged product GGX, not hardware
texture sampling or a new native image format. Nonfinite BRDF cases are retained.
"""

import argparse
import json
import os
import resource
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from material_filter import direction
from moment_codec import MODES, decode, encode, fixtures
from probe_material_filter import SHADER as BRDF_SHADER
from probe_material_filter import run, sha

DECODE_SHADER = """#version 450
layout(local_size_x=64) in;
layout(std430,set=0,binding=0) readonly buffer Palette { uint words[]; };
struct Probe { vec4 PointDx; vec2 Dy; int Which; uint Mode; };
layout(std430,set=0,binding=1) readonly buffer Inputs { Probe probes[]; };
struct Result { vec4 Fields; vec4 Control; };
layout(std430,set=1,binding=0) writeonly buffer Outputs { Result results[]; };
layout(std140,set=2,binding=0) uniform Pushed { uint count; uint unmapped; uvec2 padding; };
vec4 data(uint at) { return vec4(uintBitsToFloat(words[at]),uintBitsToFloat(words[at+1]),
                               uintBitsToFloat(words[at+2]),uintBitsToFloat(words[at+3])); }
void main() {
  uint index=gl_GlobalInvocationID.x;
  if(index>=count) { return; }
  uint at=64+48*uint(probes[index].Which), mode=words[at];
  vec4 filtered=vec4(0.0); float otherOrder=0.0, total=0.0;
  for(uint i=0;i<8;++i) {
    uint pixel=at+8+4*i;
    vec4 v;
    if(mode==4) {
      vec2 rg=unpackHalf2x16(words[pixel]),ba=unpackHalf2x16(words[pixel+1]);
      v=vec4(rg,ba);
    } else if(mode==3) {
      v=vec4(0.0,unpackHalf2x16(words[pixel]).x,
             uintBitsToFloat(words[pixel+2]),uintBitsToFloat(words[pixel+3]));
    } else { v=data(pixel); }
    float w=uintBitsToFloat(words[at+40+i]);
    filtered+=w*v;
    otherOrder+=w*(mode==2 ? (256.0*v.g+v.r)/257.0 : v.g);
    total+=w;
  }
  float m=mode==2 ? (256.0*filtered.g+filtered.r)/257.0 : filtered.g;
  float a=filtered.a, ratio=clamp(m/max(a,1e-8),0.0,1.0);
  float r=pow(ratio,0.25), metal=clamp(filtered.b/max(a,1e-8),0.0,1.0);
  results[index]=Result(vec4(m,a,filtered.b,r),vec4(otherOrder,metal,total,float(a>0)));
}
"""


def capture(payload, shader, out, binary, includes=None):
    out.mkdir(parents=True, exist_ok=False)
    count = len(payload)
    prefix = np.zeros(64, dtype="<u4")
    prefix[0] = 1
    np.concatenate((prefix, payload.view("<u4").ravel())).tofile(out / "palette.f32")
    probes = np.zeros((count, 8), dtype="<u4")
    probes[:, 6] = np.arange(count)
    probes.tofile(out / "probes.bin")
    (out / "probe.comp").write_text(shader)
    command = [
        "nice",
        "-n",
        "10",
        os.environ.get("GLSLANG", "glslangValidator"),
        "-V",
        "--target-env",
        "vulkan1.0",
    ]
    if includes is not None:
        command.append("-I" + str(includes))
    commands = [
        run(
            [*command, str(out / "probe.comp"), "-o", str(out / "probe.spv")],
            out,
            "compile",
        )
    ]
    commands.append(
        run(
            [
                str(binary),
                str(out / "probe.spv"),
                str(out / "palette.f32"),
                str(out / "probes.bin"),
                str(out / "native.f32"),
                "0",
            ],
            out,
            "native",
        )
    )
    values = np.fromfile(out / "native.f32", dtype="<f4").reshape(-1, 8)
    if len(values) != count:
        raise ValueError("missing native records")
    p = (out / "palette.f32").stat().st_size
    i = probes.nbytes
    return values, {
        "commands": commands,
        "palette_bytes": p,
        "input_output_bytes_each": i,
        "requested_buffer_bytes": 2 * p + 4 * i,
        "copied_bytes": p + 2 * i,
    }


def decode_controls(out, binary):
    fields, weights, metadata = fixtures()
    encoded = np.stack([encode(fields, m) for m in range(len(MODES))], axis=1)
    packets = np.zeros((len(fields), len(MODES), 48), dtype="<u4")
    packets[:, :, 0] = np.arange(len(MODES))
    packets[:, :, 8:40] = encoded.reshape(len(fields), len(MODES), 32)
    packets[:, :, 40:48] = (
        np.broadcast_to(weights[:, None, :], (len(fields), len(MODES), 8))
        .copy()
        .view("<u4")
    )
    values, cost = capture(
        packets.reshape(-1, 48), DECODE_SHADER, out / "decode", binary
    )
    values = values.reshape(len(fields), len(MODES), 8)
    if not np.isfinite(values).all():
        raise ValueError("nonfinite channel reconstruction")
    expected = np.stack(
        [
            (decode(encoded[:, m], m) * weights[:, :, None]).sum(1)
            for m in range(len(MODES))
        ],
        axis=1,
    )
    eps = np.finfo(np.float32).eps
    np.testing.assert_allclose(
        values[:, :, :3], expected, rtol=64 * eps, atol=np.finfo(np.float32).tiny
    )
    np.testing.assert_allclose(
        values[:, :, 0], values[:, :, 4], rtol=32 * eps, atol=np.finfo(np.float32).tiny
    )
    np.testing.assert_array_equal(values[:, 1, 1:3], values[:, 2, 1:3])
    np.testing.assert_array_equal(values[:, 1, 5], values[:, 2, 5])
    np.save(out / "fields.npy", fields, allow_pickle=False)
    np.save(out / "weights.npy", weights, allow_pickle=False)
    return (
        values,
        metadata,
        cost,
        {
            "source_reference": "unquantised Float32 premultiplied mip fields; CPU weighted in Float64",
            "channel_cpu_max_absolute": float(abs(values[:, :, :3] - expected).max()),
            "rg_commutation_max_absolute": float(
                abs(values[:, 2, 0] - values[:, 2, 4]).max()
            ),
            "rgba8_a_b_preserved_bit_exact": True,
        },
    )


def brdf_controls(decoded, metadata, out, binary, source_commit, tables):
    shader = out / "shader"
    shader.mkdir()
    for name in ("metalRoughBrdf.glsl", "microfacetEnergy.glsl"):
        (shader / name).write_bytes(
            subprocess.check_output(
                ["git", "show", source_commit + ":src/render/shaders/" + name]
            )
        )
    (shader / "brdfTables.glsl").write_bytes(tables.read_bytes())
    poses = [
        (view, offset)
        for view in (0.0, 35.0, 65.0)
        for offset in (-15.0, -5.0, -1.0, 0.0, 1.0, 5.0, 15.0)
    ]
    rows = np.zeros((len(decoded), len(MODES), len(poses), 20), dtype="<f4")
    rows[:, :, :, :3] = [0.4, 0.3, 0.2]
    rows[:, :, :, 3] = decoded[:, :, 3, None]
    rows[:, :, :, 4:7] = 0.04
    rows[:, :, :, 7] = 1
    rows[:, :, :, 10] = 1
    for j, (view, offset) in enumerate(poses):
        rows[:, :, j, 12:15] = direction(-view + offset)
        rows[:, :, j, 16:19] = direction(view)
    actual, cost = capture(
        rows.reshape(-1, 20), BRDF_SHADER, out / "brdf", binary, shader
    )
    actual = actual.reshape(len(decoded), len(MODES), len(poses), 8)
    rgb = actual[..., :3] + actual[..., 4:7]
    result = []
    for case, (record, desc) in enumerate(zip(rgb, metadata, strict=True)):
        ref = record[0]
        entry = dict(desc)
        for mode, name in enumerate(MODES):
            finite = np.isfinite(ref).all(1) & np.isfinite(record[mode]).all(1)
            delta = record[mode, finite].astype(float) - ref[finite]
            contribution = decoded[case, mode, 1] * record[mode, finite].astype(
                float
            ) - float(decoded[case, 0, 1]) * ref[finite].astype(float)
            entry[name] = {
                "finite_comparable_poses": int(finite.sum()),
                "nonfinite_poses": int((~np.isfinite(record[mode]).all(1)).sum()),
                "lost_opaque_coverage": bool(
                    decoded[case, 0, 1] > 0 and decoded[case, mode, 1] == 0
                ),
                "lost_positive_moment": bool(
                    decoded[case, 0, 0] > 0 and decoded[case, mode, 0] == 0
                ),
                "decoded_roughness": float(decoded[case, mode, 3]),
                "max_conditional_rgb_error_finite_only": float(abs(delta).max())
                if delta.size
                else None,
                "max_contribution_rgb_error_finite_only": float(abs(contribution).max())
                if contribution.size
                else None,
            }
        result.append(entry)
    return (
        result,
        cost,
        {
            "poses": poses,
            "nonfinite_native_responses": int((~np.isfinite(actual).all(-1)).sum()),
            "interpretation": "Nonfinite controls are retained, never counted as a successful finite error comparison. Opaque contribution only; no full ThinSheet/alpha-mask compositor.",
        },
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--tables", type=Path, required=True)
    parser.add_argument(
        "--source-commit", default="0eb2e739d009cf12df5855f9f688dd0169a6de1c"
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    binary = args.binary.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    out.mkdir(parents=True, exist_ok=False)
    decoded, metadata, decode_cost, controls = decode_controls(out, binary)
    cases, brdf_cost, brdf_status = brdf_controls(
        decoded, metadata, out, binary, args.source_commit, args.tables
    )
    report = {
        "scope": "Numeric channel reconstruction, then unchanged product direct GGX on actual native consumer. No texture/image filtering, native format, full atlas/normal/ThinSheet/mask/world/device budget admission.",
        "binary": str(binary),
        "binary_sha256": sha(binary),
        "source_commit": args.source_commit,
        "modes": MODES,
        "cases": cases,
        "decode_controls": controls,
        "brdf_status": brdf_status,
        "decode_cost": decode_cost,
        "brdf_cost": brdf_cost,
        "source_grid": "4x4 Float32 premultiplied source plus its 2x2 box mip; eight clamped samples with common bilinear/trilinear weights",
        "python_peak_rss_linux_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "code_sha256": {
            p.name: sha(p)
            for p in (
                Path(__file__),
                Path(__file__).with_name("moment_codec.py"),
                Path(__file__).with_name("probe_material_filter.py"),
            )
        },
        "input_table_sha256": sha(args.tables),
    }
    (out / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(
        "cases",
        len(cases),
        "native_nonfinite",
        brdf_status["nonfinite_native_responses"],
        flush=True,
    )


if __name__ == "__main__":
    main()
