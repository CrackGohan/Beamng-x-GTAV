#!/usr/bin/env python3
"""Download what the release ships from other projects, pinned by SHA-256, into build/deps/.

  python3 tools/fetch_deps.py

ReShade 6.8.0 with add-on support (BSD-3-Clause) from reshade.me: the setup exe is a zip with ReShade64.dll
inside; 7z extracts it. Nothing is executed.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPS = os.path.join(ROOT, "build", "deps")
RESHADE_URL = "https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe"
RESHADE_SETUP_SHA = "afe4c8f13048306307983b8b3d41d5bf00a86820440b0e57dea10950e1176445"


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    os.makedirs(DEPS, exist_ok=True)
    dll = os.path.join(DEPS, "ReShade64.dll")
    if os.path.exists(dll):
        print("have", dll, sha(dll))
        return
    work = tempfile.mkdtemp(prefix="reshade-")
    setup = os.path.join(work, "setup.exe")
    req = urllib.request.Request(RESHADE_URL, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req) as r, open(setup, "wb") as f:
        shutil.copyfileobj(r, f)
    got = sha(setup)
    if got != RESHADE_SETUP_SHA:
        sys.exit(f"ReShade setup checksum mismatch: {got}")
    out = os.path.join(work, "x")
    subprocess.run(["7z", "x", "-y", "-o" + out, setup, "ReShade64.dll"], check=True, stdout=subprocess.DEVNULL)
    shutil.copy(os.path.join(out, "ReShade64.dll"), dll)
    shutil.rmtree(work)
    print("fetched", dll, sha(dll))


if __name__ == "__main__":
    main()
