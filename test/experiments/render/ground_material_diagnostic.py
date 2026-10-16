"""Inspect stored ground classes and a controlled canopy contribution on a frozen client."""

import argparse
import hashlib
import json
import math
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
COLOURS = {
    "mixed_broadleaf": (0.25, 0.06, 0.015),
    "conifer_forest": (0.02, 0.20, 0.04),
    "meadow": (0.6, 0.65, 0.02),
    "alpine_grassland": (0.02, 0.8, 0.03),
    "arable": (0.6, 0.3, 0.015),
    "heath": (0.4, 0.03, 0.5),
    "rock_crevice": (0.8, 0.02, 0.5),
    "scree": (0.8, 0.2, 0.015),
    "glacier": (0.02, 0.65, 0.8),
    "settlement": (0.3, 0.3, 0.3),
    "sealed": (0.02, 0.02, 0.02),
    "water": (0.01, 0.03, 0.8),
    "riverbank": (0.2, 0.08, 0.015),
    "unmapped": (0.5, 0.5, 0.8),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canopy_sources(shader, output, catalog, family, cover):
    templates = catalog["templates"]
    row = next(i for i, t in enumerate(templates) if t["name"] == family)
    grass = templates[row]["grass"]
    blade = next(b for b in catalog["bladeClasses"] if b["name"] == grass["class"])
    dry = grass["dryFraction"]
    mean = [
        a + (z - a) * dry
        for a, z in zip(blade["greenLinear"], blade["dryLinear"], strict=True)
    ]
    material = (shader / "groundMaterial.glsl").read_text()
    begin = material.index("vec4 groundWears(")
    end = material.index("\nfloat groundRockWeight(", begin)
    function = material[begin:end].replace("return vec4(", "vec4 result = vec4(")
    assert function.endswith("}\n")
    colour = ", ".join(f"{v:.10f}" for v in mean)
    function = function[:-2] + (
        f"  if (row == {row}u) {{ result.rgb = mix(result.rgb, vec3({colour}), {cover:g}); }}\n"
        "  return result;\n}\n"
    )
    (output / "groundMaterial.glsl").write_text(
        material[:begin] + function + material[end:]
    )
    layers = (shader / "groundLayers.glsl").read_text()
    needle = "  vec4 litterRecipe = vec4("
    assert layers.count(needle) == 1
    layers = layers.replace(
        needle,
        f"  if (materialRow == {row}u) {{ difference.rgb *= 1.0 - {cover:g}; }}\n"
        + needle,
    )
    (output / "groundLayers.glsl").write_text(layers)
    return {
        "family": family,
        "row": row,
        "cover": cover,
        "dry_fraction": dry,
        "mean_rgb": mean,
    }


def prepare(source, output, mode, family, cover):
    catalog_file = source / "src/assets/world/vegetation.json"
    catalog = json.loads(catalog_file.read_text())
    names = [t["name"] for t in catalog["templates"]] + ["unmapped"]
    colours = [COLOURS[name] for name in names]
    shader = source / "src/render/shaders"
    for name in ("groundMaterial.glsl", "groundLayers.glsl"):
        shutil.copyfile(shader / name, output / name)
    fragment = (shader / "groundLit.glsl").read_text()
    needle = "vec3 albedo = clamp(composed.Albedo, vec3(0.0), vec3(1.0));"
    assert fragment.count(needle) == 1
    control = None
    if mode == "classes":
        array = ", ".join("vec3(" + ",".join(map(str, c)) + ")" for c in colours)
        replacement = (
            f"const vec3 marker[{len(names)}] = vec3[]({array});\n"
            f"  vec3 albedo = marker[clamp(which,0,{len(names) - 1})];"
        )
    elif mode == "rock-share":
        replacement = "vec3 albedo = vec3(rockWeight);"
    else:
        control = canopy_sources(shader, output, catalog, family, cover)
        replacement = needle
    fragment_file = output / "groundLit.glsl"
    fragment_file.write_text(fragment.replace(needle, replacement))
    shaders = output / "build/shaders"
    shutil.copytree(source / "build/shaders", shaders, dirs_exist_ok=True)
    commands = []
    variants = sorted(shaders.glob("groundLit-*.frag.spv"))
    assert variants
    with (output / "compile.log").open("w") as log:
        for artifact in variants:
            layout = (
                artifact.name.removeprefix("groundLit-")
                .removesuffix(".frag.spv")
                .ljust(5, "0")
            )
            fields = (
                "SUBJECT_WRITES_VELOCITY",
                "SUBJECT_NORMAL_LOCATION",
                "SUBJECT_IDENTITY_LOCATION",
                "SUBJECT_BASE_LOCATION",
                "SUBJECT_METALROUGH_LOCATION",
            )
            command = [
                "nice",
                "-n",
                "10",
                os.environ.get("GLSLANG", "glslangValidator"),
                "-V",
                "--target-env",
                "vulkan1.0",
                "-S",
                "frag",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/content/shade",
                "-Ibuild/shaders",
                *[f"-D{k}={v}" for k, v in zip(fields, layout, strict=True)],
                str(fragment_file),
                "-o",
                str(artifact),
            ]
            subprocess.run(
                command,
                cwd=source,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=True,
                timeout=60,
            )
            commands.append(command)
    return {
        "catalog_sha256": digest(catalog_file),
        "legend": dict(zip(names, colours, strict=True)),
        "control": control,
        "compile_commands": commands,
        "fragment_sha256": {p.name: digest(p) for p in variants},
        "source_sha256": {p.name: digest(p) for p in output.glob("*.glsl")},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-checkout", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--mode", choices=("classes", "rock-share", "canopy-rgb"), required=True
    )
    parser.add_argument("--family", default="alpine_grassland")
    parser.add_argument("--cover", type=float)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--preload-seconds", type=float, default=10)
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("place")
    args = parser.parse_args()
    if args.mode == "canopy-rgb" and (args.cover is None or not 0 <= args.cover <= 1):
        parser.error("canopy-rgb requires an explicit finite --cover in [0,1]")
    if args.width <= 0 or args.height <= 0:
        parser.error("positive render dimensions required")
    if not math.isfinite(args.preload_seconds) or args.preload_seconds <= 0:
        parser.error("finite positive preload timeout required")
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error("use an explicit system-temp output outside the checkout")
    source, binary = args.source_checkout.resolve(), args.binary.resolve()
    if output.is_relative_to(source):
        parser.error("diagnostic output must be outside the source snapshot")
    output.mkdir(parents=True, exist_ok=True)
    link = output / "src"
    if link.exists():
        assert link.resolve() == source / "src"
    else:
        link.symlink_to(source / "src", target_is_directory=True)
    report = prepare(source, output, args.mode, args.family, args.cover)
    report.update(
        {
            "mode": args.mode,
            "binary": str(binary),
            "binary_sha256": digest(binary),
            "scope": "Private lit diagnostic; no numeric colour decoding, complete canopy BSDF or budget acceptance",
            "executed": False,
        }
    )
    if not args.compile_only:
        command = [
            str(binary),
            "shots",
            "--scenario-overrides",
            json.dumps({"render": {"widthPx": args.width, "heightPx": args.height}}),
            "--stats",
            "--offline",
            "--preload-seconds",
            str(args.preload_seconds),
            args.place,
        ]
        with (output / "capture.log").open("w") as log:
            try:
                result = subprocess.run(
                    command,
                    cwd=output,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    check=False,
                    timeout=args.preload_seconds + 120,
                )
                status = result.returncode
            except subprocess.TimeoutExpired:
                status = 124
                report["reason"] = "client exceeded the diagnostic process timeout"
        report.update({"executed": True, "command": command, "exit": status})
    (output / "diagnostic.json").write_text(json.dumps(report, indent=2) + "\n")
    print(output / "diagnostic.json")
    return report.get("exit", 0)


if __name__ == "__main__":
    raise SystemExit(main())
