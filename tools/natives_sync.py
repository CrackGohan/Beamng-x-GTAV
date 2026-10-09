#!/usr/bin/env python3
"""Keep sheets/natives.json in step with sheets/systems.json.

  python3 tools/natives_sync.py [--db path/to/gta5-nativedb-data/natives.json]

Recomputes every native's used_by from the systems that list it. Natives a system lists that have no row
yet are added from the native DB when --db is given (name, namespace, original hash, signature), with an
empty xmap cell per supported build; without --db they are reported. Rows no system uses are reported.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    db = None
    if "--db" in sys.argv:
        with open(sys.argv[sys.argv.index("--db") + 1], encoding="utf-8") as f:
            raw = json.load(f)
        db = {fn["name"]: (ns, h, fn) for ns, fns in raw.items() for h, fn in fns.items()}
    with open(os.path.join(ROOT, "sheets", "systems.json"), encoding="utf-8") as f:
        systems = json.load(f)
    with open(os.path.join(ROOT, "sheets", "natives.json"), encoding="utf-8") as f:
        natives = json.load(f)
    with open(os.path.join(ROOT, "sheets", "games.json"), encoding="utf-8") as f:
        builds = next(g for g in json.load(f)["rows"] if g["id"] == "gta-v")["supported_builds"]
    used = {}
    for s in systems["rows"]:
        for n in s.get("natives", []):
            used.setdefault(n, set()).add(s["id"])
    rows = {r["id"]: r for r in natives["rows"]}
    for name, users in sorted(used.items()):
        if name in rows:
            rows[name]["used_by"] = sorted(users)
        elif db and name in db:
            ns, h, fn = db[name]
            rows[name] = {"id": name, "ns": ns, "hash": h, "ret": fn.get("return_type"),
                          "params": [f"{p['type']} {p['name']}" for p in fn.get("params", [])],
                          "since_build": int(fn.get("build") or 323), "used_by": sorted(users),
                          "xmap": {b: "" for b in builds}}
            print("added", name)
        else:
            print("MISSING (run with --db):", name)
    for name in rows:
        if name not in used:
            print("unused row:", name)
    natives["rows"] = [rows[k] for k in sorted(rows)]
    with open(os.path.join(ROOT, "sheets", "natives.json"), "w", encoding="utf-8") as f:
        json.dump(natives, f, indent=1)


if __name__ == "__main__":
    main()
