"""Check production GGX through the existing native four-buffer consumer.

Float64 controls use the angular NDF and Smith lambda formulation. Raw input
denormals remain visible as separate accuracy losses, never as passing parity.
This is shader arithmetic, not an atlas, raster, device-budget or image gate.
"""

import argparse
import hashlib
import json
import math
import os
import struct
import subprocess
import tempfile
from dataclasses import asdict, dataclass, replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FLOAT_MIN = float.fromhex("0x1p-126")
FLOAT_MAX = float.fromhex("0x1.fffffep127")
RELATIVE_BOUND = 32 * 2**-23
LABELS = ("distribution", "visibility", "lobe", "specular_r", "r", "g", "b", "zero")
SHADER = """#version 450
#extension GL_GOOGLE_include_directive : require
layout(local_size_x=64) in;
layout(std430,set=0,binding=0) readonly buffer Palette { float words[]; };
struct Probe { vec4 PointDx; vec2 Dy; int Which; uint Mode; };
layout(std430,set=0,binding=1) readonly buffer Inputs { Probe probes[]; };
struct Sample { vec4 Values; vec4 Other; };
layout(std430,set=1,binding=0) writeonly buffer Outputs { Sample result[]; };
layout(std140,set=2,binding=0) uniform Counts {
  uint count; uint unmapped; uvec2 padding;
};
const float kPi = 3.14159265358979323846;
#include "metalRoughBrdf.glsl"
void main() {
  uint i=gl_GlobalInvocationID.x;
  if(i>=count) { return; }
  uint at=64u+12u*uint(probes[i].Which);
  float nh=words[at], a2=words[at+1], nl=words[at+2], nv=words[at+3];
  vec3 f0=vec3(words[at+4],words[at+5],words[at+6]);
  float d=a2>0.0?brdfDistribution(nh,a2):0.0;
  float v=a2>0.0?brdfVisibility(nl,nv,a2):0.0;
  float lobe=brdfLobe(a2,nl,nv,nh);
  Brdf brdf=metalRoughBrdf(vec3(0.0),f0,words[at+7],a2,nl,nv,nh,words[at+8]);
  if(words[at+9]>0.0) {
#ifdef GGX_COSINE_SPECULAR
    vec3 fresnel=brdfFresnel(f0,words[at+7],words[at+8]);
    brdf.specular=brdfWeightedSpecular(fresnel,a2,nl,nv,nh,nl);
#else
    brdf.specular*=nl;
#endif
  }
  result[i]=Sample(vec4(d,v,lobe,brdf.specular.r),vec4(brdf.specular,0.0));
}
"""


def float32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


def from_bits(bits):
    return struct.unpack("<f", struct.pack("<I", bits))[0]


@dataclass(frozen=True)
class Case:
    moment: str
    pose: str
    fresnel: str
    nh: float
    a2: float
    nl: float
    nv: float
    vh: float
    f0: tuple
    f90: float
    cosine_weighted: bool = False

    def fields(self):
        return (
            self.nh,
            self.a2,
            self.nl,
            self.nv,
            *self.f0,
            self.f90,
            self.vh,
            int(self.cosine_weighted),
            0,
            0,
        )


def moments():
    result = []
    for roughness in (0, 1, 0.5, 0.2, 0.12, 0.02, 0.01, 0.001, 1e-4, 1e-6, 1e-8, 1e-9):
        alpha = float32(float32(roughness) ** 2)
        result.append((str(roughness), float32(alpha * alpha)))
    result += [("minimum-normal", FLOAT_MIN), ("subnormal", float32(1e-40))]
    bits = struct.unpack("<I", struct.pack("<f", 2**-64))[0]
    return result + [
        (f"small-adjacent{step:+}", from_bits(bits + step)) for step in (-1, 0, 1)
    ]


def fixtures():
    lower, upper = from_bits(0x3F7FFFFF), from_bits(0x3F800001)
    poses = []
    for cosine in (1, lower, 0.5, 0.2, 0.15, 0.14, 0.1, 0.001, 1e-6):
        c = float32(cosine)
        poses.append((f"mirror-{cosine}", 1, c, c, c))
    for cosine in (lower, 0.9999, 0.5, 0.1, 0.001, 1e-6):
        c = float32(cosine)
        poses.append((f"aligned-{cosine}", c, c, c, 1))
    poses += [
        ("rounded-half-cosine", upper, 1, 1, 1),
        ("rounded-fresnel-cosine", 1, 1, 1, upper),
        ("asymmetric-grazing", 1, FLOAT_MIN, float32(1e-6), float32(1e-6)),
    ]
    finishes = [
        ("dielectric", (float32(0.04),) * 3, 1),
        ("disabled", (0, 0, 0), 0),
        ("coloured", (float32(0.9), float32(0.04), 0), 1),
        ("tiny-f0", (FLOAT_MIN, float32(0.04), 1), 1),
        ("tiny-finish", (FLOAT_MIN,) * 3, FLOAT_MIN),
        ("input-denorm-f0", (float32(1e-40), float32(0.04), 0), 1),
    ]
    cases = [
        Case(name, pose, finish, nh, a2, nl, nv, vh, f0, f90)
        for name, a2 in moments()
        for pose, nh, nl, nv, vh in poses
        for finish, f0, f90 in finishes
    ]
    return cases + [replace(case, cosine_weighted=True) for case in cases]


