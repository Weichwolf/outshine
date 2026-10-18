"""Use a source-free cooked plant at one unchanged placement in an actual world."""

import argparse
import hashlib
import json
import shlex
import subprocess
import tempfile
from pathlib import Path

import native_tree_detail_world as world_probe

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def compile_product(native, output):
    database = json.loads((ROOT / "compile_commands.json").read_text())
    objects, commands = [], []
    for name in (
        "PlantCrownPlan.cpp",
        "PlantCrownAsset.cpp",
        "PlantLimbPlan.cpp",
        "PlantLimbAsset.cpp",
        "PlantLimbSurface.cpp",
        "PlantSurface.cpp",
        "PlantShoot.cpp",
        "PlantFrame.cpp",
        "PlantSkeleton.cpp",
        "PlantRandom.cpp",
        "PlantRecipeJson.cpp",
        "PlantRecipe.cpp",
        "PlantPlan.cpp",
        "PlantBladeSurface.cpp",
    ):
        target = output / (name + ".o")
        command = world_probe.compile_unit(
            database, ROOT, name, ROOT / "src/generators/flora" / name, target
        )
        world_probe.checked(
            ["nice", "-n", "10", *command], ROOT, output / (name + ".log")
        )
        objects.append(target)
        commands.append(command)
    return objects, commands


def prepare_assets(native, output, objects, args):
    from native_plant_world_source import export

    cold = output / "cold-source"
    cold.mkdir()
    source = export(args.inputs.resolve(), cold, args.carrier_segments)
    specification = {
        "format": "native-plant-world-probe-2",
        "cold_source": source["files_sha256"],
        "blade_segments": args.blade_segments,
        "blade_across": 1,
        "carrier_base_level": args.carrier_base_level,
    }
    request = hashlib.sha256(
        json.dumps(specification, sort_keys=True).encode()
    ).hexdigest()
    database = json.loads((ROOT / "compile_commands.json").read_text())
    obj, binary = output / "prepare.o", output / "prepare"
    command = world_probe.compile_unit(
        database, ROOT, "TreePrototype.cpp", HERE / "NativePlantWorldPrepare.cpp", obj
    )
    world_probe.checked(
        ["nice", "-n", "10", *command], ROOT, output / "prepare-compile.log"
    )
    compiler = shlex.split(database[0]["command"])[0]
    link = [
        compiler,
        str(obj),
        *map(str, objects),
        str(native / "build/liboutshine.a"),
        str(native / "build/libgenerators.a"),
        *world_probe.libraries(native),
        "-o",
        str(binary),
    ]
    world_probe.checked(
        ["nice", "-n", "10", *link], native, output / "prepare-link.log"
    )
    args.assets = output / "plant.sqlite"
    args.request = request
    prepare = [
        str(binary),
        str(args.assets),
        "-",
        "none",
        str(cold / "wood.input"),
        str(cold / "blades.json"),
        request,
        str(args.blade_segments),
    ]
    if args.carrier_segments is not None:
        prepare.append(str(cold / "carriers.input"))
        if args.carrier_base_level:
            prepare.append("base")
    with (
        (cold / "crown.input").open() as input_,
        (output / "prepare.log").open("w") as log,
    ):
        subprocess.run(
            prepare,
            stdin=input_,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=60,
        )
    return [command, link, prepare]


