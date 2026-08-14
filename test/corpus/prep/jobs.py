"""The jobs: fetch the subjects, generate the ones we own, convert a .blend, render the oracle."""

import json
import glob
import os
import tempfile

from . import blender as blender_module
from . import fetch, fixtures, licence, manifest as manifest_module, patch as patch_module
from .refusal import Refusal
from .store import derived_key, sha256_hex, sha256_of_file

HERE = os.path.dirname(os.path.abspath(__file__))
RENDER_SCRIPT = os.path.join(HERE, "in_blender_render.py")
CONVERT_SCRIPT = os.path.join(HERE, "in_blender_convert.py")

# THE PREPARER'S OWN CODE, DIGESTED INTO THE ORACLE KEY (board:1120). The key covered Blender, the
# subject pins, the declared scene and the recipe -- everything the manifest says and nothing the
# PREPARER does. But `in_blender_render.py` decides which passes are enabled, how materials are
# built, and where lights and cameras sit, and `exr.py` decides how the products are decoded on the
# way out. All of that changes what Cycles renders while the declaration is untouched.
#
# MEASURED, WHICH IS WHY IT IS HERE: enabling three render passes changed 8 of 58 beauty products
# under keys that still matched, up to 8 ulps where 256 samples accumulate. No verdict moved, and
# that was luck rather than design -- the drift was found by hashing before and after on a hunch,
# which is not an instrument.
#
# THE WHOLE FILE AND NOT A VERSION STRING: a version somebody has to remember to bump is a second
# fact about the code, kept by hand, and it goes stale exactly when it matters.
# EVERY SOURCE OF THE PREPARER, AND NOT A LIST. A named list is a second copy of a fact -- it was
# (render script, exr.py) while manifest.py declared QUANTITY_PASSES, so editing a quantity's socket
# changed the bytes and not the product name, and the key did not move. A list also has to agree with
# the one the harness holds, and it did not. Anything under this directory can change what lands on
# disk, so everything under it is digested and there is nothing to keep in step.
RENDER_CODE = tuple(sorted(glob.glob(os.path.join(HERE, "*.py"))))

PROVENANCE_NAME = "provenance.json"
# THE BEAUTY PAIR PLUS ONE PAIR PER QUANTITY (manifest.QUANTITY_PASSES). The beauty products keep the
# product names they have always had, so their cache keys are unchanged -- what misses is the QUANTITY
# keys, which never existed, and a miss on any product re-runs the one render that produces them all.
PRODUCTS = ("exr", "raw") + tuple(
    q + suffix for q in manifest_module.QUANTITY_PASSES for suffix in ("Exr", "Raw")
)


def render_code_digest():
    """One digest over every source that runs inside Blender or decodes what comes out."""
    return sha256_hex(b"".join(open(path, "rb").read() for path in sorted(RENDER_CODE)))


def plan(manifest, store):
    """What the run costs cold, published before it starts spending it."""
    rows = []
    total = 0
    for subject in manifest.subjects:
        for file in subject.files:
            if subject.kind == "generated":
                rows.append({"subject": subject.id, "as": file["as"], "generator": file["generator"]})
                continue
            cached = store.enabled and os.path.isfile(store.path(file["sha256"]))
            rows.append(
                {"subject": subject.id, "as": file["as"], "url": file["url"],
                 "member": file.get("member"), "bytes": file["bytes"], "cached": cached}
            )
            if not cached:
                total += file["bytes"]
    # THE COST IS RECIPES TIMES FRAMES AND NOT RECIPES. A sequence renders its whole declared grid
    # per recipe (board:1129), so a plan that counted recipes alone would understate a 31-frame case
    # by a factor of 31 -- and this exists to publish what a cold run costs before it spends it.
    frames = manifest.frame_grid()
    return {"files": rows, "bytesToFetch": total, "renders": sorted(manifest.renders),
            "framesPerRecipe": len(frames), "rendersToRun": len(manifest.renders) * len(frames)}


