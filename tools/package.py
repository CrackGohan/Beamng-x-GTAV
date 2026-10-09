#!/usr/bin/env python3
"""Build the release from sheets/files.json: one zip per Melty component, the install recipe and a
manifest of every entry (for Melty's inspect_package / validate_recipe / one_click_check).

  python3 tools/package.py 0.1.0

Needs: build/gta/BeamLS.asi (gta/build.sh) and build/deps/ReShade64.dll (tools/fetch_deps.py).
Output: dist/BeamLS-<v>.zip, dist/BeamLS-BeamNG-<v>.zip, dist/recipe.json, dist/entries.json, melty.json.
Zips are deterministic (fixed timestamps, sorted entries) so the same sources give the same bytes.
"""
import hashlib
import json
import os
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")
STAMP = (2026, 1, 1, 0, 0, 0)


def load(name):
    with open(os.path.join(ROOT, "sheets", name + ".json"), encoding="utf-8") as f:
        return json.load(f)


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def credits_files():
    """Licence texts of everything shipped, plus the generated CREDITS.md."""
    um = "/tmp/universal-modder-LICENSE"
    out = {
        "credits/CREDITS.md": os.path.join(ROOT, "CREDITS.md"),
        "credits/LICENSE-ReShade.md": os.path.join(ROOT, "gta", "third_party", "reshade", "LICENSE.md"),
        "credits/LICENSE-MinHook.txt": os.path.join(ROOT, "gta", "third_party", "minhook", "LICENSE.txt"),
        "credits/LICENSE-universal-modder.txt": os.path.join(ROOT, "gta", "third_party", "LICENSE-universal-modder.txt"),
        "credits/README.txt": os.path.join(ROOT, "gta", "README-package.txt"),
    }
    return out


def add_tree(entries, src, prefix):
    for dirpath, _, files in os.walk(src):
        for fn in files:
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, src).replace(os.sep, "/")
            entries[prefix + rel] = full


def write_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in sorted(entries):
            info = zipfile.ZipInfo(name, STAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            with open(entries[name], "rb") as f:
                z.writestr(info, f.read())


def recipe(version, names):
    games = {g["id"]: g for g in load("games")["rows"]}
    settings = {s["id"]: s["value"] for s in load("settings")["rows"]}
    files = load("files")["rows"]
    mappings, seen = [], set()
    for r in files:
        top = r["path"].split("/")[0] + "/"
        key = (r["component"], top)
        if key in seen:
            continue
        seen.add(key)
        mappings.append({"component": r["component"], "from": top, "to": r["installs_to"]})
    return {
        "schemaVersion": 1,
        "mode": "installed",
        "games": [
            {"slug": "gta-v", "role": "primary", "version": settings["melty_gta_version"]},
            {"slug": "beamng-drive", "role": "companion"},
        ],
        "components": [
            {"id": "main", "fileName": names["main"], "label": "BeamNG x Los Santos (GTA V side)", "kind": "main", "required": True},
            {"id": "beamng", "fileName": names["beamng"], "label": "BeamNG x Los Santos (BeamNG side)", "kind": "extra", "required": True},
        ],
        "requirements": [{"kind": "loader", "id": games["gta-v"]["loader"]}],
        "mappings": mappings,
        "launch": {"kind": "game"},
        "together": [{"launch": {"kind": "exe", "path": settings["melty_bng_launch"]}, "startFirst": True,
                      "waitSeconds": settings["melty_bng_wait_s"]}],
        "settings": [{"file": "{documents}/Rockstar Games/GTA V/settings.xml", "set": settings["melty_gta_settings"]}],
        "runtimeData": ["{localappdata}/BeamLS"],
    }


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: tools/package.py <version>")
    version = sys.argv[1]
    if subprocess.run([sys.executable, os.path.join(ROOT, "tools", "preflight.py")], stdout=subprocess.DEVNULL).returncode != 0:
        sys.exit("preflight is not clean: run python3 tools/preflight.py")
    os.makedirs(DIST, exist_ok=True)
    names = {"main": f"BeamLS-{version}.zip", "beamng": f"BeamLS-BeamNG-{version}.zip"}
    comps = {"main": {}, "beamng": {}}
    for r in load("files")["rows"]:
        src = os.path.join(ROOT, r["from"])
        if r["id"] == "credits":
            for name, path in credits_files().items():
                comps[r["component"]][name] = path
        elif os.path.isdir(src):
            add_tree(comps[r["component"]], src, r["path"])
        else:
            if not os.path.exists(src):
                sys.exit(f"missing {r['from']} (files row {r['id']}): build it first")
            comps[r["component"]][r["path"]] = src
    manifest = {}
    for comp, entries in comps.items():
        for name, path in entries.items():
            if not os.path.exists(path):
                sys.exit(f"missing {path}")
        out = os.path.join(DIST, names[comp])
        write_zip(out, entries)
        manifest[comp] = {"fileName": names[comp], "size": os.path.getsize(out), "sha256": sha(out),
                          "entries": [{"path": n, "size": os.path.getsize(p)} for n, p in sorted(entries.items())]}
        print(f"{names[comp]}: {len(entries)} files, {os.path.getsize(out)} bytes, sha256 {manifest[comp]['sha256']}")
    rec = recipe(version, names)
    with open(os.path.join(DIST, "recipe.json"), "w") as f:
        json.dump(rec, f, indent=1)
    with open(os.path.join(DIST, "entries.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    # melty.json at the repo root: the same recipe with * where the version goes
    star = json.loads(json.dumps(rec).replace(version, "*"))
    with open(os.path.join(DIST, "melty.json"), "w") as f:
        json.dump(star, f, indent=1)
    print("recipe: dist/recipe.json, manifest: dist/entries.json, dist/melty.json (copy to the repo root once agreed)")


if __name__ == "__main__":
    main()
