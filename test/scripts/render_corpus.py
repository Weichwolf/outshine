#!/usr/bin/env python3
"""Score every render corpus case THROUGH outshine-client, against the oracle's own picture.

A manifest is vendor data and the client speaks scenarios, so the translation lives here rather
than behind the door. Each case becomes one scenario, the client stands it and prints a digest,
and the digest is compared with the line this tree committed for it.
"""
import argparse, json, math, os, pathlib, subprocess, sys, tempfile

from PIL import Image
import numpy as np

TREE = pathlib.Path(__file__).resolve().parents[2]
CLIENT = TREE / "build" / "outshine-client"
kFactoryWorldRadiance = 0.05087608844041824


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared"


def parts_of(entry):
    """The file's primitives in the order an importer meets them, and the material each wears."""
    if entry.suffix != ".gltf":
        return []
    file = json.loads(entry.read_text())
    worn = []
    for mesh in file.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            worn.append(primitive.get("material", -1))
    return worn


def wears(material, worn, names):
    """The oracle's own shading, said as the door says it.

    A corpus case's oracle is an EMISSION shader over one colour per material: Blender's world is
    at strength zero, its light is none and its bounce budget is zero, so the reference picture IS
    the albedo and nothing in it is shaded. Handing the engine the same flat radiance is what makes
    the two pictures answer one question -- where the geometry is and which texel lands where --
    rather than two.
    """
    kind = material.get("kind", "")
    said = []
    by_material = {}
    if kind == "emission-per-material":
        stated = material.get("colourLinearPerMaterial", {})
        for at, name in enumerate(names):
            if name in stated:
                by_material[at] = stated[name]
        for at, (name, colour) in enumerate(stated.items()):
            by_material.setdefault(at, colour)
    elif kind == "emission-by-material-index":
        for at, colour in enumerate(material.get("colourLinearByMaterialIndex", [])):
            by_material[at] = colour
    elif kind == "emission":
        return ['      <wears part="%d" keepsMaps="yes">'
                '<row unlit="yes" r="1" g="1" b="1" '
                'emissionR="1" emissionG="1" emissionB="1"/></wears>' % part
                for part in range(len(worn))]
    for part, material_at in enumerate(worn):
        colour = by_material.get(material_at)
        if colour is None:
            continue
        said.append(f'      <wears part="{part}">')
        said.append(f'        <row unlit="yes" emissionR="{colour[0]}" '
                    f'emissionG="{colour[1]}" emissionB="{colour[2]}" r="0" g="0" b="0"/>')
        said.append('      </wears>')
    return said


def derived_camera(entry):
    """What Blender ACTUALLY framed, which is the only camera worth declaring.

    A manifest may state a camera and 34 of them state none at all, but provenance.json records
    what the oracle was rendered with either way -- and a manifest's stated camera goes stale the
    moment the framing rule moves, which it just did. Reading the provenance means the reference
    and the scenario come from ONE source and cannot disagree."""
    told = entry.parent / "provenance.json"
    if not told.exists():
        return {}
    said = json.loads(told.read_text())
    for render in said.get("report", {}).get("render", []):
        camera = render.get("provenance", {}).get("camera", {})
        if camera.get("derivedFrom"):
            return camera["derivedFrom"]
    return {}


def scenario_for(manifest, entry):
    scene = manifest.get("scene", {})
    camera = scene.get("camera", {})
    camera = {**camera, **derived_camera(entry)}
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
    if scene.get("material", {}).get("kind") in ("diffuse", "metal-rough"):
        lines.append(f'    <environment r="{kFactoryWorldRadiance}" g="{kFactoryWorldRadiance}" '
                     f'b="{kFactoryWorldRadiance}"/>')
    lines.append('  </lighting>')
    lines += ['  <assets>', f'    <asset uri="{entry}" kind="gltf" animation="ignore">']
    worn = parts_of(entry)
    names = [one.get("name") for one in
             (json.loads(entry.read_text()).get("materials", [])
              if entry.suffix == ".gltf" else [])]
    lines += wears(scene.get("material", {}), worn, names)
    lines += ['    </asset>', '  </assets>', '  <views>',
              f'    <view id="oracle" fovDeg="{math.degrees(camera.get("yfovRad", 0.5)):.9f}" '
              f'nearM="{camera.get("clipStartM", 0.0):.9f}" farM="{camera.get("clipEndM", 0.0):.9f}">',
              f'      <at x="{at[0]:.12f}" y="{at[1]:.12f}" z="{at[2]:.12f}"/>',
              f'      <lookAt x="{look[0]:.12f}" y="{look[1]:.12f}" z="{look[2]:.12f}"/>',
              f'      <up x="{up[0]:.12f}" y="{up[1]:.12f}" z="{up[2]:.12f}"/>',
              '    </view>', '  </views>', '</scenario>']
    return "\n".join(lines) + "\n"


