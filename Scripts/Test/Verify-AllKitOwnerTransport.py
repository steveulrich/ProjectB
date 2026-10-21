"""Verify captured AllKit owner transport; controls corrupt real evidence only."""
from __future__ import annotations

import argparse
import copy
import csv
from datetime import datetime
import hashlib
import json
import math
import re
import stat
from pathlib import Path

from AllKitEvidence import verify_gameplay

ROLES = ("host", "client1", "client2", "client3")
ROOT = Path(__file__).resolve().parents[2]
NATIVE_FAILURE = re.compile(r"^(?:\[[^\]\r\n]*\]\s*)*\s*(?:[A-Za-z_]\w+:\s*(?:Error|Fatal):|Log\w*(?:Error|Fatal):|(?:Fatal error|Assertion failed|Ensure condition failed):)[^\r\n]*", re.MULTILINE)
ACTIONS = [
    "MeleePrimary", "ArgusSlide", "NoRetreat", "ForGlory", "Retribution",
    "AlonaPrimary", "AlonaSunsGrace", "AlonaRadiance", "AlonaSunBurst", "AlonaBlessing",
    "KorrynPrimary", "KorrynFlock", "KorrynBurdenOfSin", "KorrynCircleOfSpite", "KorrynAuraOfSilence",
    "RawlinsPrimary", "RawlinsDoubleDown", "RawlinsPowerShot", "RawlinsSlideShot", "RawlinsBlazingBarrage",
]
CASES = [(i, c) for i in range(20) for c in (0, 1)] + [(20, 0), (21, 0)]
ROW_FLOATS = ("health", "maximum", "attack", "armor", "move", "incoming", "leech")
ROW_TOLERANCE = {"health": .15, "maximum": .01, "attack": .01, "armor": .01, "move": .01, "incoming": .01, "leech": .001}
ROW_INTS = ("tags", "kills", "deaths", "assists")
MARKER = re.compile(r"^(?:\[[^\]\r\n]*\]\s*)*LogTemp: (?:Display|Error): BwayAllKitOwner: (\w+)(?: ([^\r\n]*))?\r?$", re.MULTILINE)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def child(parent, relative):
    parent = Path(parent).resolve()
    if Path(relative).is_absolute():
        raise ValueError("Relative manifest path required")
    path = (parent / relative).resolve()
    if not path.is_relative_to(parent) or path == parent:
        raise ValueError("Manifest path escapes root")
    return path


def assert_bytes(path, entry):
    if Path(path).stat().st_size != int(entry["bytes"]) or digest(path) != entry["sha256"].upper():
        raise ValueError("Manifest bytes changed: " + str(path))


def archive_files(manifest):
    files = {}
    for entry in manifest["files"]:
        key = entry["path"].replace("\\", "/").casefold()
        if key in files:
            raise ValueError("Duplicate source manifest path")
        assert_bytes(child(manifest["archive"], entry["path"]), entry)
        files[key] = entry
    if not files:
        raise ValueError("Empty archived source manifest")
    return files


def identity(head, entries):
    value = head + "\n" + "\n".join(e["path"] + ":" + e["sha256"] for e in entries)
    return hashlib.sha256(value.encode()).hexdigest().upper()


def native_inputs(manifest):
    return sorted((e for e in manifest["files"] if re.search(r"\.(cpp|h|cs|uproject|uplugin)$", e["path"], re.I)), key=lambda e: e["path"].casefold())


