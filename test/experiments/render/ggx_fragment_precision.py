"""Compare the production GGX functions in the actual fragment execution stage.

Uses the existing numeric fixtures and independent angular-NDF/Smith-lambda
reference. Normal-input failures stay failures; denorm losses stay separate.
"""

import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path

import ggx_stability as ggx

ROOT = Path(__file__).resolve().parents[3]


def fragment_source():
    source = ggx.SHADER[ggx.SHADER.index("layout(std140") :]
    source = source.replace(
        "uint i=gl_GlobalInvocationID.x;", "uint i=uint(gl_FragCoord.x);"
    )
    source = source.replace("uint(probes[i].Which)", "i")
    source = source.replace(
        "result[i]=Sample(vec4(d,v,lobe,brdf.specular.r),vec4(brdf.specular,0.0));",
        "distributionValues=vec4(d,v,lobe,brdf.specular.r);\n"
        "  colourValues=vec4(brdf.specular,0.0);",
    )
    if "result[i]" in source or "probes[" in source:
        raise ValueError("the shared GGX probe no longer matches the fragment adapter")
    return (
        "#version 450\n#extension GL_GOOGLE_include_directive : require\n"
        "layout(std430,set=0,binding=0) readonly buffer Palette {float words[];};\n"
        "layout(location=0)out vec4 distributionValues;\n"
        "layout(location=1)out vec4 colourValues;\n" + source
    )


def capture(binary, shader_root, output, arm, cases, compiler):
    spirv = output / (arm + ".frag.spv")
    compiled = ggx.run(
        [
            compiler,
            "-V",
            "--target-env",
            "vulkan1.0",
            "-I" + str(shader_root),
            *(["-DGGX_COSINE_SPECULAR=1"] if arm == "candidate" else []),
            str(output / "probe.frag"),
            "-o",
            str(spirv),
        ],
        output,
        arm + "-compile",
    )
    raw = output / (arm + ".f32")
    executed = ggx.run(
        [binary, output / "probe.vert.spv", spirv, output / "palette.f32", raw],
        output,
        arm + "-native",
    )
    return dict(
        ggx.compare(cases, raw.read_bytes()),
        compile=compiled,
        native=executed,
        source_sha256=ggx.sha(shader_root / "metalRoughBrdf.glsl"),
        spirv_sha256=ggx.sha(spirv),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--baseline", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error(
            "native diagnostic output belongs in the system temporary directory"
        )
    output.mkdir(parents=True, exist_ok=True)
    cases = ggx.fixtures()
    ggx.prepare(output, cases)
    (output / "probe.frag").write_text(fragment_source())
    (output / "probe.vert").write_text(
        "#version 450\nvoid main(){vec2 p[3]=vec2[](vec2(-1,-1),vec2(3,-1),"
        "vec2(-1,3));gl_Position=vec4(p[gl_VertexIndex],0,1);}\n"
    )
    compiler = os.environ.get("GLSLANG", "glslangValidator")
    ggx.run(
        [
            compiler,
            "-V",
            str(output / "probe.vert"),
            "-o",
            str(output / "probe.vert.spv"),
        ],
        output,
        "vertex-compile",
    )
    candidate = capture(
        args.binary.resolve(),
        ROOT / "src/render/shaders",
        output,
        "candidate",
        cases,
        compiler,
    )
    baseline = (
        capture(
            args.binary.resolve(),
            args.baseline.resolve(),
            output,
            "baseline",
            cases,
            compiler,
        )
        if args.baseline
        else None
    )
    report = {
        "cases": len(cases),
        "stage": "fragment",
        "binary_sha256": ggx.sha(args.binary),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "candidate": candidate,
        "baseline": baseline,
        "scope": __doc__,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    errors = candidate["normal_input_errors"]
    nonfinite = candidate["nonfinite_representable"]
    print(
        f"{len(cases)} fragment cases: {len(errors)} normal-input errors; {len(nonfinite)} nonfinite"
    )
    raise SystemExit(bool(errors or nonfinite))


if __name__ == "__main__":
    main()
