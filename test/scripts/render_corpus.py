#!/usr/bin/env python3
"""Score every render corpus case THROUGH outshine-client.

A manifest is vendor data and the client speaks scenarios, so the translation lives here rather
than behind the door. Each case becomes one scenario, the client stands it and prints a digest,
and the digest is compared with the line this tree committed for it.
"""
import argparse, hashlib, json, math, os, pathlib, subprocess, sys, tempfile

TREE = pathlib.Path(__file__).resolve().parents[2]
CLIENT = TREE / "build" / "outshine-client"


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared"


def facing(position, look_at, roll):
    fx, fy, fz = (look_at[i] - position[i] for i in range(3))
    length = math.sqrt(fx * fx + fy * fy + fz * fz)
    if length <= 0.0:
        return (0.0, 0.0, 0.0, 1.0)
    fx, fy, fz = fx / length, fy / length, fz / length
    ux, uy, uz = 0.0, 1.0, 0.0
    rx, ry, rz = fy * uz - fz * uy, fz * ux - fx * uz, fx * uy - fy * ux
    rl = math.sqrt(rx * rx + ry * ry + rz * rz)
    if rl <= 1.0e-9:
        rx, ry, rz, rl = 1.0, 0.0, 0.0, 1.0
    rx, ry, rz = rx / rl, ry / rl, rz / rl
    ux, uy, uz = ry * fz - rz * fy, rz * fx - rx * fz, rx * fy - ry * fx
    if roll:
        c, s = math.cos(roll), math.sin(roll)
        rx, ry, rz, ux, uy, uz = (c * rx + s * ux, c * ry + s * uy, c * rz + s * uz,
                                  c * ux - s * rx, c * uy - s * ry, c * uz - s * rz)
    m = ((rx, ux, -fx), (ry, uy, -fy), (rz, uz, -fz))
    trace = m[0][0] + m[1][1] + m[2][2]
    if trace > 0.0:
        w = math.sqrt(1.0 + trace) * 0.5
        d = 0.25 / w
        return ((m[2][1] - m[1][2]) * d, (m[0][2] - m[2][0]) * d, (m[1][0] - m[0][1]) * d, w)
    if m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0
        return (0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[2][1] - m[1][2]) / s)
    if m[1][1] > m[2][2]:
        s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0
        return ((m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s, (m[0][2] - m[2][0]) / s)
    s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0
    return ((m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s, (m[1][0] - m[0][1]) / s)


def scenario_for(manifest, entry):
    scene = manifest.get("scene", {})
    camera = scene.get("camera", {})
    render = manifest.get("renders", {}).get("default", {})
    at = camera.get("positionM", [0.0, 0.0, 3.0])
    look = camera.get("lookAtM", [0.0, 0.0, 0.0])
    qx, qy, qz, qw = facing(at, look, camera.get("rollRad", 0.0))
    light = scene.get("light", {})
    lines = ['<scenario>',
             f'  <render widthPx="{render.get("resolutionX", 1280)}" '
             f'heightPx="{render.get("resolutionY", 720)}" fps="60"/>']
    if light.get("kind") == "sun":
        lines.append(f'  <lighting sunIrradianceWPerM2="{light.get("irradianceWPerM2", 0.0)}"/>')
    lines += [f'  <assets>', f'    <asset uri="{entry}" kind="gltf" animation="ignore"/>',
              '  </assets>', '  <views>',
              f'    <view id="oracle" fovDeg="{math.degrees(camera.get("yfovRad", 0.5)):.9f}">',
              f'      <at x="{at[0]:.12f}" y="{at[1]:.12f}" z="{at[2]:.12f}" '
              f'qx="{qx:.12f}" qy="{qy:.12f}" qz="{qz:.12f}" qw="{qw:.12f}"/>',
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
