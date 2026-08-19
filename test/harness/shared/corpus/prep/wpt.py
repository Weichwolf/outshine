"""The WPT slice, derived from the pin rather than chosen.

WHAT THIS PRODUCES IS A DECLARATION AND NOT A PRODUCT. One directory per upstream test, each holding
a manifest that names the pin, the files and what the case is about -- the same shape the picture
corpus uses, so one enumeration reaches both suites and the viewer needs no second reader.

IT SELECTS NOTHING. Which of these the UI library can actually hold is a question about the subset,
the subset is C++, and a `grep` for a property name would answer a different question -- a shorthand
it does not expand, a selector it reads by shape. So every test in the named directory that STATES
its own layout arrives here, and the run publishes how much of it the subset reaches.
"""

import hashlib
import json
import os
import re
import urllib.request

from .refusal import Refusal

RAW = "https://raw.githubusercontent.com/web-platform-tests/wpt/"
TREE = "https://api.github.com/repos/web-platform-tests/wpt/git/trees/"

ASSERTION = re.compile(r"data-expected-(width|height)|data-offset-(x|y)")
STYLESHEET = re.compile(r"""<link\b[^>]*\brel\s*=\s*["']?stylesheet["']?[^>]*>""", re.I)
HREF = re.compile(r"""\bhref\s*=\s*["']([^"']+)["']""", re.I)
URL_IN_CSS = re.compile(r"""url\(\s*["']?([^"')]+)["']?\s*\)""")
TITLE = re.compile(r"<title>(.*?)</title>", re.I | re.S)
PUBLIC_DOMAIN = re.compile(r"copyright is dedicated to the Public Domain", re.I)

# THE PROJECT'S OWN STATEMENT, and it is the only one most of these files carry. `LICENSE.md` at the
# pin offers the 3-Clause BSD and the W3C Software and Document Licence together; a file that
# dedicates itself to the public domain in its own header says so and is recorded as CC0.
PROJECT_LICENCE = {"spdx": "BSD-3-Clause OR LicenseRef-W3C-Software-and-Document",
                   "holder": "web-platform-tests contributors",
                   "covers": "Everything"}


def _get(url, binary=False):
    with urllib.request.urlopen(url, timeout=60) as response:
        raw = response.read()
    return raw if binary else raw.decode("utf-8", "replace")


def tests_in(commit, directory):
    """Every file the named directory holds at the pin, exhaustively -- the tree endpoint says so."""
    listing = json.loads(_get(TREE + commit + ":" + directory.replace("/", "%2F") + "?recursive=1"))
    if listing.get("truncated"):
        raise Refusal("the listing of " + directory,
                      expected="the whole directory",
                      observed="a truncated tree",
                      why="a truncated enumeration is not an enumeration, and a selection drawn "
                          "from one is a curation nobody declared")
    return sorted(entry["path"] for entry in listing["tree"]
                  if entry["type"] == "blob" and entry["path"].endswith(".html")
                  and "/" not in entry["path"])


def _resolve(reference, directory):
    if reference.startswith("/"):
        return reference[1:]
    if "://" in reference:
        return None
    return os.path.normpath(os.path.join(directory, reference))


def _fetched(commit, path, role, text):
    raw = text.encode("utf-8") if isinstance(text, str) else text
    licence = dict(PROJECT_LICENCE, statedAt=RAW + commit + "/LICENSE.md")
    if isinstance(text, str) and PUBLIC_DOMAIN.search(text):
        licence = {"spdx": "CC0-1.0", "holder": "the file's own header", "covers": "Everything",
                   "statedAt": RAW + commit + "/" + path}
    return {"url": RAW + commit + "/" + path,
            "sha256": hashlib.sha256(raw).hexdigest(),
            "bytes": len(raw),
            "as": os.path.basename(path),
            "role": role,
            "licence": licence}


def _companions(commit, document, directory, seen):
    """Every file the document's own elements reach for, one level of `url()` behind a stylesheet."""
    files = []
    for element in STYLESHEET.findall(document):
        reference = HREF.search(element)
        if reference is None:
            continue
        path = _resolve(reference.group(1), directory)
        if path is None or path in seen:
            continue
        seen.add(path)
        sheet = _get(RAW + commit + "/" + path)
        files.append(_fetched(commit, path, "stylesheet", sheet))
        for inner in URL_IN_CSS.findall(sheet):
            nested = _resolve(inner, os.path.dirname(path))
            if nested is None or nested in seen:
                continue
            seen.add(nested)
            files.append(_fetched(commit, nested, "font", _get(RAW + commit + "/" + nested, binary=True)))
    return files


def case(commit, directory, test, pinned_on, pin_reason, viewport):
    """One upstream test as a case, or None when it states nothing this suite can decide."""
    document = _get(RAW + commit + "/" + directory + "/" + test)
    if not ASSERTION.search(document):
        return None
    name = test[:-len(".html")]
    title = TITLE.search(document)
    files = [_fetched(commit, directory + "/" + test, "document", document)]
    files += _companions(commit, document, directory, {directory + "/" + test})
    return {
        "schema": "outshine/declared-case-manifest",
        "schemaVersion": 1,
        "id": name,
        "title": (title.group(1).strip().replace("\n", " ") if title else name),
        "covers": ["css-flexbox-1"],
        "criterion": {
            "kind": "layout-assertions",
            "says": "every element carrying data-expected-width, data-expected-height, data-offset-x "
                    "or data-offset-y lands at exactly that width, height and viewport offset in CSS pixels",
            "statedAt": "https://github.com/web-platform-tests/wpt/blob/" + commit + "/" + directory + "/" + test,
        },
        "viewport": {"widthPx": viewport[0], "heightPx": viewport[1],
                     "note": "the reftest viewport WPT lays its own references out in"},
        "subjects": [{
            "kind": "document",
            "id": name,
            "name": name,
            "source": {"kind": "web-platform-tests", "commit": commit,
                       "path": directory + "/" + test,
                       "pinnedOn": pinned_on, "pinReason": pin_reason},
            "files": files,
            "entry": test,
        }],
    }


def write(case_declaration, root):
    directory = os.path.join(root, case_declaration["id"])
    os.makedirs(directory, exist_ok=True)
    path = os.path.join(directory, "manifest.json")
    with open(path, "w") as f:
        json.dump(case_declaration, f, indent=2)
        f.write("\n")
    return path
