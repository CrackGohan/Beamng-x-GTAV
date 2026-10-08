#!/usr/bin/env python3
"""Preflight: lay every sheet's rows and columns over each other and list what will fail.

  python3 tools/preflight.py              # build stage (default)
  python3 tools/preflight.py --release    # release stage: everything filled and verified
  python3 tools/preflight.py --json       # machine-readable report

Categories
  UNFILLED      a required cell is empty (None, "", [], {} or "TODO")
  PC-ONLY       an empty cell in a column the sheet marks pc_columns: it can only be filled from the
                running game on the creator's PC. Allowed at build stage, blocking at release stage.
  BROKEN-REF    a reference to another sheet's row that does not exist
  MISMATCH      two sheets disagree (a system lists a native whose row does not list the system, ...)
  UNIMPLEMENTED a system's module file does not exist yet
  STALE-GEN     generated code is older than the sheets (run tools/gen.py)
  UNVERIFIED    a cell the row lists under "unverified" (fine to build, must be checked before release)

Exit code 1 when anything blocking for the chosen stage is found.
"""
import hashlib
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHEETS = os.path.join(ROOT, "sheets")
STAMP = os.path.join(ROOT, "gta", "src", "gen", "STAMP")


def load_sheets():
    sheets = {}
    for name in sorted(os.listdir(SHEETS)):
        if name.endswith(".json"):
            with open(os.path.join(SHEETS, name), encoding="utf-8") as f:
                data = json.load(f)
            sheets[data["sheet"]] = data
    return sheets


def sheets_digest():
    h = hashlib.sha256()
    for name in sorted(os.listdir(SHEETS)):
        if name.endswith(".json"):
            with open(os.path.join(SHEETS, name), "rb") as f:
                h.update(name.encode())
                h.update(f.read())
    return h.hexdigest()


def is_empty(v):
    if v is None:
        return True
    if isinstance(v, str) and v.strip() in ("", "TODO"):
        return True
    if isinstance(v, (list, dict)) and len(v) == 0:
        return True
    return False


def as_list(v):
    if v is None:
        return []
    return v if isinstance(v, list) else [v]


