"""Offline asset preparation for the glTF render ladder. Never a test, a gate or a build step."""

import argparse
import hashlib
import json
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from prep import blender as blender_module
from prep import jobs, manifest as manifest_module
from prep import vendor
from prep import test262 as test262_module
from prep import wpt as wpt_module
from prep.refusal import Refusal
from prep.store import ContentStore

PREPARED_LEAF = "outshine-prepared"

def prepared_directory(manifest_path):
    """Where a case's fetched, generated, converted and rendered files go, and it is NOT the tree.

    `CLAUDE.md`: every artefact goes to the system temp directory, never into the tree -- a repository
    is what is declared and what is built from it. The leaf is the case's own path with its separators
    flattened, so two cases cannot collide and the mapping is derivable from either end without a
    table."""
    case = os.path.dirname(os.path.abspath(manifest_path))
    root = os.path.abspath(os.curdir)
    leaf = os.path.relpath(case, root) if case.startswith(root + os.sep) else os.path.basename(case)
    return os.path.join(tempfile.gettempdir(), PREPARED_LEAF, leaf.replace(os.sep, "-"))

CASE_TREES = ("test/render/khronos/glTF", "test/render/khronos/generator", "test/render/outshine/grown",
              "test/render/wpt/css", "test/render/test262/js")

def every_manifest():
    """Every case's declaration, in a stable order so two runs report the same list."""
    found = []
    for tree in CASE_TREES:
        for here, _, files in os.walk(tree):
            if "manifest.json" in files:
                found.append(os.path.join(here, "manifest.json"))
    return sorted(found)

def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("job", choices=("fetch", "generate", "patch", "convert", "render", "all",
                                        "dry-run", "wpt-cases", "test262-cases", "generator-cases",
                                        "scenario-assets"))
    parser.add_argument("--manifest", action="append", default=None,
                        help="a case's manifest; repeatable")
    parser.add_argument("--every-case", action="store_true",
                        help="every manifest under " + " and ".join(CASE_TREES))
    parser.add_argument("--dest", default=None,
                        help="default: this case's directory under the system temp root")
    parser.add_argument("--store", default=None, help="default: the library's content store")
    parser.add_argument("--blender", default=None)
    parser.add_argument("--recipe", action="append", default=None, help="render only this recipe; repeatable")
    parser.add_argument("--force", action="store_true", help="redo the work even on a cache hit")
    parser.add_argument("--no-cache", action="store_true")
    parser.add_argument("--wpt-commit", default=None, help="wpt-cases: the pin every case records")
    parser.add_argument("--wpt-directory", default=None, help="wpt-cases: the upstream directory, e.g. css/css-flexbox")
    parser.add_argument("--wpt-root", default=None, help="wpt-cases: where the case directories are written")
    parser.add_argument("--wpt-viewport", default="800x600")
    parser.add_argument("--wpt-pinned-on", default=None, help="wpt-cases: the date the pin was taken")
    parser.add_argument("--wpt-pin-reason", default=None)
    parser.add_argument("--t262-commit", default=None, help="test262-cases: the pin every case records")
    parser.add_argument("--t262-root", default=None)
    parser.add_argument("--t262-pinned-on", default=None)
    parser.add_argument("--t262-pin-reason", default=None)
    parser.add_argument("--gen-commit", default=None, help="generator-cases: the pin every case records")
    parser.add_argument("--gen-root", default=None, help="generator-cases: where the case directories are written")
    parser.add_argument("--gen-pinned-on", default=None)
    parser.add_argument("--gen-pin-reason", default=None)
    arguments = parser.parse_args(argv)

    if arguments.job == "scenario-assets":
        return _scenario_assets(arguments)
    if arguments.job == "wpt-cases":
        return _wpt_cases(arguments)
    if arguments.job == "test262-cases":
        return _test262_cases(arguments)
    if arguments.job == "generator-cases":
        return _generator_cases(arguments)

    manifests = list(arguments.manifest or [])
    if arguments.every_case:
        manifests += [one for one in every_manifest() if one not in manifests]
    if not manifests:
        parser.error("name at least one --manifest, or --every-case")

    if len(manifests) > 1:
        refused = []
        for one in manifests:
            try:
                _prepare(one, arguments)
            except Refusal as refusal:
                refused.append(one)
                print("refused: " + one, file=sys.stderr)
                print(str(refusal), file=sys.stderr)
        print("prepared %d of %d cases" % (len(manifests) - len(refused), len(manifests)),
              file=sys.stderr)
        for one in refused:
            print("  refused " + one, file=sys.stderr)
        return 1 if refused else 0
    return _prepare(manifests[0], arguments)

