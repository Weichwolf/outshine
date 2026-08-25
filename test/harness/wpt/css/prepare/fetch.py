"""Bytes from web-platform-tests, under a pin the manifest states.

WHY THIS IS NOT THE OTHER FETCH STEP. The picture corpus reaches into release archives and asks for
byte ranges inside them; nothing here does. A WPT case is a handful of small text files at a commit,
so what this vendor needs is a plain request, a digest and a refusal -- and a step that carried the
zip machinery it never uses would be a dead path with a reason to fire.
"""

import os
import re
import tempfile
import urllib.request

from prep.refusal import Refusal
from prep.store import sha256_of_file

WPT_URL = re.compile(
    r"^https://raw\.githubusercontent\.com/web-platform-tests/wpt/([0-9a-f]{40})/(.+)$"
)

def check_url(url, commit=None):
    match = WPT_URL.match(url)
    if match is None:
        raise Refusal("fetch " + url,
                      expected="a web-platform-tests raw URL pinned to a 40-hex commit",
                      observed=url,
                      why="an unpinned source is a corpus that changes under a measurement")
    if commit is not None and match.group(1) != commit:
        raise Refusal("fetch " + url,
                      expected="the commit the subject declares, " + commit,
                      observed=match.group(1))
    return match

def stream_to_file(url, path):
    with urllib.request.urlopen(url, timeout=120) as response, open(path, "wb") as out:
        while True:
            chunk = response.read(1 << 16)
            if not chunk:
                break
            out.write(chunk)

def download_to_store(url, expected_sha256, store, expected_bytes=None, commit=None, member=None):
    """Fetch, hash, refuse on mismatch, and file the bytes under their own digest."""
    check_url(url, commit)
    if member is not None:
        raise Refusal("fetch " + url, expected="a whole file", observed="a member of " + str(member),
                      why="nothing upstream of this suite is an archive")
    if store.has(expected_sha256):
        return "cached", os.path.getsize(store.path(expected_sha256))
    handle, temp = tempfile.mkstemp(prefix="outshine-wpt-")
    os.close(handle)
    try:
        stream_to_file(url, temp)
        observed = sha256_of_file(temp)
        size = os.path.getsize(temp)
        if observed != expected_sha256:
            raise Refusal("fetch " + url,
                          expected="sha256 " + expected_sha256,
                          observed="sha256 " + observed + " (" + str(size) + " bytes)",
                          why="the pin exists so that different bytes are never silently accepted")
        if expected_bytes is not None and size != expected_bytes:
            raise Refusal("fetch " + url, expected=str(expected_bytes) + " bytes",
                          observed=str(size) + " bytes")
        store.keep_file(expected_sha256, temp)
        return "fetched", size
    finally:
        if os.path.exists(temp):
            os.remove(temp)
