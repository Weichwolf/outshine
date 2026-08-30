#!/usr/bin/env python3
"""Score every render corpus case THROUGH outshine-client.

A manifest is vendor data and the client speaks scenarios, so the translation lives here rather
than behind the door. Each case becomes one scenario, the client stands it and prints a digest,
and the digest is compared with the line this tree committed for it.
"""
import argparse, hashlib, json, math, os, pathlib, subprocess, sys, tempfile

TREE = pathlib.Path(__file__).resolve().parents[2]
CLIENT = TREE / "build" / "outshine-client"
kFactoryWorldRadiance = 0.05087608844041824


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared"


def scenario_for(manifest, entry):
    scene = manifest.get("scene", {})
    camera = scene.get("camera", {})
    render = manifest.get("renders", {}).get("default", {})
    at = camera.get("positionM", [0.0, 0.0, 3.0])
    look = camera.get("lookAtM", [0.0, 0.0, 0.0])
    up = camera.get("upM", [0.0, 1.0, 0.0])
    light = scene.get("light", {})
    animated = bool(manifest.get("subjects", [{}])[0].get("animation"))
    keeps = ["sceneDepth", "sceneShadingNormal", "sceneSurfaceIdentity"]
    if animated:
        keeps.append("sceneVelocity")
    lines = ['<scenario>',
             f'  <render widthPx="{render.get("resolutionX", 1280)}" '
             f'heightPx="{render.get("resolutionY", 720)}" fps="60" '
             f'transfer="linear" precision="float" exposure="1.0">']
    lines += [f'    <keep name="{one}"/>' for one in keeps]
    lines.append('  </render>')
    lines.append('  <lighting>')
    if light.get("kind") == "sun":
        d = light.get("directionM", [0.0, -1.0, 0.0])
        span = math.sqrt(sum(v * v for v in d)) or 1.0
        elevation = math.degrees(math.asin(max(-1.0, min(1.0, -d[1] / span))))
        bearing = math.degrees(math.atan2(-d[0], -d[2]))
        lines.append(f'    <key lux="{light.get("irradianceWPerM2", 0.0)}" '
                     f'elevationDeg="{elevation:.9f}" bearingDeg="{bearing:.9f}"/>')
    lines.append(f'    <environment r="{kFactoryWorldRadiance}" g="{kFactoryWorldRadiance}" '
                 f'b="{kFactoryWorldRadiance}"/>')
    lines.append('  </lighting>')
    lines += ['  <assets>', f'    <asset uri="{entry}" kind="gltf" animation="ignore"/>',
              '  </assets>', '  <views>',
              f'    <view id="oracle" fovDeg="{math.degrees(camera.get("yfovRad", 0.5)):.9f}" '
              f'nearM="{camera.get("clipStartM", 0.0):.9f}" farM="{camera.get("clipEndM", 0.0):.9f}">',
              f'      <at x="{at[0]:.12f}" y="{at[1]:.12f}" z="{at[2]:.12f}"/>',
              f'      <lookAt x="{look[0]:.12f}" y="{look[1]:.12f}" z="{look[2]:.12f}"/>',
              f'      <up x="{up[0]:.12f}" y="{up[1]:.12f}" z="{up[2]:.12f}"/>',
              '    </view>', '  </views>', '</scenario>']
    return "\n".join(lines) + "\n"


def cases(only):
    for manifest in sorted((TREE / "test" / "khronos").glob("*/*/manifest.json")):
        declared = json.loads(manifest.read_text())
        if not declared.get("renders"):
            continue
        name = manifest.parent.name
        if only and name not in only:
            continue
        yield name, manifest.parent.relative_to(TREE), declared


def main():
    ask = argparse.ArgumentParser()
    ask.add_argument("case", nargs="*")
    ask.add_argument("--write", action="store_true", help="rewrite the digest file from this run")
    told = ask.parse_args()

    kept = TREE / "test" / "khronos" / "pictures.txt"
    standing = {}
    if kept.exists():
        for line in kept.read_text().splitlines():
            if line and not line.startswith("#"):
                what, digest = line.split()[:2]
                standing[what] = digest

    made, red = {}, 0
    for name, where, declared in cases(set(told.case)):
        entry = prepared_root() / str(where).replace("/", "-") / declared["subjects"][0]["entry"]
        if not entry.exists():
            entry = entry.with_suffix(".gltf")
        if not entry.exists():
            print(f"UNPREPARED {name}")
            continue
        with tempfile.TemporaryDirectory() as scratch:
            wrote = pathlib.Path(scratch) / "case.scn"
            wrote.write_text(scenario_for(declared, entry))
            ran = subprocess.run([str(CLIENT), "run", "--rows", str(wrote), name],
                                 capture_output=True, text=True, timeout=300)
        digest = ""
        for line in ran.stdout.splitlines():
            if line.startswith("ROW"):
                digest = line.split()[2]
        if not digest:
            print(f"REFUSED {name}: {ran.stdout.strip()[:200]} {ran.stderr.strip()[:200]}")
            red += 1
            continue
        made[name] = digest
        was = standing.get(name)
        if was and was != digest and not told.write:
            print(f"MOVED   {name}  {was} -> {digest}")
            red += 1
        elif not was:
            print(f"NEW     {name}  {digest}")
        else:
            print(f"HELD    {name}  {digest}")

    over = hashlib.sha256("".join(f"{k} {made[k]}\n" for k in sorted(made)).encode()).hexdigest()[:16]
    print(f"\n{len(made)} case(s), the roll-up over every digest is {over}")
    if told.write:
        kept.write_text("# EVERY RENDER CASE'S PICTURE, digested. Rewritten by hand when a picture is\n"
                        "# MEANT to change, and the oracle comparison is what licenses the rewrite.\n"
                        + "".join(f"{k} {made[k]}\n" for k in sorted(made))
                        + f"# roll-up {over}\n")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
