"""What the corpus consumes, RECORDED per file rather than gated (board:1171).

THE OWNER'S RULING: licence does not gate this corpus, because the models are fetched and rendered and
never redistributed. What was a refusal is now a note -- read at the pin, carried into provenance, and
printed once so a run says what it consumed.

THE KNOWLEDGE BELOW IS KEPT AND NOT DELETED. Each entry was established on evidence and a deleted line
is scope given up; what changed is what the tree DOES with it. If anything is ever published from this
corpus, this list is the thing that says which models may not go."""

import json

from .refusal import Refusal

ALLOWED_SPDX = frozenset(["CC0-1.0", "CC-BY-4.0"])

# Khronos's, Cesium's, UX3D's and DGG's statements that a logo is non-copyrightable. They carry no
# obligation, so they are allowed by prefix rather than enumerated as they appear.
ALLOWED_SPDX_PREFIXES = ("LicenseRef-LegalMark-",)

# Established once, on evidence. NOT a gate any more (board:1171) and still the record: standing here
# rather than in a reviewer's memory is the difference between a fact and a habit.
NOTED_SUBJECTS = {
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
    """The note this subject carries, or None. It is RECORDED and never refused (board:1171)."""
    return NOTED_SUBJECTS.get(name)


def declared_grants(field):
    """A file's declared licences, whether the manifest spelled one or several.

    ONE PLACE, because a file under two grants -- MultiUVTest's CC-BY and Khronos's logo mark -- has
    to declare both, and the normalisation was written twice with only one of the two copies knowing
    a list was possible.
    """
    return list(field) if isinstance(field, list) else [field]


def check_spdx(spdx, where):
    """The note this grant carries, or None. RECORDED and never refused (board:1171).

    `ALLOWED_SPDX` keeps its name and its meaning -- these are the grants that carry no obligation this
    tree would have to meet if it ever published anything. Everything else is consumed and said aloud.
    """
    if spdx in ALLOWED_SPDX:
        return None
    for prefix in ALLOWED_SPDX_PREFIXES:
        if spdx.startswith(prefix):
            return None
    return "%s is %s, which is outside %s" % (where, spdx, ", ".join(sorted(ALLOWED_SPDX)))


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
