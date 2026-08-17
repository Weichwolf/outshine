"""Bytes from upstream, under a pin the manifest states."""

import os
import re
import struct
import tempfile
import urllib.error
import urllib.request
import zlib

from prep.refusal import Refusal
from prep.store import sha256_hex, sha256_of_file

KHRONOS_HOST = "raw.githubusercontent.com"
KHRONOS_REPO = "KhronosGroup/glTF-Sample-Assets"

# The whole of what may be reached. A source not on this list is not a source: everything under it
# is pinned, licence-checked and reachable without an account, and nothing else has been shown to be.
PLAIN_PREFIXES = (
    "https://download.blender.org/demo/cycles/",
    "https://download.blender.org/demo/eevee/",
    "https://download.blender.org/demo/bbb/",
    "https://download.blender.org/demo/asset-bundles/",
    "https://studio.blender.org/",
)

KHRONOS_URL = re.compile(
    r"^https://" + re.escape(KHRONOS_HOST) + "/" + re.escape(KHRONOS_REPO) + r"/([0-9a-f]{40})/(.+)$"
)

USER_AGENT = "outshine-corpus-prep/1"
TIMEOUT_S = 120


def khronos_url(commit, path):
    return "https://%s/%s/%s/%s" % (KHRONOS_HOST, KHRONOS_REPO, commit, path)


def check_url(url, commit=None):
    """The pin lives inside the URL for a repo source, so the URL and the declared pin cross-check."""
    match = KHRONOS_URL.match(url)
    if match:
        if commit is not None and match.group(1) != commit:
            raise Refusal(
                "url " + url,
                expected="the manifest's pinned commit " + commit,
                observed="a url naming commit " + match.group(1),
                why="a url that carries its own pin must carry the declared one",
            )
        return "khronos"
    for prefix in PLAIN_PREFIXES:
        if url.startswith(prefix) and ".." not in url:
            return "plain"
    raise Refusal(
        "url " + url,
        expected="the pinned glTF-Sample-Assets tree, or " + ", ".join(PLAIN_PREFIXES),
        observed=url,
        why="an unlisted source has no pin, no stated licence and no reason to be trusted",
    )


class _RefuseOffListRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        check_url(newurl)
        return urllib.request.HTTPRedirectHandler.redirect_request(
            self, request, fp, code, msg, headers, newurl
        )


_OPENER = urllib.request.build_opener(_RefuseOffListRedirect())


def _request(url, headers=None):
    fields = {"User-Agent": USER_AGENT}
    fields.update(headers or {})
    return urllib.request.Request(url, headers=fields)


def _open(url, headers=None):
    try:
        return _OPENER.open(_request(url, headers), timeout=TIMEOUT_S)
    except (urllib.error.URLError, OSError) as error:
        raise Refusal("fetch " + url, why=str(error))


def stream_to_file(url, path):
    with _open(url) as response, open(path, "wb") as out:
        for block in iter(lambda: response.read(1 << 20), b""):
            out.write(block)


def content_length(url):
    with _open(url, {"Range": "bytes=0-0"}) as response:
        if response.status != 206:
            raise Refusal(
                "range request to " + url,
                expected="HTTP 206 Partial Content",
                observed="HTTP %d" % response.status,
                why="without ranges an 830 MB archive must be fetched whole to reach one member",
            )
        content_range = response.headers.get("Content-Range", "")
        response.read()
    match = re.match(r"^bytes \d+-\d+/(\d+)$", content_range)
    if not match:
        raise Refusal("range request to " + url, expected="a Content-Range header", observed=content_range)
    return int(match.group(1))


def read_range(url, start, length):
    if length <= 0:
        return b""
    with _open(url, {"Range": "bytes=%d-%d" % (start, start + length - 1)}) as response:
        if response.status != 206:
            raise Refusal(
                "range request to " + url,
                expected="HTTP 206 Partial Content",
                observed="HTTP %d" % response.status,
            )
        data = response.read()
    if len(data) != length:
        raise Refusal("range request to " + url, expected="%d bytes" % length, observed="%d bytes" % len(data))
    return data


_EOCD = b"PK\x05\x06"
_EOCD64_LOCATOR = b"PK\x06\x07"
_EOCD64 = b"PK\x06\x06"
_CENTRAL = b"PK\x01\x02"
_LOCAL = b"PK\x03\x04"