def check(stage):
    sheets = load_sheets()
    issues = {k: [] for k in ("UNFILLED", "PC-ONLY", "BROKEN-REF", "MISMATCH", "UNIMPLEMENTED", "STALE-GEN", "UNVERIFIED")}
    keys = {}
    for sname, sheet in sheets.items():
        keys[sname] = {r[sheet["key"]] for r in sheet["rows"]}

    supported = {}
    for g in sheets["games"]["rows"]:
        supported[g["id"]] = g["supported_builds"]
    gta_builds = supported.get("gta-v", [])

    for sname, sheet in sheets.items():
        optional = set(sheet.get("optional", []))
        pc_cols = set(sheet.get("pc_columns", []))
        refs = sheet.get("refs", {})
        seen = set()
        for row in sheet["rows"]:
            rid = row.get(sheet["key"])
            where = f"{sname}[{rid}]"
            if rid in seen:
                issues["MISMATCH"].append(f"{where}: duplicate key")
            seen.add(rid)
            for col in sheet["columns"]:
                if col not in row:
                    if col not in optional:
                        issues["UNFILLED"].append(f"{where}.{col}: missing")
                    continue
                v = row[col]
                if col in pc_cols and isinstance(v, dict):
                    for build in gta_builds:
                        if is_empty(v.get(build)):
                            issues["PC-ONLY"].append(f"{where}.{col}[{build}]: empty")
                    continue
                if is_empty(v) and col not in optional:
                    if isinstance(v, list) and col in refs:
                        continue  # an empty reference list is a legitimate "none"
                    (issues["PC-ONLY"] if col in pc_cols else issues["UNFILLED"]).append(f"{where}.{col}: empty")
            for col in as_list(row.get("unverified")):
                if col not in sheet["columns"]:
                    issues["MISMATCH"].append(f"{where}: unverified names unknown column '{col}'")
                else:
                    note = row.get("notes", "")
                    issues["UNVERIFIED"].append(f"{where}.{col}" + (f"  ({note})" if note else ""))
            for col, target in refs.items():
                for v in as_list(row.get(col)):
                    if v not in keys.get(target, set()):
                        issues["BROKEN-REF"].append(f"{where}.{col}: '{v}' is not a row of {target}")

    # Cross-sheet agreement -------------------------------------------------------------------
    systems = {r["id"]: r for r in sheets["systems"]["rows"]}
    natives = {r["id"]: r for r in sheets["natives"]["rows"]}
    bng = {r["id"]: r for r in sheets["bng_api"]["rows"]}
    messages = {r["id"]: r for r in sheets["messages"]["rows"]}

    for sid, s in systems.items():
        for n in s.get("natives", []):
            if n in natives and sid not in natives[n]["used_by"]:
                issues["MISMATCH"].append(f"systems[{sid}] calls {n} but natives[{n}].used_by does not list it")
        for b in s.get("bng_api", []):
            if b in bng and sid not in bng[b]["used_by"]:
                issues["MISMATCH"].append(f"systems[{sid}] uses bng_api {b} but bng_api[{b}].used_by does not list it")
        if s["game"] == "gta-v" and s.get("bng_api"):
            issues["MISMATCH"].append(f"systems[{sid}] is a GTA system but lists BeamNG calls")
        if s["game"] == "beamng-drive" and s.get("natives"):
            issues["MISMATCH"].append(f"systems[{sid}] is a BeamNG system but lists GTA natives")
        if not os.path.exists(os.path.join(ROOT, s["module"])):
            issues["UNIMPLEMENTED"].append(f"systems[{sid}]: {s['module']} does not exist")
    for nid, n in natives.items():
        for sid in n["used_by"]:
            if sid in systems and nid not in systems[sid].get("natives", []):
                issues["MISMATCH"].append(f"natives[{nid}].used_by lists {sid} but that system does not call it")
    for bid, b in bng.items():
        for sid in b["used_by"]:
            if sid in systems and bid not in systems[sid].get("bng_api", []):
                issues["MISMATCH"].append(f"bng_api[{bid}].used_by lists {sid} but that system does not use it")
    for mid, m in messages.items():
        for role in ("sender", "receiver"):
            sid = m[role]
            if sid in systems and mid not in systems[sid].get("messages", []):
                issues["MISMATCH"].append(f"messages[{mid}].{role} is {sid} but that system does not list the message")
        for f in m["fields"]:
            if len(f) < 3:
                issues["UNFILLED"].append(f"messages[{mid}] field {f}: needs [name, type, meaning]")
            elif f[1] == "rec[]" and (len(f) < 4 or not f[3]):
                issues["UNFILLED"].append(f"messages[{mid}].{f[0]}: rec[] needs its record fields")
            elif f[1] not in ("f32", "i32", "u32", "bool", "str", "f32[]", "rec[]"):
                issues["MISMATCH"].append(f"messages[{mid}].{f[0]}: unknown type {f[1]}")
    used_msgs = {m for s in systems.values() for m in s.get("messages", [])}
    for mid in messages:
        if mid not in used_msgs:
            issues["MISMATCH"].append(f"messages[{mid}] is not used by any system")
    input_fields = {f[0] for f in messages["input"]["fields"]}
    for c in sheets["controls"]["rows"]:
        if c["field"] != "none" and c["field"] not in input_fields:
            issues["BROKEN-REF"].append(f"controls[{c['id']}].field: '{c['field']}' is not a field of message input")
    for st in sheets["settings"]["rows"]:
        for sid in st["used_by"]:
            if sid in systems:
                side = "gta" if systems[sid]["game"] == "gta-v" else "beamng"
                if st["side"] not in (side, "both"):
                    issues["MISMATCH"].append(f"settings[{st['id']}] is side {st['side']} but {sid} is on {side}")
    if not any(r["id"] == "fallback" for r in sheets["player_proxies"]["rows"]):
        issues["UNFILLED"].append("player_proxies: needs a 'fallback' row")
    for o in sheets["obstacles"]["rows"]:
        if o["bng_kind"] not in ("slab", "post", "proxy_vehicle", "none"):
            issues["MISMATCH"].append(f"obstacles[{o['id']}].bng_kind: unknown kind {o['bng_kind']}")

    # Generated code freshness ------------------------------------------------------------------
    digest = sheets_digest()
    stamp = open(STAMP).read().strip() if os.path.exists(STAMP) else ""
    if stamp != digest:
        issues["STALE-GEN"].append("generated code does not match the sheets: run python3 tools/gen.py")

    blocking = ["UNFILLED", "BROKEN-REF", "MISMATCH", "UNIMPLEMENTED", "STALE-GEN"]
    if stage == "release":
        blocking += ["PC-ONLY", "UNVERIFIED"]
    return issues, blocking


def main():
    stage = "release" if "--release" in sys.argv else "build"
    issues, blocking = check(stage)
    if "--json" in sys.argv:
        print(json.dumps({"stage": stage, "blocking": blocking, "issues": issues}, indent=1))
    else:
        print(f"preflight ({stage} stage)")
        for cat, items in issues.items():
            mark = "BLOCKING" if cat in blocking else "ok to build"
            print(f"\n== {cat}: {len(items)} ({mark if items else 'none'})")
            for i in items:
                print("  - " + i)
    bad = sum(len(issues[c]) for c in blocking)
    print(f"\n{'CLEAN' if bad == 0 else 'NOT CLEAN'}: {bad} blocking item(s) for the {stage} stage")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
