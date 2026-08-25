"""HTTP a corpus can trust: one opener, one hash check, one refusal.

WHAT IS HERE AND WHAT IS NOT. The mechanics of getting bytes -- the opener, the redirect guard,
the streaming write, the digest comparison -- are the same for every corpus and live here once.
WHICH sources a corpus will accept is not mechanics, it is that corpus's declaration, so each
harness's own fetch.py states its allowed hosts and passes the check in. A corpus that could
fetch from anywhere has no pin, and a pin is what makes a fetched corpus a measurement.
"""

import os
import tempfile
import urllib.error
import urllib.request

from prep.refusal import Refusal
from prep.store import sha256_of_file

USER_AGENT = "outshine-corpus-prep/1"
TIMEOUT_S = 120

def opener(check_url):
    """An opener that runs the caller's own source check over every redirect it is offered."""

    class _RefuseOffListRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, request, fp, code, msg, headers, newurl):
            check_url(newurl)
            return urllib.request.HTTPRedirectHandler.redirect_request(
                self, request, fp, code, msg, headers, newurl
            )

    return urllib.request.build_opener(_RefuseOffListRedirect())

def request(url, headers=None):
    fields = {"User-Agent": USER_AGENT}
    fields.update(headers or {})
    return urllib.request.Request(url, headers=fields)

def open_url(built, url, headers=None):
    try:
        return built.open(request(url, headers), timeout=TIMEOUT_S)
    except (urllib.error.URLError, OSError) as error:
        raise Refusal("fetch " + url, why=str(error))

def stream_to_file(built, url, path):
    with open_url(built, url) as response, open(path, "wb") as out:
        for block in iter(lambda: response.read(1 << 20), b""):
            out.write(block)

def check_digest(subject, observed, expected, size, expected_bytes):
    if observed != expected:
        raise Refusal(
            "fetch " + subject,
            expected="sha256 " + expected,
            observed="sha256 " + observed + " (" + str(size) + " bytes)",
            why="the pin exists so that different bytes are never silently accepted",
        )
    if expected_bytes is not None and size != expected_bytes:
        raise Refusal("fetch " + subject, expected=str(expected_bytes) + " bytes",
                      observed=str(size) + " bytes")

def to_store(built, url, expected_sha256, store, expected_bytes=None):
    if store.has(expected_sha256):
        return "cached", os.path.getsize(store.path(expected_sha256))
    handle, temp = tempfile.mkstemp(prefix="outshine-fetch-")
    os.close(handle)
    try:
        stream_to_file(built, url, temp)
        observed = sha256_of_file(temp)
        size = os.path.getsize(temp)
        check_digest(url, observed, expected_sha256, size, expected_bytes)
        store.keep_file(expected_sha256, temp)
        return "fetched", size
    finally:
        if os.path.exists(temp):
            os.remove(temp)