def reference(case):
    cosine = min(abs(case.nh), 1)
    angle = math.acos(cosine)
    tangent2 = math.tan(angle) ** 2
    d = (
        case.a2 / (math.pi * cosine**4 * (case.a2 + tangent2) ** 2)
        if case.a2 > 0
        else 0
    )

    def smith_lambda(c):
        return (math.sqrt(1 + case.a2 * (1 - c * c) / (c * c)) - 1) / 2

    masking = 1 / (1 + smith_lambda(case.nl) + smith_lambda(case.nv))
    visibility = masking / (4 * case.nl * case.nv) if case.a2 > 0 else 0
    lobe = d * visibility
    grazing = (1 - min(max(case.vh, 0), 1)) ** 5
    fresnel = tuple(x + (case.f90 - x) * grazing for x in case.f0)
    colour = tuple(x * lobe * (case.nl if case.cosine_weighted else 1) for x in fresnel)
    return (d, visibility, lobe, colour[0], *colour, 0)


def compare(cases, raw):
    if len(raw) != 32 * len(cases):
        raise ValueError("native output does not contain all 32-byte records")
    values = struct.unpack("<" + str(8 * len(cases)) + "f", raw)
    nonfinite, inaccurate, denorm, unrepresentable = [], [], [], []
    for index, case in enumerate(cases):
        for component, wanted in enumerate(reference(case)):
            actual = values[8 * index + component]
            row = {"case": index, "component": LABELS[component], "reference": wanted}
            if abs(wanted) > FLOAT_MAX:
                unrepresentable.append(row)
                continue
            if not math.isfinite(actual):
                nonfinite.append(dict(row, actual=str(actual)))
            elif abs(actual - wanted) > max(FLOAT_MIN, RELATIVE_BOUND * abs(wanted)):
                row = dict(row, actual=actual)
                has_denorm = any(0 < abs(x) < FLOAT_MIN for x in case.fields())
                (denorm if has_denorm else inaccurate).append(row)
    return {
        "nonfinite_representable": nonfinite,
        "normal_input_errors": inaccurate,
        "input_denorm_accuracy_losses": denorm,
        "unrepresentable_reference_components": unrepresentable,
    }


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, output, label):
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=90, check=False
    )
    log = result.stdout + result.stderr
    (output / (label + ".log")).write_text(log)
    if result.returncode or "Validation Error" in log or "VUID-" in log:
        raise RuntimeError(f"{label} failed; see its native log")
    return {"command": list(map(str, command)), "exit": result.returncode}


def prepare(output, cases):
    palette = bytearray(64 * 4)
    struct.pack_into("<I", palette, 0, 1)
    for case in cases:
        palette += struct.pack("<12f", *case.fields())
    (output / "palette.f32").write_bytes(palette)
    probes = b"".join(
        struct.pack("<6fiI", 0, 0, 0, 0, 0, 0, i, 0) for i in range(len(cases))
    )
    (output / "probes.bin").write_bytes(probes)
    (output / "probe.comp").write_text(SHADER)
    (output / "cases.json").write_text(json.dumps([asdict(c) for c in cases], indent=2))


def capture(binary, shader_root, output, arm, cases):
    spirv = output / (arm + ".spv")
    compiler = os.environ.get("GLSLANG", "glslangValidator")
    compiled = run(
        [
            "nice",
            "-n",
            "10",
            compiler,
            "-V",
            "--target-env",
            "vulkan1.0",
            "-S",
            "comp",
            "-I" + str(shader_root),
            *(["-DGGX_COSINE_SPECULAR=1"] if arm == "candidate" else []),
            str(output / "probe.comp"),
            "-o",
            str(spirv),
        ],
        output,
        arm + "-compile",
    )
    raw = output / (arm + ".f32")
    executed = run(
        [
            str(binary),
            str(spirv),
            str(output / "palette.f32"),
            str(output / "probes.bin"),
            str(raw),
            "0",
        ],
        output,
        arm + "-native",
    )
    return dict(
        compare(cases, raw.read_bytes()),
        compile=compiled,
        native=executed,
        source_sha256=sha(shader_root / "metalRoughBrdf.glsl"),
        spirv_sha256=sha(spirv),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--consumer-receipt", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error(
            "native diagnostic output belongs in the system temporary directory"
        )
    output.mkdir(parents=True, exist_ok=True)
    consumer = (
        json.loads(args.consumer_receipt.read_text()) if args.consumer_receipt else None
    )
    if consumer and consumer.get("binary_sha256") != sha(args.binary):
        parser.error("native consumer does not match its build receipt")
    cases = fixtures()
    prepare(output, cases)
    candidate = capture(
        args.binary.resolve(), ROOT / "src/render/shaders", output, "candidate", cases
    )
    baseline = (
        capture(
            args.binary.resolve(), args.baseline.resolve(), output, "baseline", cases
        )
        if args.baseline
        else None
    )
    report = {
        "cases": len(cases),
        "native_executed": True,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "binary_sha256": sha(args.binary),
        "consumer_build_receipt": consumer,
        "source_sha256": {
            name: sha(ROOT / "src/render/shaders" / name)
            for name in (
                "metalRoughBrdf.glsl",
                "subjectLighting.glsl",
                "microfacetEnergy.glsl",
            )
        },
        "candidate": candidate,
        "baseline": baseline,
        "relative_bound": RELATIVE_BOUND,
        "absolute_flush_bound": FLOAT_MIN,
        "input_bytes": (output / "palette.f32").stat().st_size
        + (output / "probes.bin").stat().st_size,
        "scope": __doc__,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2))
    if candidate["nonfinite_representable"] or candidate["normal_input_errors"]:
        raise SystemExit(
            "production GGX failed native finite/normal-input reference checks"
        )
    print(
        f"{len(cases)} native cases: no nonfinite representable result or normal-input error"
    )
    print(
        f"input denorm accuracy losses retained: {len(candidate['input_denorm_accuracy_losses'])}"
    )


if __name__ == "__main__":
    main()