SCENARIO_ASSETS = (
    {
        "leaf": "tools-driver-f31",
        "title": "2014 BMW 3 Series (F31)",
        "author": "DisneyCars (https://sketchfab.com/supercarmodels)",
        "licence": "CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)",
        "source": "https://sketchfab.com/3d-models/"
                  "2014-bmw-3-series-f31-71746440f98d48ca9ea41ceeaa3504c7",
        "credit": 'This work is based on "2014 BMW 3 Series (F31)" by DisneyCars, '
                  "licensed under CC-BY-4.0",
        "declaredBy": "tools/driver/f31.scenario",
        "files": {
            "scene.gltf": "c60068fcd0f8c25e73225cd3725a422fca46c00a2a68ca481988a6680cc5fb1d",
            "scene.bin": "be46e9c11f5b7f16a2cc01a3a96b92394bff04ed3742a8974de2f9bc093ba453",
        },
        "carries": ("textures",),
        "roots": ("~/Downloads/2014_bmw_3_series_f31",),
    },
)

def _digest(path):
    """The file's SHA-256, which is what decides whether this is the declared asset."""
    reading = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            reading.update(block)
    return reading.hexdigest()

def _scenario_assets(arguments):
    """Place the assets a SCENARIO declares, from a licensed copy this machine already holds.

    A Khronos subject carries a URL and is fetched; these carry a page a person visits and accepts a
    licence on, so there is nothing a script may fetch unattended. What a script CAN do is verify: the
    scenario pins the digest, so a wrong file is refused by name and a right one is the declared asset
    byte for byte. Absent, this refuses and prints where the asset comes from -- it never substitutes."""
    placed, refused = [], []
    for asset in SCENARIO_ASSETS:
        destination = os.path.join(tempfile.gettempdir(), PREPARED_LEAF, asset["leaf"])
        found = None
        for root in asset["roots"]:
            here = os.path.expanduser(root)
            if all(os.path.isfile(os.path.join(here, name)) for name in asset["files"]):
                found = here
                break
        if found is None:
            refused.append({"leaf": asset["leaf"], "why": "no root holds every declared file",
                            "looked": [os.path.expanduser(r) for r in asset["roots"]],
                            "source": asset["source"], "licence": asset["licence"]})
            continue
        wrong = {}
        for name, want in asset["files"].items():
            got = _digest(os.path.join(found, name))
            if got != want:
                wrong[name] = {"declared": want, "found": got}
        if wrong:
            refused.append({"leaf": asset["leaf"], "why": "a file is not what the scenario declares",
                            "from": found, "files": wrong, "source": asset["source"]})
            continue
        os.makedirs(destination, exist_ok=True)
        for name in asset["files"]:
            shutil.copyfile(os.path.join(found, name), os.path.join(destination, name))
        for directory in asset["carries"]:
            source = os.path.join(found, directory)
            if not os.path.isdir(source):
                continue
            target = os.path.join(destination, directory)
            if os.path.isdir(target):
                shutil.rmtree(target)
            shutil.copytree(source, target)
        with open(os.path.join(destination, "CREDIT.txt"), "w", encoding="utf-8") as handle:
            handle.write(asset["credit"] + "\n" + asset["source"] + "\n" + asset["licence"] + "\n")
        placed.append({"leaf": asset["leaf"], "from": found, "to": destination,
                       "declaredBy": asset["declaredBy"]})
    _emit({"placed": placed, "refused": refused})
    return 0 if not refused else 1

def _wpt_cases(arguments):
    """Turn an upstream directory at the pin into one case directory each, and report both counts."""
    for name in ("wpt_commit", "wpt_directory", "wpt_root", "wpt_pinned_on", "wpt_pin_reason"):
        if getattr(arguments, name) is None:
            raise Refusal("wpt-cases", expected="--" + name.replace("_", "-"), observed="absent")
    width, _, height = arguments.wpt_viewport.partition("x")
    viewport = (int(width), int(height))
    tests = wpt_module.tests_in(arguments.wpt_commit, arguments.wpt_directory)
    written, stated_nothing = [], 0
    for test in tests:
        declared = wpt_module.case(arguments.wpt_commit, arguments.wpt_directory, test,
                                   arguments.wpt_pinned_on, arguments.wpt_pin_reason, viewport)
        if declared is None:
            stated_nothing += 1
            continue
        written.append(wpt_module.write(declared, arguments.wpt_root))
    _emit({"directory": arguments.wpt_directory, "commit": arguments.wpt_commit,
           "tests": len(tests), "cases": len(written),
           "statesNoLayoutOfItsOwn": stated_nothing, "root": arguments.wpt_root})
    return 0

