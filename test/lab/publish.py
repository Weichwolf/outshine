"""THE GATE ON `build/shots/lab/`: a picture is published only if its case is GREEN.

The lab's pictures were copied there by hand, which meant a sheet from a run with a red case sat
beside a sheet from a green one and nothing said which was which. Two of them were broken -- the
Gradiente at Lombard Street ran in three disconnected pieces and the Goeltzschtalbruecke's jumped
ten metres back over ground it had already drawn -- and they were published because a person moved
a file. That is the whole reason this exists:

ONE DIRECTORY PER AREA -- `roads`, `infra`, `buildings`, `landmarks`, `roofs`, `places` -- so a
reader opens the one thing they came for rather than scrolling three hundred files in one list.

    A PICTURE IS EVIDENCE ONLY IF ITS CASE PASSED. `take()` copies on green and DELETES on red, so
    a case that breaks removes its own old picture rather than leaving yesterday's good one
    standing under today's broken code
    THE NAME IS THE CASE, NEVER AN ORDINAL. The beds numbered their sheets `enumerate(picked)`,
    so `python3 real.py Lombard` wrote `01_...` and a full run wrote `03_...`: the directory grew
    two files for one case and three roofs shared the prefix `G02`. A published name is derived
    from the case alone and a rerun overwrites exactly what it wrote before
    AN UNFILTERED RUN SWEEPS ITS OWN KIND. Otherwise a case DELETED from a bed keeps its picture
    for ever, and the directory slowly fills with evidence for cases that no longer exist

None of this makes a picture CORRECT. A green case with a wrong oracle publishes a wrong picture,
which is why every published sheet is also looked at -- `LOOKED.md` beside them is that record, and
a picture nobody has looked at is not a regression reference yet.
"""
import pathlib
import shutil

ROOT = pathlib.Path(__file__).resolve().parents[2]
SHOTS = ROOT / "build" / "shots" / "lab"


def _safe(text):
    keep = [c if (c.isalnum() or c in "-_.,") else "_" for c in str(text)]
    return "".join(keep).strip("_")


def sweep(area):
    """Drop every picture of one area, for a run that is about to rebuild all of them."""
    here = SHOTS / area
    if here.is_dir():
        for old in here.glob("*.png"):
            old.unlink()


def take(area, key, path, red):
    """Publish one picture, or remove what stands in its place. Returns the published path, or
    None when the case was red."""
    here = SHOTS / area
    here.mkdir(parents=True, exist_ok=True)
    out = here / f"{_safe(key)}.png"
    if red or path is None:
        out.unlink(missing_ok=True)
        return None
    shutil.copyfile(path, out)
    return out