def zip_directory(url, size=None):
    """The central directory over HTTP ranges: an archive is read, never downloaded, to list it."""
    check_url(url)
    total = size if size is not None else content_length(url)
    tail_length = min(total, 1 << 16)
    tail = read_range(url, total - tail_length, tail_length)
    at = tail.rfind(_EOCD)
    if at < 0:
        raise Refusal("zip " + url, expected="an end-of-central-directory record in the last 64 KiB", observed="none")
    entries, directory_bytes, directory_at = struct.unpack_from("<HII", tail, at + 10)
    locator = tail.rfind(_EOCD64_LOCATOR, 0, at)
    if locator >= 0:
        eocd64_at = struct.unpack_from("<Q", tail, locator + 8)[0]
        head = read_range(url, eocd64_at, 56)
        if head[:4] == _EOCD64:
            entries = struct.unpack_from("<Q", head, 32)[0]
            directory_bytes = struct.unpack_from("<Q", head, 40)[0]
            directory_at = struct.unpack_from("<Q", head, 48)[0]

    directory = read_range(url, directory_at, directory_bytes)
    members = {}
    offset = 0
    for _ in range(entries):
        if directory[offset : offset + 4] != _CENTRAL:
            raise Refusal("zip " + url, expected="a central directory header", observed=repr(directory[offset : offset + 4]))
        method, = struct.unpack_from("<H", directory, offset + 10)
        compressed, uncompressed = struct.unpack_from("<II", directory, offset + 20)
        name_len, extra_len, comment_len = struct.unpack_from("<HHH", directory, offset + 28)
        local_at, = struct.unpack_from("<I", directory, offset + 42)
        name = directory[offset + 46 : offset + 46 + name_len].decode("utf-8", "replace")
        extra = directory[offset + 46 + name_len : offset + 46 + name_len + extra_len]
        uncompressed, compressed, local_at = _zip64(extra, uncompressed, compressed, local_at)
        members[name] = {
            "name": name,
            "method": method,
            "compressedBytes": compressed,
            "uncompressedBytes": uncompressed,
            "localHeaderAt": local_at,
        }
        offset += 46 + name_len + extra_len + comment_len
    return {"url": url, "archiveBytes": total, "members": members}


def _zip64(extra, uncompressed, compressed, local_at):
    at = 0
    while at + 4 <= len(extra):
        tag, size = struct.unpack_from("<HH", extra, at)
        body = extra[at + 4 : at + 4 + size]
        if tag == 0x0001:
            cursor = 0
            if uncompressed == 0xFFFFFFFF and cursor + 8 <= len(body):
                uncompressed = struct.unpack_from("<Q", body, cursor)[0]
                cursor += 8
            if compressed == 0xFFFFFFFF and cursor + 8 <= len(body):
                compressed = struct.unpack_from("<Q", body, cursor)[0]
                cursor += 8
            if local_at == 0xFFFFFFFF and cursor + 8 <= len(body):
                local_at = struct.unpack_from("<Q", body, cursor)[0]
        at += 4 + size
    return uncompressed, compressed, local_at


def zip_member_bytes(url, member, directory=None):
    listing = directory or zip_directory(url)
    entry = listing["members"].get(member)
    if entry is None:
        raise Refusal(
            "zip member " + member,
            expected="a member of " + url,
            observed="%d members, none of that name" % len(listing["members"]),
        )
    header = read_range(url, entry["localHeaderAt"], 30)
    if header[:4] != _LOCAL:
        raise Refusal("zip member " + member, expected="a local file header", observed=repr(header[:4]))
    name_len, extra_len = struct.unpack_from("<HH", header, 26)
    data_at = entry["localHeaderAt"] + 30 + name_len + extra_len
    raw = read_range(url, data_at, entry["compressedBytes"])
    if entry["method"] == 0:
        return raw
    if entry["method"] == 8:
        return zlib.decompress(raw, -zlib.MAX_WBITS)
    raise Refusal("zip member " + member, expected="stored or deflated", observed="compression method %d" % entry["method"])


def download_to_store(url, expected_sha256, store, expected_bytes=None, commit=None, member=None):
    """Fetch, hash, refuse on mismatch, and file the bytes under their own digest."""
    check_url(url, commit)
    if store.has(expected_sha256):
        return "cached", os.path.getsize(store.path(expected_sha256))

    if member is not None:
        # The pin is the member's digest, which is what is consumed; hashing 830 MB to reach 2 MB
        # would cost the whole download the range request exists to avoid.
        data = zip_member_bytes(url, member)
        observed = sha256_hex(data)
        size = len(data)
        _check(url + "!" + member, observed, expected_sha256, size, expected_bytes)
        store.keep(expected_sha256, data)
        return "fetched", size

    handle, temp = tempfile.mkstemp(prefix="outshine-fetch-")
    os.close(handle)
    try:
        stream_to_file(url, temp)
        observed = sha256_of_file(temp)
        size = os.path.getsize(temp)
        _check(url, observed, expected_sha256, size, expected_bytes)
        store.keep_file(expected_sha256, temp)
        return "fetched", size
    finally:
        if os.path.exists(temp):
            os.remove(temp)


def _check(subject, observed, expected, size, expected_bytes):
    if observed != expected:
        raise Refusal(
            "fetch " + subject,
            expected="sha256 " + expected,
            observed="sha256 " + observed + " (" + str(size) + " bytes)",
            why="the pin exists so that different bytes are never silently accepted",
        )
    if expected_bytes is not None and size != expected_bytes:
        raise Refusal("fetch " + subject, expected=str(expected_bytes) + " bytes", observed=str(size) + " bytes")