def _generator_cases(arguments):
    """The generator's animation groups at the pin, one case directory each (board:1458)."""
    for name in ("gen_commit", "gen_root", "gen_pinned_on", "gen_pin_reason"):
        if getattr(arguments, name) is None:
            raise Refusal("generator-cases", expected="--" + name.replace("_", "-"), observed="absent")
    generator_module = vendor.at(os.path.join(vendor.harness_of(arguments.gen_root), "prepare",
                                              "import_cases.py"))
    written, groups = [], {}
    for group in generator_module.GROUPS:
        stated = generator_module.says(arguments.gen_commit, group)
        models = generator_module.models_in(arguments.gen_commit, group)
        groups[group] = len(models)
        for name, files, materials, animates, moves, ends in models:
            declared = generator_module.case(
                arguments.gen_commit, group, name, files, materials, animates, moves, ends, stated,
                arguments.gen_pinned_on, arguments.gen_pin_reason,
                generator_module.measured_fraction(arguments.gen_root, name))
            written.append(generator_module.write(declared, arguments.gen_root, name))
    _emit({"commit": arguments.gen_commit, "groups": groups, "cases": len(written),
           "root": arguments.gen_root})
    return 0

def _test262_cases(arguments):
    """Turn every declared container at the pin into one case directory each."""
    for name in ("t262_commit", "t262_root", "t262_pinned_on", "t262_pin_reason"):
        if getattr(arguments, name) is None:
            raise Refusal("test262-cases", expected="--" + name.replace("_", "-"), observed="absent")
    tests = test262_module.tests_at(arguments.t262_commit)
    written = []
    for test in tests:
        declared = test262_module.case(arguments.t262_commit, test, arguments.t262_pinned_on,
                                       arguments.t262_pin_reason)
        written.append(test262_module.write(declared, arguments.t262_root))
    _emit({"containers": len(test262_module.CONTAINERS), "commit": arguments.t262_commit,
           "tests": len(tests), "cases": len(written), "root": arguments.t262_root})
    return 0

def _prepare(manifest_path, arguments):
    declared = manifest_module.load(manifest_path)
    destination = arguments.dest or prepared_directory(manifest_path)
    os.makedirs(destination, exist_ok=True)
    if os.path.abspath(destination) != os.path.abspath(declared.directory):
        shutil.copyfile(manifest_path, os.path.join(destination, "manifest.json"))
    store = ContentStore(arguments.store, enabled=not arguments.no_cache)

    if arguments.job == "dry-run":
        _emit({"manifest": declared.id, "destination": destination, "store": store.directory,
               "plan": jobs.plan(declared, store)})
        return 0

    report = {}
    if arguments.job in ("fetch", "all"):
        report["fetch"] = jobs.fetch_subjects(declared, store, destination, force=arguments.force)

    needs_generation = any(subject.kind == "generated" for subject in declared.subjects)
    if arguments.job == "generate" or (arguments.job == "all" and needs_generation):
        report["generate"] = jobs.generate_subjects(declared, destination)

    needs_patching = any(subject.patch for subject in declared.subjects)
    if arguments.job == "patch" or (arguments.job == "all" and needs_patching):
        report["patch"] = jobs.patch_subjects(declared, destination)

    blender = None
    if declared.oracle and arguments.job in ("convert", "render", "all"):
        blender = blender_module.Blender(blender_module.locate(arguments.blender))
        notice = blender.against(declared.blender_version)
        if notice:
            print("notice: " + notice + " -- recorded, not refused", file=sys.stderr)

    needs_conversion = declared.oracle and any(subject.kind == "blend" for subject in declared.subjects)
    if declared.oracle and (arguments.job == "convert" or (arguments.job == "all" and needs_conversion)):
        report["convert"] = jobs.convert_blends(declared, store, blender, destination, force=arguments.force)

    if declared.oracle and arguments.job in ("render", "all"):
        report["render"] = jobs.render_oracle(
            declared, store, blender, destination, only=arguments.recipe, force=arguments.force
        )

    report["store"] = {"directory": store.directory, "hits": store.hits, "misses": store.misses,
                       "writes": store.writes}
    provenance = jobs.write_provenance(declared, store, destination, blender, report)
    _emit({"manifest": declared.id, "destination": destination, "provenance": provenance, "report": report})
    return 0

def _emit(document):
    json.dump(document, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")

if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Refusal as refusal:
        print(str(refusal), file=sys.stderr)
        sys.exit(1)
