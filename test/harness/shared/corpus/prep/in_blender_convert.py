"""Runs inside Blender: .blend to glTF, every export setting written out."""

import json
import sys

import bpy

def fail(message):
    print("in_blender_convert: " + message, file=sys.stderr)
    sys.exit(9)

def job_from_argv():
    if "--" not in sys.argv:
        fail("no job file after --")
    with open(sys.argv[sys.argv.index("--") + 1], "r") as f:
        return json.load(f)

def export_properties():
    try:
        bpy.ops.preferences.addon_enable(module="io_scene_gltf2")
    except Exception:
        pass
    return set(bpy.ops.export_scene.gltf.get_rna_type().properties.keys())

def main():
    job = job_from_argv()
    known = export_properties()
    unknown = sorted(k for k in job["exportSettings"] if k not in known)
    if unknown:
        fail("the exporter has no such setting(s): " + ", ".join(unknown))

    scene = bpy.context.scene
    before_start = scene.frame_start
    scene.frame_start = job["frameStart"]

    settings = dict(job["exportSettings"])
    settings["filepath"] = job["outputPath"]
    result = bpy.ops.export_scene.gltf(**settings)
    if "FINISHED" not in result:
        fail("glTF export returned " + repr(result))

    provenance = {
        "blenderVersion": bpy.app.version_string,
        "blenderBuildHash": bpy.app.build_hash.decode() if isinstance(bpy.app.build_hash, bytes) else str(bpy.app.build_hash),
        "blendFile": bpy.data.filepath,
        "frameStartBefore": before_start,
        "frameStartUsed": scene.frame_start,
        "frameEnd": scene.frame_end,
        "fps": scene.render.fps,
        "fpsBase": scene.render.fps_base,
        "secondsPerFrame": scene.render.fps_base / scene.render.fps,
        "objects": sorted(obj.name for obj in bpy.data.objects),
        "meshObjects": sorted(obj.name for obj in bpy.data.objects if obj.type == "MESH"),
        "appliedSettings": {k: _plain(v) for k, v in sorted(settings.items())},
    }
    print(job["provenanceOpen"] + json.dumps(provenance) + job["provenanceClose"])

def _plain(value):
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)

main()