def build(native, output, database, extra, source_text):
    source = output / "spatial.cpp"
    source.write_text(source_text)
    obj = output / "spatial.o"
    command = world_probe.compile_unit(
        database, native, "VegetationStreaming.cpp", source, obj
    )
    command.insert(1, "-I" + str(ROOT / "src/generators/flora"))
    world_probe.checked(
        ["nice", "-n", "10", *command], native, output / "spatial-compile.log"
    )
    compiler = shlex.split(database[0]["command"])[0]
    binary = output / "spatial"
    link = [
        compiler,
        str(output / "main.o"),
        str(obj),
        *map(str, extra),
        str(native / "build/liboutshine.a"),
        str(native / "build/libgenerators.a"),
        *world_probe.libraries(native),
        "-o",
        str(binary),
    ]
    world_probe.checked(
        ["nice", "-n", "10", *link], native, output / "spatial-link.log"
    )
    return binary, [command, link]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--request")
    parser.add_argument(
        "--inputs", type=Path, default=HERE.parent / "vegetation/data/crown682"
    )
    parser.add_argument("--blade-segments", type=int, choices=(2, 4, 8), default=4)
    parser.add_argument("--carrier-segments", type=int, choices=(2, 4))
    parser.add_argument("--carrier-base-level", action="store_true")
    parser.add_argument("--world", type=Path)
    parser.add_argument("--place", required=True)
    parser.add_argument("--bearing-deg", required=True, type=float)
    parser.add_argument("--pitch-deg", required=True, type=float)
    args = parser.parse_args()
    if args.carrier_base_level and args.carrier_segments is None:
        parser.error("base-level alpha control requires a connected carrier")
    output, native = args.output.resolve(), args.native_root.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        raise ValueError("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
    runtime = output / "runtime"
    (runtime / "build").mkdir(parents=True)
    (runtime / "src").symlink_to(native / "src")
    for name in ("shaders", "compute-shader-contracts.json"):
        (runtime / "build" / name).symlink_to(native / "build" / name)
    extras, product_commands = compile_product(native, output)
    if args.assets is None:
        product_commands += prepare_assets(native, output, extras, args)
    elif not args.request:
        raise ValueError("existing assets require their complete request identity")
    database = json.loads((native / "compile_commands.json").read_text())
    main_command = world_probe.compile_unit(
        database, native, "Main.cpp", native / "src/client/Main.cpp", output / "main.o"
    )
    world_probe.checked(
        ["nice", "-n", "10", *main_command], native, output / "main-compile.log"
    )
    snapshot = args.world
    if snapshot is None:
        dump, dump_commands = world_probe.build(
            native, output, database, [], "dump", dump=True
        )
        product_commands += dump_commands
        world_probe.capture(dump, runtime, output, args.place, "source-dump")
        snapshot = output / "world.json"
    original = json.loads(snapshot.read_text())
    selected = world_probe.choose(original, args.bearing_deg, args.pitch_deg, 1, True)
    _, group, row = selected[0]
    selection = (
        f"1 1 0\n{group} {row} "
        + " ".join(format(v, ".17g") for v in original["groups"][group]["models"][row])
        + "\n"
    )
    (output / "selection.txt").write_text(selection)
    flat, commands = world_probe.build(native, output, database, [], "flat")
    results = [world_probe.capture(flat, runtime, output, args.place, "flat")]
    commands += product_commands
    source = world_probe.instrument(native, output, False)
    source = world_probe.replace_once(
        source,
        json.dumps(str(HERE / "NativeTreeDetailPreview.h")),
        json.dumps(str(HERE / "NativeTreeDetailPreview.h"))
        + "\n#include "
        + json.dumps(str(HERE / "NativePlantWorldPreview.h")),
    )
    source = world_probe.replace_once(
        source,
        "NativeTreeDetailProbe::Directory = config.Cache.Directory;",
        "NativeTreeDetailProbe::Directory = config.Cache.Directory;\n"
        + "NativePlantWorldProbe::Database = "
        + json.dumps(str(args.assets.resolve()))
        + ";\n"
        + "NativePlantWorldProbe::Identity = "
        + json.dumps(args.request)
        + ";",
    )
    source = world_probe.replace_once(
        source,
        "NativeTreeDetailProbe::Prepare(Groups_,*Renderer_,error)",
        "NativePlantWorldProbe::Prepare(Groups_,*Renderer_,error)",
    )
    spatial, built = build(native, output, database, extras, source)
    commands += built
    results.append(world_probe.capture(spatial, runtime, output, args.place, "spatial"))
    plant = next(
        line
        for line in (output / "spatial.log").read_text().splitlines()
        if line.startswith("PLANT ")
    )
    receipt = {
        "source_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "native_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=native, text=True
        ).strip(),
        "selected": selected,
        "plant": plant,
        "results": results,
        "assets_sha256": hashlib.sha256(args.assets.read_bytes()).hexdigest(),
        "parameters": {
            k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()
        },
        "commands": [main_command, *commands],
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(
        json.dumps({"selected": selected, "plant": plant, "results": results}),
        flush=True,
    )


if __name__ == "__main__":
    main()
