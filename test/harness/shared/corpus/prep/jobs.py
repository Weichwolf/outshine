"""The jobs: fetch the subjects, generate the ones we own, convert a .blend, render the oracle."""

import json
import sys
import glob
import os
import tempfile

from . import blender as blender_module
from . import licence, manifest as manifest_module, patch as patch_module, vendor
from .refusal import Refusal
from .store import canonical_json, derived_key, sha256_hex, sha256_of_file

HERE = os.path.dirname(os.path.abspath(__file__))
RENDER_SCRIPT = os.path.join(HERE, "in_blender_render.py")
CONVERT_SCRIPT = os.path.join(HERE, "in_blender_convert.py")

SHARED_CODE = tuple(sorted(
    glob.glob(os.path.join(vendor.harness_root(HERE), "shared", "**", "*.py"), recursive=True)))

def _vendor_code(case_directory):
    """Every `.py` of the harness serving this case, or none where it is the shared one itself."""
    harness = vendor.harness_of(case_directory)
    shared = os.path.join(vendor.harness_root(HERE), "shared")
    if os.path.abspath(harness) == os.path.abspath(shared):
        return ()
    return tuple(sorted(glob.glob(os.path.join(harness, "**", "*.py"), recursive=True)))

PROVENANCE_NAME = "provenance.json"
PRODUCTS = ("exr", "raw") + tuple(
    q + suffix for q in manifest_module.QUANTITY_PASSES for suffix in ("Exr", "Raw")
)

def render_code_digest(case_directory):
    """One digest over the preparer that can actually reach this case.

    The order is part of the digest and is stated identically here and in the C++ check, or the two
    never agree: shared first in sorted path order, then the vendor's, also sorted.
    """
    paths = list(SHARED_CODE) + list(_vendor_code(case_directory))
    return sha256_hex(b"".join(open(path, "rb").read() for path in paths))

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
            produced, said = vendor.step(manifest.directory, "fixtures").generate(where, file["generator"])
            path = os.path.join(destination, file["as"])
            with open(path, "wb") as out:
                out.write(produced)
            made = {"subject": subject.id, "as": file["as"], "bytes": len(produced),
                    "sha256": sha256_hex(produced)}
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
            how, size = vendor.step(manifest.directory, "fetch").download_to_store(
                file["url"],
                file["sha256"],
                store,
                expected_bytes=file["bytes"],
                commit=subject.source.get("commit"),
                member=file.get("member"),
            )
            target = os.path.join(destination, file["as"])
            placed = "kept"
            packed = file.get("unpack")
            if packed is None:
                if force or not _matches(target, file["sha256"]):
                    store.copy_out(file["sha256"], target)
                    placed = "placed"
            elif force or not os.path.exists(target):
                vendor.step(manifest.directory, "fetch").unpack(
                    packed, store.path(file["sha256"]), target)
                placed = "unpacked"
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
            spdxNote = licence.check_spdx(entry["spdx"], subject.name + " metadata.json")

            if spdxNote:

                print("notice: " + spdxNote + " -- recorded, not refused", file=sys.stderr)

    checked = []
    for file in subject.files:
        declared = licence.declared_grants(file["licence"])
        for entry in declared:
            spdxNote = licence.check_spdx(entry["spdx"], file["as"])

            if spdxNote:

                print("notice: " + spdxNote + " -- recorded, not refused", file=sys.stderr)
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
    recipe_key = {
        "blenderDeclared": manifest.blender_version,
        "blenderObserved": blender.version,
        "blenderBuildHash": blender.build_hash,
        "subjects": subject_pin,
        "scene": manifest.scene.as_job(),
        "recipe": recipe,
        "preparer": render_code_digest(manifest.directory),
    }
    if frame is not None:
        recipe_key["frame"] = frame
    keys = {product: derived_key("oracle." + product, dict(recipe_key, product=product))
            for product in names}
    provenance_key = derived_key("oracle.provenance", dict(recipe_key, product="provenance"))
    stored = all(store.has(key) for key in keys.values()) and store.has(provenance_key)
    placed = stored and all(_matches(targets[p], sha256_of_file(store.path(keys[p]))) for p in targets)
    row = {"recipe": name, "keys": keys, "provenanceKey": provenance_key}
    if frame is not None:
        row["frame"] = frame
    rendered = bool(force) or not stored
    if rendered:
        _run_render(manifest, blender, gltf_paths, recipe, keys, provenance_key, store, frame)
    if rendered or not placed:
        for product in targets:
            store.copy_out(keys[product], targets[product])
    account = _stored_provenance(store, provenance_key, manifest)
    for key in keys.values():
        store.forget(key)
    store.forget(provenance_key)
    return dict(row, cache="miss" if rendered else "hit", products=_sizes(targets),
                provenance=account)

def _stored_provenance(store, key, manifest):
    document = store.read(key)
    if document is None:
        raise Refusal(
            "render " + manifest.id,
            expected="the render's own account under key " + key,
            observed="no such object in the content store",
            why="a render's products and its account are stored together or the render is redone",
        )
    try:
        return json.loads(document)
    except ValueError as error:
        raise Refusal("render " + manifest.id, expected="the account under key " + key + " as JSON",
                      observed=str(error))

def _run_render(manifest, blender, gltf_paths, recipe, keys, provenance_key, store, frame):
    with tempfile.TemporaryDirectory(prefix="outshine-oracle-") as work:
        paths = {product: os.path.join(work, "oracle." + product) for product in keys}
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
        for product in keys:
            if not os.path.isfile(paths[product]):
                raise Refusal("render " + manifest.id, expected=product + " product", observed="no such file")
            store.keep_file(keys[product], paths[product])
    store.keep(provenance_key, canonical_json(provenance).encode("utf-8"))

def write_provenance(manifest, store, destination, blender, report):
    document = {
        "manifestId": manifest.id,
        "manifestSchemaVersion": manifest_module.SCHEMA_VERSION,
        "preparerDigest": render_code_digest(manifest.directory),
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