def generate_subjects(manifest, destination):
    """The fixtures we own. Written every run rather than cached: the recipe is a few hundred bytes
    of manifest and the product is a few kilobytes, so a cache here would only be a second place the
    bytes could be stale."""
    report = []
    for subject in manifest.subjects:
        if subject.kind != "generated":
            continue
        for file in subject.files:
            where = "manifest subject %s file %s generator" % (subject.id, file["as"])
            produced, said = fixtures.generate(where, file["generator"])
            path = os.path.join(destination, file["as"])
            with open(path, "wb") as out:
                out.write(produced)
            made = {"subject": subject.id, "as": file["as"], "bytes": len(produced),
                    "sha256": sha256_hex(produced)}
            # WHAT THE PRODUCER SAID ABOUT WHAT IT MADE, into provenance.json. For a part the engine
            # grew this is the only place the counts, the solved DBH and the framing the rule gives
            # it are written down, and a manifest quoting one of them can be checked against it.
            if said:
                made["reported"] = said
            report.append(made)
    if not report:
        raise Refusal("generate " + manifest.id, why="no subject is generated; there is nothing to make")
    return report


def fetch_subjects(manifest, store, destination, force=False):
    report = {"files": [], "licence": []}
    for subject in manifest.subjects:
        if subject.kind == "generated":
            continue
        for file in subject.files:
            how, size = fetch.download_to_store(
                file["url"],
                file["sha256"],
                store,
                expected_bytes=file["bytes"],
                commit=subject.source.get("commit"),
                member=file.get("member"),
            )
            target = os.path.join(destination, file["as"])
            placed = "kept"
            if force or not _matches(target, file["sha256"]):
                store.copy_out(file["sha256"], target)
                placed = "placed"
            report["files"].append(
                {"subject": subject.id, "as": file["as"], "bytes": size, "source": how, "destination": placed}
            )
        report["licence"].extend(_check_licences(subject, store))
    return report


def _check_licences(subject, store):
    """Per file. A repository-level claim covers nothing in particular, so nothing inherits one."""
    derived = None
    if subject.source["kind"] == "khronos-sample-assets":
        metadata = subject.metadata_file()
        if metadata is None:
            raise Refusal(
                "manifest subject " + subject.id,
                expected="a file with role metadata (the model's metadata.json at the pin)",
                observed="none",
                why="the licence is derived from upstream, never transcribed",
            )
        derived = licence.spdx_from_khronos_metadata(store.read(metadata["sha256"]), subject.name)
        for entry in derived:
            licence.check_spdx(entry["spdx"], subject.name + " metadata.json")

    checked = []
    for file in subject.files:
        declared = file["licence"] if isinstance(file["licence"], list) else [file["licence"]]
        for entry in declared:
            licence.check_spdx(entry["spdx"], file["as"])
        if derived is not None:
            licence.check_declared_matches_derived(declared, derived, file["as"])
        checked.append(
            {"subject": subject.id, "as": file["as"],
             "spdx": sorted(set(e["spdx"] for e in declared)),
             "verifiedAgainst": "upstream metadata.json" if derived is not None else "manifest statedAt"}
        )
    return checked


def patch_subjects(manifest, destination):
    """The declared corrections, applied to the fetched bytes and never to the pin.

    It runs AFTER the fetch and BEFORE the oracle, which is what makes both sides take it: the
    runner loads the files in this directory and the oracle is rendered from the same ones.
    """
    report = []
    for subject in manifest.subjects:
        if not subject.patch:
            continue
        report.extend(patch_module.apply(subject, destination))
    if not report:
        raise Refusal("patch " + manifest.id, why="no subject declares a patch; there is nothing to correct")
    return report


def convert_blends(manifest, store, blender, destination, force=False):
    results = []
    for subject in manifest.subjects:
        if subject.kind != "blend":
            continue
        results.append(_convert(subject, manifest, store, blender, destination, force))
    if not results:
        raise Refusal(
            "convert " + manifest.id, why="no subject is a .blend; there is nothing to convert"
        )
    return results


