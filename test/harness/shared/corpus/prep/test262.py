"""The test262 slice, derived from the pin and from the subset's own vocabulary.

**THE POPULATION IS DECLARED AND THE SELECTION IS NOT.** test262 at this pin holds 11 164 files under
`language/expressions` alone, 4 059 of them about classes -- fetching a corpus twenty times the size of
the one this interpreter could possibly decide costs the network for nothing and measures the same
number. So the CONTAINERS are named here, and the name of each is a construct `board:1448` writes down
as INSIDE the subset: the directories test262 keeps its arithmetic, its comparisons, its logic, its
grouping, its assignment and its `if`, `while` and block statements in.

Within each container the enumeration is exhaustive, and a container that resolves to nothing is a
REFUSAL rather than a silent zero -- `does-not-equals` is spelled with an `s` upstream, and a list that
answered zero for a misspelling would report a capability gap that is a typing error.

**Whether a case is inside the subset is C++ and not here.** This module fetches and declares; the
harness decides, the same way the CSS corpus works.
"""

import hashlib
import json
import os
import re
import urllib.request

from .refusal import Refusal

RAW = "https://raw.githubusercontent.com/tc39/test262/"
TREE = "https://api.github.com/repos/tc39/test262/git/trees/"

# THE CONTAINERS, AND EVERY NAME IS A CONSTRUCT THE SUBSET DECLARES. Adding one is adding a capability
# to `board:1448` first; removing one is giving up scope, which is the owner's.
CONTAINERS = (
    "test/language/expressions/addition",
    "test/language/expressions/subtraction",
    "test/language/expressions/multiplication",
    "test/language/expressions/division",
    "test/language/expressions/modulus",
    "test/language/expressions/unary-minus",
    "test/language/expressions/unary-plus",
    "test/language/expressions/logical-not",
    "test/language/expressions/logical-and",
    "test/language/expressions/logical-or",
    "test/language/expressions/less-than",
    "test/language/expressions/less-than-or-equal",
    "test/language/expressions/greater-than",
    "test/language/expressions/greater-than-or-equal",
    "test/language/expressions/equals",
    "test/language/expressions/does-not-equals",
    "test/language/expressions/grouping",
    "test/language/expressions/assignment",
    "test/language/statements/if",
    "test/language/statements/while",
    "test/language/statements/block",
    "test/language/statements/empty",
    "test/language/statements/expression",
)

# WHAT test262 PREPENDS IS NOT FETCHED, AND THE REASON IS THE SUBSET ITSELF. `assert.js` and `sta.js`
# DEFINE FUNCTIONS, which `board:1448` writes down as outside -- so shipping them as script text would
# make every non-`raw` case fail at the parser for a reason that has nothing to do with the case. The
# harness implements their CONTRACT as host natives instead, and the case's own text is run unmodified.
#
# WHAT A CASE NEEDS IS STILL RECORDED, as an attribute rather than a file: a case whose `includes` names
# something the runner does not provide is OUTSIDE THE SUBSET and says which name put it there.

FRONTMATTER = re.compile(r"/\*---(.*?)---\*/", re.S)

LICENCE = {"spdx": "BSD-3-Clause", "holder": "Ecma International",
           "covers": "Everything"}