def timestamp(value):
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def payload_paths(candidate, manifest_path, entries, expected_count):
    candidate = Path(candidate)
    if getattr(candidate.lstat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
        raise ValueError("Candidate root is a reparse point")
    candidate = candidate.resolve()
    seen = set()
    for entry in entries:
        path = child(candidate, entry["path"])
        raw = candidate / entry["path"]
        for node in (raw, *raw.parents):
            if getattr(node.lstat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                raise ValueError("Reparse point in packaged payload")
            if node == candidate:
                break
        if path in seen:
            raise ValueError("Duplicate package payload path")
        assert_bytes(path, entry)
        seen.add(path)
    runtime_saved = (candidate / "Windows/ProjectB/Saved").resolve()
    extras = {p.resolve() for p in candidate.rglob("*") if p.is_file()} - seen - {Path(manifest_path).resolve()}
    invalid_extras = {p for p in extras if not p.is_relative_to(runtime_saved) or p.suffix.lower() in (".exe", ".dll", ".pak", ".ucas", ".utoc", ".uasset", ".umap")}
    if not seen or len(entries) != expected_count or invalid_extras:
        raise ValueError("Immutable package payload membership/count changed")
    return seen


def bind(record, reviewed_native_sha256):
    if not re.fullmatch(r"[A-Fa-f0-9]{64}", reviewed_native_sha256):
        raise ValueError("Explicit reviewed native SHA256 required")
    package = read_json(record["packageResult"])
    manifest_path = Path(package["sourceManifest"])
    manifest = read_json(manifest_path)
    files = archive_files(manifest)
    snapshot_path = Path(package["sourceSnapshot"])
    snapshot = read_json(snapshot_path)
    if not (digest(record["packageResult"]) == record["packageResultSHA256"].upper()
            and manifest_path.resolve() == Path(record["sourceManifest"]).resolve()
            and digest(manifest_path) == record["sourceManifestSHA256"].upper() == package["sourceManifestSha256"].upper()
            and package["status"] == "passed" and package["exitCode"] == 0 and package["moduleRestored"] is True
            and package["candidate"] == record["candidate"]
            and record["effectiveSourceIdentity"] == manifest["effectiveSourceIdentity"] == package["effectiveSourceIdentity"]
            and identity(manifest["sourceHead"], manifest["files"]) == manifest["effectiveSourceIdentity"].upper()
            and record["sourceHead"] == manifest["sourceHead"] == snapshot["sourceHead"] == package["sourceHead"]
            and snapshot_path.resolve() == Path(record["sourceSnapshot"]).resolve()
            and digest(snapshot_path) == record["sourceSnapshotSHA256"].upper() == package["sourceSnapshotSha256"].upper()):
        raise ValueError("Package/source/snapshot binding changed")
    for kind in ("patch", "status", "untracked"):
        assert_bytes(snapshot["files"][kind]["path"], snapshot["files"][kind])
    snapshot_identity = hashlib.sha256((snapshot["sourceHead"] + "|" + "|".join(snapshot["files"][k]["sha256"] for k in ("patch", "status", "untracked"))).encode()).hexdigest().upper()
    if snapshot_identity != package["sourceIdentity"].upper() or snapshot_identity != snapshot["sourceIdentity"].upper() or snapshot_identity != record["sourceIdentity"].upper():
        raise ValueError("Snapshot content identity changed")
    native = [f for f in files.values() if f["path"].replace("\\", "/").endswith("/Private/Development/BwayAllKitCombatProbe.cpp")]
    if len(native) != 1 or native[0]["sha256"].upper() != reviewed_native_sha256.upper():
        raise ValueError("Archived AllKit native hash is not the explicit reviewed revision")
    final_inputs = native_inputs(manifest)
    native_identity = identity(manifest["sourceHead"], final_inputs)
    if native_identity != record["nativeSourceIdentity"].upper():
        raise ValueError("Final native input identity changed")
    targets = set()
    for reference in package["compileRecords"]:
        compiled = read_json(reference["path"])
        original = read_json(compiled["originalCompileRecord"])
        compile_manifest = read_json(compiled["nativeSourceManifest"])
        archive_files(compile_manifest)
        inputs = native_inputs(compile_manifest)
        if not (digest(reference["path"]) == reference["sha256"].upper() and compiled["status"] == "passed"
                and compiled["exitCode"] == 0 and not compiled["failureSignals"] and compiled["sourceHead"] == manifest["sourceHead"]
                and compiled["target"] == reference["target"] and compiled["target"] not in targets
                and digest(compiled["originalCompileRecord"]) == compiled["originalCompileRecordSha256"].upper()
                and all(compiled[k] == original[k] for k in ("status", "exitCode", "sourceHead", "target", "startedUtc", "generatedUtc", "log"))
                and digest(compiled["nativeSourceManifest"]) == compiled["nativeSourceManifestSha256"].upper()
                and compile_manifest["sourceHead"] == manifest["sourceHead"]
                and timestamp(compile_manifest["createdUtc"]) <= timestamp(compiled["startedUtc"])
                and identity(compile_manifest["sourceHead"], inputs) == compiled["nativeSourceIdentity"].upper() == native_identity
                and len(inputs) == len(final_inputs) == compiled["nativeInputCount"]
                and re.fullmatch(r"[A-Fa-f0-9]{64}", compiled["binarySha256"])
                and timestamp(compiled["binaryLastWriteUtc"]) >= timestamp(compiled["startedUtc"])
                and (compiled["target"] != "LyraGame" or compiled["binarySha256"].upper() == record["executableSHA256"].upper())):
            raise ValueError("Bound compile/native/binary evidence changed")
        targets.add(compiled["target"])
    if targets != {"LyraEditor", "LyraGame"}:
        raise ValueError("Both bound Editor and Game compile records required")
    candidate = ROOT / "Saved/StagedBuilds" / record["candidate"]
    artifact_manifest = Path(package["hashManifest"])
    if not (Path(package["stage"]).resolve() == candidate and artifact_manifest.resolve() == candidate / "sha256.csv"
            and artifact_manifest.resolve() == Path(record["artifactManifest"]).resolve()
            and digest(artifact_manifest) == record["artifactManifestSHA256"].upper()):
        raise ValueError("Candidate artifact manifest binding changed")
    with artifact_manifest.open(encoding="utf-8-sig", newline="") as stream:
        payload = payload_paths(candidate, artifact_manifest, list(csv.DictReader(stream)), package["artifactCount"])
    exe = candidate / "Windows/ProjectB/Binaries/Win64/LyraGame.exe"
    if not (Path(record["executable"]).resolve() == exe and exe in payload
            and digest(exe) == record["executableSHA256"].upper() == record["executableSHA256After"].upper() == package["executableSha256"].upper()
            and Path(record["runner"]).resolve() == ROOT / "Scripts/Test/Run-AllKitOwnerTransport.ps1"
            and digest(record["runner"]) == record["runnerSHA256"].upper()):
        raise ValueError("Runner or executable binding changed")
    settings = candidate / "Windows/ProjectB/Saved/Config/Windows/GameUserSettings.ini"
    if Path(record["settings"]).resolve() != settings or record["settingsRestored"] is not True:
        raise ValueError("Exact staged settings restoration required")
    if record["settingsExistedBefore"]:
        if digest(settings) != record["settingsBeforeSHA256"].upper() or digest(record["settingsBackup"]) != record["settingsBeforeSHA256"].upper():
            raise ValueError("Original settings bytes not restored")
    elif settings.exists():
        raise ValueError("Original settings absence not restored")
    return {name: True for name in ("package_bytes", "manifest_bytes", "successful_candidate", "source_identity", "reviewed_native", "snapshot_bytes_and_content", "bound_editor_and_game_compiles", "full_payload_membership", "runner_and_binary", "exact_settings_restore")}


def events(text):
    return [(m[1], dict(re.findall(r"(\w+)=([^\s]+)", m[2] or ""))) for m in MARKER.finditer(text)]


def numeric(row, key):
    value = float(row[key])
    if not math.isfinite(value):
        raise ValueError(f"Non-finite {key}")
    return value


def close(a, b, tolerance=.15):
    return math.isfinite(a) and math.isfinite(b) and abs(a-b) <= tolerance


def immune_primary_expected_rows(baseline, target, armor_shred):
    """Only Korryn's independent armor/tag effect survives damage immunity."""
    expected = copy.deepcopy(baseline)
    expected[target]["armor"] = numeric(baseline[target], "armor") + armor_shred
    expected[target]["tags"] = int(baseline[target]["tags"]) | 32
    return expected


def credit_lifecycle(events, plan, credit):
    """Join the native death callback and replacement to the captured victim."""
    names = ("CreditBinding", "CreditDeath", "CreditObserverRemoved", "CreditRespawn", "CreditPASS", "CleanupAuthority", "PASS")
    found = {}
    positions = {}
    for name in names:
        matches = [(i, row) for i, (kind, row) in enumerate(events) if kind == name]
        if len(matches) != 1:
            raise ValueError(f"Expected exactly one {name}, found {len(matches)}")
        positions[name], found[name] = matches[0]
    activations = [i for i, (kind, row) in enumerate(events) if kind == "Activation" and row.get("case") == "21" and row.get("control") == "0"]
    if len(activations) != 1:
        raise ValueError("Expected exactly one credit-kill Activation")
    binding, death, removed, respawn = (found[name] for name in names[:4])
    joined = all(row["case"] == "21" and row["serial"] == plan["serial"] and row["victim"] == plan["target"] == credit["victim"] for row in (binding, death, removed, respawn))
    key, pawn = binding["pawnKey"], binding["pawn"]
    return {
        "identity": joined and bool(key) and pawn not in ("", "None")
            and death["pawnKey"] == removed["pawnKey"] == respawn["oldPawnKey"] == key
            and death["pawn"] == respawn["oldPawn"] == pawn,
        "bound_before_activation": positions["CreditBinding"] < activations[0]
            and binding["callbackBound"] == binding["existingDeathBinding"] == "1",
        "single_consumed_death": death["boundIdentity"] == death["deathStarted"] == death["consumed"] == death["assisters"] == death["callbacks"] == "1"
            and death["contributors"] == "0" and numeric(death, "health") == 0,
        "observer_removed": removed["componentAlive"] in ("0", "1") and removed["callbackBound"] == respawn["callbackBound"] == "0",
        "distinct_living_respawn": respawn["identityChanged"] == "1" and respawn["newPawnKey"] not in ("", key)
            and respawn["newPawn"] not in ("", "None") and numeric(respawn, "health") > 0,
        "ordered_death_cleanup_respawn": positions["CreditBinding"] < positions["CreditDeath"] < positions["CreditObserverRemoved"]
            < positions["CreditPASS"] < positions["CreditRespawn"] < positions["CleanupAuthority"] < positions["PASS"],
    }


def verify(record, logs, expected_lag):
    checks = {}

    def check(name, value):
        if name in checks:
            raise ValueError(f"Duplicate check: {name}")
        checks[name] = bool(value)

    def select(ev, name, **match):
        return [r for kind, r in ev if kind == name and all(r.get(k) == str(v) for k, v in match.items())]

    def one(ev, name, **match):
        found = select(ev, name, **match)
        if len(found) != 1:
            raise ValueError(f"Expected exactly one {name} {match}, found {len(found)}")
        return found[0]

    def rows(ev, phase, case, control, observer):
        found = select(ev, "Row", phase=phase, case=case, control=control, observer=observer)
        mapped = {int(r["player"]): r for r in found}
        if len(found) != 4 or len(mapped) != 4:
            raise ValueError(f"Missing/duplicate {phase} rows {case}/{control}/{observer}")
        for r in found:
            for key in ROW_FLOATS + ROW_INTS + ("effects",):
                numeric(r, key)
        return mapped

    def equal_rows(left, right):
        return left.keys() == right.keys() and all(
            all(close(numeric(left[p], k), numeric(right[p], k), ROW_TOLERANCE[k]) for k in ROW_FLOATS)
            and all(int(left[p][k]) == int(right[p][k]) for k in ROW_INTS)
            for p in left)

    try:
        check("runner_schema", record.get("schema") == "allkit-owner-1")
        check("runner_success", record.get("status") == "passed-transport-scope" and record.get("runError") is None)
        check("requested_lag", record.get("expectedLagMs") == expected_lag)
        for key in ("hostReady", "allConnectedAlive", "clientExitZero", "allOwnedExited", "settingsRestored"):
            check(key, record.get(key) is True)
        check("unchanged_binary", bool(record.get("executableSHA256")) and record.get("executableSHA256") == record.get("executableSHA256After"))
        processes = {p["role"]: p for p in record["processes"]}
        check("four_unique_processes", set(processes) == set(ROLES) and len(record["processes"]) == 4 and len({p["processId"] for p in processes.values()}) == 4)
        check("selected_rendered_remote_role", record.get("renderedRemoteRole") in ROLES[1:])
        check("owned_listen_endpoint", record.get("udpPortAvailableBefore") is True and bool(record.get("listenEndpoints"))
              and all(e["LocalPort"] == 7777 and e["OwningProcess"] == processes["host"]["processId"] for e in record["listenEndpoints"]))
        check("exact_attempt_token", record["token"] == record["candidate"] + "-" + record["attempt"])
        ev = {role: events(logs[role]) for role in ROLES}
        ids = {}
        for role in ROLES:
            text = logs[role]
            check(f"{role}.no_failure", not re.search(r"BwayAllKitOwner: (FAIL|FailureCleanup)|Fatal error:|Assertion failed:|Ensure condition failed:", text))
            native_errors = NATIVE_FAILURE.findall(text)
            check(f"{role}.no_unclassified_native_error", not native_errors)
            r = one(ev[role], "Ready")
            ids[role] = int(r["player"])
            check(f"{role}.fresh_token", r["token"] == record["token"])
            check(f"{role}.net_mode", int(r["netMode"]) == (2 if role == "host" else 3))
            check(f"{role}.effective_lag", int(r["lag"]) == expected_lag and all(int(r[k]) == 0 for k in ("lagMin", "lagMax", "incomingMin", "incomingMax", "loss", "incomingLoss", "jitter", "variance")))
            rendered = role in ("host", record["renderedRemoteRole"])
            check(f"{role}.render_topology", ("D3D12" in r["rhi"].upper()) if rendered else "NULL" in r["rhi"].upper())
            check(f"{role}.frame_cap", close(numeric(r, "maxFPS"), 30, .1))
            if rendered:
                check(f"{role}.resolution", int(r["width"]) == 640 and int(r["height"]) == 360)
            final = one(ev[role], "PeerFinalPASS")
            check(f"{role}.local_cleanup", int(final["observer"]) == ids[role] and final["cleanup"] == "1")
            p = processes[role]
            command = "bway.Test.AllKitOwnerHost" if role == "host" else "bway.Test.AllKitOwnerClient"
            args = p["args"]
            expected_user_dir = Path(p["log"]).parent / f"allkit-{record['token']}-users" / role
            check(f"{role}.isolated_settings", p["userDirectoryWasAbsent"] is True and Path(p["userDirectory"]).is_dir() and Path(p["userDirectory"]).resolve() == expected_user_dir.resolve()
                  and p["engineSavedSuffix"] == record["token"] + "-" + role
                  and args.count(f'-UserDir="{p["userDirectory"]}"') == 1
                  and args.count("-enginesaveddirsuffix=" + p["engineSavedSuffix"]) == 1
                  and sum(a.startswith("-UserDir=") for a in args) == 1 and sum(a.startswith("-enginesaveddirsuffix=") for a in args) == 1)
            check(f"{role}.exact_launch_arguments", args.count("-ExecCmds=" + command) == 1 and args.count('-ExecCmds="t.MaxFPS 30"') == 1
                  and sum(a.startswith("-ExecCmds=") for a in args) == 2 and args.count("-AllKitOwnerToken=" + record["token"]) == 1
                  and args.count("-PktLag=" + str(expected_lag)) == 1 and args.count("-MaxFPS=30") == 1
                  and args.count("-AllKitRemoteArgus") == int(record["remoteArgus"])
                  and args.count("-AllKitOwnerAutoExit") == int(role != "host")
                  and ("-NullRHI" in args) != rendered and p["rendered"] is rendered
                  and (not rendered or all(a in args for a in ("-d3d12", "-ResX=640", "-ResY=360", "-ForceRes"))))
            check(f"{role}.process_exit", p.get("exited") is True)
            if role != "host":
                check(f"{role}.natural_exit", p.get("normalExit") is True and p.get("exitCode") == 0 and not p.get("closeRequested") and not p.get("forcedKill"))
        gameplay = verify_gameplay(ev, ids, record.get("remoteArgus"))
        checks.update(gameplay["checks"])
        if gameplay["checks"].get("complete_parse") is False:
            return {"passed": False, "checks": checks, "firstError": gameplay["firstError"]}
    except (KeyError, ValueError, TypeError, StopIteration) as exc:
        checks["complete_parse"] = False
        return {"passed": False, "checks": checks, "firstError": str(exc)}
    return {"passed": all(checks.values()) and bool(checks), "checks": checks,
            "firstError": next((k for k, v in checks.items() if not v), None),
            "renderedOwnerCoverage": gameplay["renderedOwnerCoverage"],
            "coverageScope": gameplay["coverageScope"]}


def controls(record, logs, lag):
    null_role = next(role for role in ROLES[1:] if role != record["renderedRemoteRole"])
    mutations = {
        "missing_owner": ("client1", r"BwayAllKitOwner: OwnerInput", "BwayAllKitOwner: RemovedOwnerInput"),
        "missing_peer": ("host", r"BwayAllKitOwner: PeerPASS", "BwayAllKitOwner: RemovedPeerPASS"),
        "missing_baseline": ("host", r"phase=Baseline", "phase=RemovedBaseline"),
        "missing_final": ("client2", r"BwayAllKitOwner: PeerFinalPASS", "BwayAllKitOwner: RemovedFinal"),
        "bad_token": ("client3", r"Ready token=\S+", "Ready token=stale"),
        "bad_health": ("host", r"(phase=Authority[^\r\n]*health=)[\d.]+", r"\g<1>9999"),
        "bad_local_health": ("client1", r"(phase=LocalPeer[^\r\n]*health=)[\d.]+", r"\g<1>9999"),
        "bad_local_leech": ("client1", r"(phase=LocalPeer[^\r\n]*leech=)[\d.]+", r"\g<1>0.2"),
        "bad_damage": ("host", r"(Plan[^\r\n]* damage=)[\d.]+", r"\g<1>9999"),
        "bad_leech": ("host", r"(Plan[^\r\n]* leech=)[\d.]+", r"\g<1>9999"),
        "bad_lag": ("client2", r"(Ready[^\r\n]* lag=)\d+", r"\g<1>999"),
        "bad_topology": (null_role, r"(Ready[^\r\n]* rhi=)\S+", r"\g<1>D3D12"),
        "bad_cleanup": ("host", r"CleanupAuthority proved=1", "CleanupAuthority proved=0"),
        "echoed_marker": ("host", r"LogTemp: Display: BwayAllKitOwner: Setup", "LogPython: Display: LogTemp: Display: BwayAllKitOwner: Setup"),
        "missing_credit_row": ("host", r"(phase=Authority case=21[^\r\n]* kills=)\d+", r"\g<1>9999"),
        "zero_damage": ("host", r"(Plan case=0 control=0[^\r\n]* damage=)[\d.]+", r"\g<1>0"),
        "zero_korryn_shred": ("host", r"(Plan case=10 control=0[^\r\n]* armorShred=)-?[\d.]+", r"\g<1>0"),
        "wrong_immune_primary_armor": ("host", r"(phase=Authority case=10 control=1[^\r\n]* armor=)-?[\d.]+", r"\g<1>9999"),
        "missing_credit_death": ("host", r"BwayAllKitOwner: CreditDeath", "BwayAllKitOwner: RemovedCreditDeath"),
        "wrong_credit_death_identity": ("host", r"(CreditDeath[^\r\n]* pawnKey=)\S+", r"\g<1>corrupt"),
        "reused_credit_victim": ("host", r"(CreditRespawn[^\r\n]* identityChanged=)1", r"\g<1>0"),
    }
    results = {}
    for name, (role, pattern, replacement) in mutations.items():
        changed = dict(logs)
        changed[role], count = re.subn(pattern, replacement, changed[role], count=1)
        results[name] = count == 1 and not verify(record, changed, lag)["passed"]
    for field in ("allConnectedAlive", "clientExitZero", "settingsRestored"):
        changed = copy.deepcopy(record)
        changed[field] = False
        results["false_" + field] = not verify(changed, logs, lag)["passed"]
    changed = dict(logs)
    changed["client2"] += "\nLogNet: Error: AllKit verifier injected unclassified error\n"
    results["unknown_native_error"] = not verify(record, changed, lag)["passed"]
    return {"passed": all(results.values()), "cases": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--expected-lag", type=int, choices=(0, 100), required=True)
    parser.add_argument("--controls", action="store_true")
    parser.add_argument("--reviewed-native-sha256", required=True, help="Reviewed AllKitCombatProbe.cpp hash in the frozen package archive")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Preserve prior verification; use a fresh output path.")
    try:
        record = json.loads(args.capture.read_text(encoding="utf-8-sig"))
        logs = {}
        identity = {}
        for p in record["processes"]:
            expected_log = args.capture.resolve().parent / f"allkit-{record['candidate']}-{record['attempt']}-{p['role']}.log"
            if Path(p["log"]).resolve() != expected_log:
                raise ValueError("Exact adjacent process log required")
            logs[p["role"]] = Path(p["log"]).read_text(encoding="utf-8-sig", errors="replace")
            identity[p["role"]] = digest(p["log"]) == p.get("logSHA256") and Path(p["log"]).stat().st_size == p["logBytes"]
        result = verify(record, logs, args.expected_lag)
        result.update(source=str(args.capture.resolve()), sourceSHA256=digest(args.capture), logIdentity=identity, scope=record.get("scope"))
        binding = bind(record, args.reviewed_native_sha256)
        result["binding"] = binding
        result["passed"] &= len(identity) == 4 and all(identity.values()) and all(binding.values())
        if not result["passed"] and not result["firstError"]:
            result["firstError"] = next(("binding." + k for k, v in binding.items() if not v), "log-identity")
        if args.controls:
            result["negativeControls"] = controls(record, logs, args.expected_lag) if result["passed"] else {"passed": False, "reason": "A valid real capture is required before corruption controls."}
            result["passed"] &= result["negativeControls"]["passed"]
    except (OSError, ValueError, KeyError, TypeError) as exc:
        result = {"passed": False, "checks": {}, "firstError": str(exc), "source": str(args.capture.resolve())}
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": result["passed"], "checks": len(result["checks"]), "firstError": result["firstError"], "output": str(args.output)}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