def _convert(subject, manifest, store, blender, destination, force):
    blend = _file_named(subject, subject.entry)
    output_name = subject.conversion.settings["outputName"]
    recipe = {
        "blenderDeclared": manifest.blender_version,
        "blenderObserved": blender.version,
        "blenderBuildHash": blender.build_hash,
        "blendSha256": blend["sha256"],
        "exportSettings": subject.conversion.settings["exportSettings"],
        "frameStart": subject.conversion.settings["frameStart"],
        "product": output_name,
    }
    key = derived_key("gltf.from.blend", recipe)
    target = os.path.join(destination, output_name)
    if not force and store.has(key) and _matches(target, sha256_of_file(store.path(key))):
        return {"subject": subject.id, "outputName": output_name, "cache": "hit", "key": key, "provenance": None}

    with tempfile.TemporaryDirectory(prefix="outshine-convert-") as work:
        job_path = os.path.join(work, "job.json")
        output_path = os.path.join(work, output_name)
        with open(job_path, "w") as f:
            json.dump(
                {
                    "exportSettings": subject.conversion.settings["exportSettings"],
                    "frameStart": subject.conversion.settings["frameStart"],
                    "outputPath": output_path,
                    "provenanceOpen": blender_module.PROVENANCE_OPEN,
                    "provenanceClose": blender_module.PROVENANCE_CLOSE,
                },
                f,
            )
        provenance = blender.run(CONVERT_SCRIPT, job_path, blend_file=os.path.join(destination, blend["as"]))
        if not os.path.isfile(output_path):
            raise Refusal("convert " + subject.id, expected=output_name, observed="the exporter wrote no such file")
        store.keep_file(key, output_path)
    store.copy_out(key, target)
    return {"subject": subject.id, "outputName": output_name, "cache": "miss", "key": key, "provenance": provenance}


def render_oracle(manifest, store, blender, destination, only=None, force=False):
    gltf_paths = []
    for name in manifest.gltf_names():
        path = os.path.join(destination, name)
        if not os.path.isfile(path):
            raise Refusal(
                "render " + manifest.id,
                expected=path,
                observed="absent",
                why="every subject is fetched, and converted if it is a .blend, before it is rendered",
            )
        gltf_paths.append(path)

    subject_pin = []
    for subject in manifest.subjects:
        # A fetched file's digest is the manifest's declared pin; a generated one has no pin to
        # declare, so the key carries the digest of what the generator actually produced -- which is
        # what makes a generator that moved under an unchanged manifest miss the cache.
        # A PATCHED SUBJECT PINS WHAT IS ON DISK AND NOT WHAT WAS FETCHED, and it carries the patch
        # declaration too: the first makes a changed transform IMPLEMENTATION miss the cache, the
        # second makes a changed DECLARATION miss it, and either alone would serve a stale oracle.
        pin = {"id": subject.id, "entry": subject.entry,
               "files": sorted([f["as"], sha256_of_file(os.path.join(destination, f["as"]))
                                if subject.patch else
                                (f.get("sha256") or sha256_of_file(os.path.join(destination, f["as"])))]
                               for f in subject.files)}
        if subject.patch:
            pin["patch"] = subject.patch
        if subject.kind == "blend":
            pin["gltfSha256"] = sha256_of_file(
                os.path.join(destination, subject.conversion.settings["outputName"])
            )
        subject_pin.append(pin)

    results = []
    for name in sorted(manifest.renders):
        if only and name not in only:
            continue
        for frame in manifest.frame_grid():
            results.append(_render_one(manifest, store, blender, destination, gltf_paths,
                                       subject_pin, name, frame, force))
    return results


