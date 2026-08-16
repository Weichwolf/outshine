"""What may enter the corpus, decided per file."""

import json

from .refusal import Refusal

ALLOWED_SPDX = frozenset(["CC0-1.0", "CC-BY-4.0"])

# Khronos's, Cesium's, UX3D's and DGG's statements that a logo is non-copyrightable. They carry no
# obligation, so they are allowed by prefix rather than enumerated as they appear.
ALLOWED_SPDX_PREFIXES = ("LicenseRef-LegalMark-",)

# Refused once, on evidence, and refused again the next time somebody declares them. Standing here
# rather than in a reviewer's memory is the difference between a rule and a habit.
REFUSED_SUBJECTS = {
    "Sponza": "Crytek Cryengine Limited License Agreement -- a proprietary EULA, not a free licence",
    "BrainStem": "Smith Micro Poser EULA",
    "DamagedHelmet": "textures are CC BY-NC 4.0 (theblueturtle_, 2016) -- non-commercial",
    "BoxTextured": "CC-BY 4.0 with trademark limitations, plus a Cesium trademark",
    "BoxTexturedNonPowerOfTwo": "on Khronos's own Models-issues.md licence/ownership list",
    "AntiqueCamera": "on Khronos's own Models-issues.md licence/ownership list",
    "CesiumMan": "on Khronos's own Models-issues.md licence/ownership list",
    "CesiumMilkTruck": "on Khronos's own Models-issues.md licence/ownership list",
    "PrimitiveModeNormalsTest": "on Khronos's own Models-issues.md licence/ownership list",
    "RecursiveSkeletons": "on Khronos's own Models-issues.md licence/ownership list",
    "Duck": "SCEA Shared Source License 1.0 (Sony, 2006)",
}


def check_subject_name(name):
    reason = REFUSED_SUBJECTS.get(name)
    if reason:
        raise Refusal("subject " + name, why=reason)


def declared_grants(field):
    """A file's declared licences, whether the manifest spelled one or several.

    ONE PLACE, because a file under two grants -- MultiUVTest's CC-BY and Khronos's logo mark -- has
    to declare both, and the normalisation was written twice with only one of the two copies knowing
    a list was possible.
    """
    return list(field) if isinstance(field, list) else [field]


def check_spdx(spdx, where):
    if spdx in ALLOWED_SPDX:
        return
    for prefix in ALLOWED_SPDX_PREFIXES:
        if spdx.startswith(prefix):
            return
    raise Refusal(
        "licence of " + where,
        expected="one of " + ", ".join(sorted(ALLOWED_SPDX)) + " or a LicenseRef-LegalMark-* mark",
        observed=spdx,
    )


def spdx_from_khronos_metadata(metadata_bytes, where):
    """The licence is read at the pin, never transcribed -- a change upstream is then a refusal."""
    try:
        document = json.loads(metadata_bytes.decode("utf-8"))
    except (ValueError, UnicodeDecodeError) as error:
        raise Refusal("metadata.json of " + where, why="not readable as JSON: " + str(error))
    legal = document.get("legal")
    if not isinstance(legal, list) or not legal:
        raise Refusal(
            "metadata.json of " + where,
            expected="a non-empty legal array",
            observed=repr(legal),
            why="an asset whose licence is not stated is out",
        )
    found = []
    for entry in legal:
        spdx = entry.get("spdx")
        if not spdx:
            raise Refusal(
                "metadata.json of " + where,
                expected="every legal entry to carry an spdx identifier",
                observed=canonical_entry(entry),
            )
        found.append(
            {
                "spdx": spdx,
                "holder": entry.get("artist") or entry.get("owner") or "",
                "year": str(entry.get("year") or ""),
                "covers": entry.get("what") or "",
            }
        )
    return found


def canonical_entry(entry):
    return json.dumps(entry, sort_keys=True, separators=(",", ":"))


def check_declared_matches_derived(declared, derived, where):
    """A declaration nobody checks is a transcription, and a transcription drifts."""
    declared_set = sorted(set(item["spdx"] for item in declared))
    derived_set = sorted(set(item["spdx"] for item in derived))
    if declared_set != derived_set:
        raise Refusal(
            "licence of " + where,
            expected="manifest declares " + ", ".join(declared_set),
            observed="upstream states " + ", ".join(derived_set),
            why="the pin moved, or the manifest was written from memory",
        )