def _get(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read().decode("utf-8", "replace")


def _list(commit, directory):
    url = TREE + commit + ":" + directory.replace("/", "%2F") + "?recursive=1"
    listing = json.loads(_get(url))
    if "tree" not in listing:
        raise Refusal("the listing of " + directory,
                      expected="a directory at the pin",
                      observed=str(listing.get("message")),
                      why="a container that resolves to nothing would report a capability gap that "
                          "is a spelling mistake")
    if listing.get("truncated"):
        raise Refusal("the listing of " + directory, expected="the whole directory",
                      observed="a truncated tree",
                      why="a truncated enumeration is not an enumeration")
    return sorted(entry["path"] for entry in listing["tree"]
                  if entry["type"] == "blob" and entry["path"].endswith(".js"))


def _declared(text):
    """The case's own frontmatter, read for the four things a verdict needs."""
    found = FRONTMATTER.search(text)
    out = {"description": "", "flags": [], "includes": [], "negative": None}
    if found is None:
        return out
    body = found.group(1)
    for line in body.splitlines():
        stripped = line.strip()
        if stripped.startswith("description:"):
            out["description"] = stripped[len("description:"):].strip()
        elif stripped.startswith("flags:"):
            out["flags"] = re.findall(r"[A-Za-z-]+", stripped[len("flags:"):])
        elif stripped.startswith("includes:"):
            out["includes"] = re.findall(r"[\w.-]+\.js", stripped[len("includes:"):])
    negative = re.search(r"negative:\s*\n\s*phase:\s*(\w+)\s*\n\s*type:\s*(\w+)", body)
    if negative is not None:
        out["negative"] = {"phase": negative.group(1), "type": negative.group(2)}
    return out


def _fetched(commit, path, role, text):
    raw = text.encode("utf-8")
    return {"url": RAW + commit + "/" + path,
            "sha256": hashlib.sha256(raw).hexdigest(),
            "bytes": len(raw),
            "as": os.path.basename(path),
            "role": role,
            "licence": dict(LICENCE, statedAt=RAW + commit + "/LICENSE")}


def case(commit, path, pinned_on, pin_reason, harness=None):
    """One upstream test as a case."""
    text = _get(RAW + commit + "/" + path)
    declared = _declared(text)
    name = os.path.basename(path)[: -len(".js")]
    # THE CASE'S OWN NAME IS NOT UNIQUE ACROSS test262, so the id carries the container it came from --
    # `addition/S11.6.1_A1` and `subtraction/S11.6.2_A1` are two cases with one basename upstream.
    corner = path.split("/")[-2]
    identity = corner + "-" + name

    del harness
    files = [_fetched(commit, path, "script", text)]
    attributes = ["flags:" + one for one in declared["flags"]]
    attributes += ["includes:" + one for one in declared["includes"]]

    criterion = {
        "kind": "script-expectation",
        "says": declared["description"] or "the case runs to its end without a refusal",
        "statedAt": "https://github.com/tc39/test262/blob/" + commit + "/" + path,
        "expects": "refuses" if declared["negative"] else "runs",
    }
    if declared["negative"]:
        criterion["says"] = declared["description"] or (
            "the case is refused, and the refusal is a " + declared["negative"]["type"])
        criterion["phase"] = declared["negative"]["phase"]
        criterion["kind"] = "script-expectation"
    return {
        "schema": "outshine/declared-case-manifest",
        "schemaVersion": 1,
        "id": identity,
        "title": declared["description"][:200] or identity,
        "covers": ["ecma262"],
        "criterion": criterion,
        "viewport": {"widthPx": 0, "heightPx": 0,
                     "note": "a script draws nothing, and a viewport of no pixels says so rather "
                             "than inventing a surface a case never asked for"},
        "subjects": [{
            "kind": "script",
            "id": identity,
            "name": name,
            "source": {"kind": "test262", "commit": commit, "path": path,
                       "pinnedOn": pinned_on, "pinReason": pin_reason},
            "files": files,
            "entry": os.path.basename(path),
            # AN EMPTY LIST IS NOT THE SAME AS NO LIST, and the schema says so: a case that needs
            # nothing beyond the runner carries no `attributes` key at all rather than an empty one.
            **({"attributes": attributes} if attributes else {}),
        }],
    }


def write(declaration, root):
    directory = os.path.join(root, declaration["id"])
    os.makedirs(directory, exist_ok=True)
    path = os.path.join(directory, "manifest.json")
    with open(path, "w") as f:
        json.dump(declaration, f, indent=2)
        f.write("\n")
    return path


def tests_at(commit):
    """Every file of every declared container, exhaustively, with its container's path restored."""
    found = []
    for container in CONTAINERS:
        for inner in _list(commit, container):
            if "/" in inner:
                continue
            found.append(container + "/" + inner)
    return sorted(found)