def _render_one(manifest, store, blender, destination, gltf_paths, subject_pin, name, frame, force):
    recipe = manifest.renders[name]
    names = manifest_module.output_names_for(name, frame)
    targets = {product: os.path.join(destination, filename) for product, filename in names.items()}
    products = tuple(names)
    recipe_key = {
        # The pinned Blender is in the key because a point release that changes Cycles
        # must miss. Both the declared and the observed version are here: the first
        # catches a manifest bump on an unchanged host, the second a host that moved
        # under an unchanged manifest, and one of the two alone catches neither.
        "blenderDeclared": manifest.blender_version,
        "blenderObserved": blender.version,
        "blenderBuildHash": blender.build_hash,
        "subjects": subject_pin,
        "scene": manifest.scene.as_job(),
        "recipe": recipe,
        # board:1120 -- see RENDER_CODE above.
        "preparer": render_code_digest(),
    }
    # THE FRAME IS PART OF THE KEY (board:1128), and it is absent from a still's key rather than
    # null in it: a case with no animation declares no frame, and its products keep the keys the
    # corpus already computed for them.
    if frame is not None:
        recipe_key["frame"] = frame
    keys = {product: derived_key("oracle." + product, dict(recipe_key, product=product))
            for product in products}
    stored = all(store.has(key) for key in keys.values())
    placed = stored and all(_matches(targets[p], sha256_of_file(store.path(keys[p]))) for p in targets)
    row = {"recipe": name, "keys": keys}
    if frame is not None:
        row["frame"] = frame
    if not force and placed:
        return dict(row, cache="hit", products=_sizes(targets), provenance=None)
    provenance = None
    if force or not stored:
        provenance = _run_render(manifest, blender, gltf_paths, recipe, keys, store, products, frame)
    for product in targets:
        store.copy_out(keys[product], targets[product])
    return dict(row, cache="miss" if provenance else "hit", products=_sizes(targets),
                provenance=provenance)


def _run_render(manifest, blender, gltf_paths, recipe, keys, store, products, frame):
    with tempfile.TemporaryDirectory(prefix="outshine-oracle-") as work:
        paths = {product: os.path.join(work, "oracle." + product) for product in products}
        job_path = os.path.join(work, "job.json")
        with open(job_path, "w") as f:
            json.dump(
                {"gltfPaths": gltf_paths, "scene": manifest.scene.as_job(), "recipe": recipe,
                 "frame": frame,
                 "exrPath": paths["exr"], "rawPath": paths["raw"],
                 "quantityPasses": {q: spec for q, spec in manifest_module.QUANTITY_PASSES.items()
                                    if q + "Raw" in paths},
                 "quantityPaths": {q: {"exr": paths[q + "Exr"], "raw": paths[q + "Raw"]}
                                   for q in manifest_module.QUANTITY_PASSES if q + "Raw" in paths},
                 "provenanceOpen": blender_module.PROVENANCE_OPEN,
                 "provenanceClose": blender_module.PROVENANCE_CLOSE},
                f,
            )
        provenance = blender.run(RENDER_SCRIPT, job_path)
        for product in products:
            if not os.path.isfile(paths[product]):
                raise Refusal("render " + manifest.id, expected=product + " product", observed="no such file")
            store.keep_file(keys[product], paths[product])
    return provenance


def write_provenance(manifest, store, destination, blender, report):
    document = {
        "manifestId": manifest.id,
        "manifestSchemaVersion": manifest_module.SCHEMA_VERSION,
        "preparerDigest": render_code_digest(),
        "blenderDeclared": manifest.blender_version,
        "blenderObserved": blender.version if blender else None,
        "blenderBuildHash": blender.build_hash if blender else None,
        "versionNotice": blender.against(manifest.blender_version) if blender else None,
        "contentStore": store.directory,
        "report": report,
    }
    path = os.path.join(destination, PROVENANCE_NAME)
    with open(path, "w") as f:
        json.dump(document, f, indent=2, sort_keys=True)
        f.write("\n")
    return path


def _file_named(subject, name):
    for file in subject.files:
        if file["as"] == name:
            return file
    raise Refusal("manifest subject " + subject.id, expected="a declared file named " + name, observed="none")


def _matches(path, sha256):
    return os.path.isfile(path) and sha256_of_file(path) == sha256


def _sizes(targets):
    return {
        product: {"path": path, "bytes": os.path.getsize(path), "sha256": sha256_of_file(path)}
        for product, path in sorted(targets.items())
    }