kMostDelta = 8
kLeastAgreeing = 0.9999


def excluded():
    """Cases this corpus does not score, each with the REASON it does not.

    Blender is not an oracle for everything it can open. A case whose answer Cycles cannot state --
    or states differently by design rather than by defect -- proves nothing about this engine, and
    running it to watch it go red teaches a reader to ignore red. A line without a reason is
    refused, because an exclusion nobody explained is indistinguishable from one nobody noticed.
    """
    told = TREE / "test" / "khronos" / "excluded.txt"
    out = {}
    if not told.exists():
        return out
    for at, line in enumerate(told.read_text().splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, why = line.partition(" ")
        if not why.strip():
            raise SystemExit(f"{told}:{at}: '{name}' is excluded and says no reason why")
        out[name] = why.strip()
    return out


def cases(only):
    for manifest in sorted((TREE / "test" / "khronos").glob("*/*/manifest.json")):
        declared = json.loads(manifest.read_text())
        if not declared.get("renders"):
            continue
        name = manifest.parent.name
        if only and name not in only:
            continue
        yield name, manifest.parent, declared


def scored(ours, reference):
    a = np.asarray(Image.open(ours).convert("RGB")).astype(np.int16)
    b = np.asarray(Image.open(reference).convert("RGB")).astype(np.int16)
    if a.shape != b.shape:
        return None, None, None
    apart = np.abs(a - b).max(axis=2)
    return float((apart <= kMostDelta).mean()), int(apart.max()), int((apart > kMostDelta).sum())


def main():
    ask = argparse.ArgumentParser()
    ask.add_argument("case", nargs="*")
    told = ask.parse_args()

    known = excluded()
    held, red, unscored, skipped = 0, [], 0, 0
    for name, where, declared in cases(set(told.case)):
        if name in known:
            skipped += 1
            print(f"ASIDE {name:34s} {known[name]}")
            continue
        reference = where / "reference.png"
        entry = prepared_root() / str(where.relative_to(TREE)).replace("/", "-") / \
            declared["subjects"][0]["entry"]
        if not entry.exists():
            entry = entry.with_suffix(".gltf")
        if not (reference.exists() and entry.exists()):
            unscored += 1
            continue
        with tempfile.TemporaryDirectory() as scratch:
            wrote = pathlib.Path(scratch) / "case.scn"
            wrote.write_text(scenario_for(declared, entry))
            ran = subprocess.run([str(CLIENT), "run", "--rows", str(wrote), name],
                                 capture_output=True, text=True, timeout=600)
        digest = ""
        for line in ran.stdout.splitlines():
            if line.startswith("ROW"):
                digest = line.split()[2]
        if not digest:
            red.append((name, 0.0, 0, "the client drew nothing"))
            continue
        drew = TREE / "build" / "shots" / "khronos" / f"{name}-{digest}.png"
        agreeing, most, apart = scored(drew, reference)
        if agreeing is None:
            red.append((name, 0.0, 0, "the frames are not the same shape"))
        elif agreeing >= kLeastAgreeing:
            held += 1
            print(f"HELD  {name:34s} {agreeing * 100:8.4f}%  worst pixel {most:3d}")
        else:
            red.append((name, agreeing, most, f"{apart} pixel(s) apart by more than {kMostDelta}"))
            print(f"APART {name:34s} {agreeing * 100:8.4f}%  worst pixel {most:3d}  {apart} px")

    print(f"\n{held} held, {len(red)} apart, {skipped} set aside with a reason, {unscored} "
          f"unscored (no reference or no prepared subject)")
    print(f"the bar is {kLeastAgreeing * 100:.2f}% of pixels within {kMostDelta} of 255; the worst "
          f"pixel is REPORTED and never gated, because one pixel at 255 is a hole rather than a "
          f"tolerance")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
