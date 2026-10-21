"""Audit real four-process Barrage lifecycle captures; never launches Unreal.

The caller supplies the exact independently reviewed native SHA256. This binds
the archived compiled input, not an arbitrary new schema or a passing marker.
Negative controls mutate the accepted capture only; no synthetic PASS fixture.
"""
import argparse
import copy
import csv
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ROLES = ("host", "client1", "client2", "client3")
SCENARIOS = {"damage-only": 1, "heal-damage": 2, "heal-only": 3, "death": 4, "round-reset": 5}
NATIVE = "Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/Development/BwayBarrageTransportProbe.cpp"
EMITTED = re.compile(r"^(?:\[[^\]]*\])*\s*(Log\w+): (?:(Display|Warning|Error|Fatal|Verbose|VeryVerbose): )?(.*)$")
NATIVE_FAILURE = re.compile(r"^(?:\[[^\]\r\n]*\]\s*)*\s*(?:[A-Za-z_]\w+:\s*(?:Error|Fatal):|Log\w*(?:Error|Fatal):|(?:Fatal error|Assertion failed|Ensure condition failed):)[^\r\n]*", re.MULTILINE)
FIELD = re.compile(r"(\w+)=([^\s]+)")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def child(parent, relative):
    parent = Path(parent).resolve()
    relative = Path(relative)
    if relative.is_absolute():
        raise ValueError("Manifest entry must be relative")
    path = (parent / relative).resolve()
    if not path.is_relative_to(parent) or path == parent:
        raise ValueError("Manifest path escapes root")
    return path


def assert_bytes(path, entry):
    if Path(path).stat().st_size != int(entry["bytes"]) or sha(path) != entry["sha256"].lower():
        raise ValueError("Manifest bytes changed: " + str(path))


def source_archive(manifest, require_effective_identity=True):
    files = {}
    for entry in manifest["files"]:
        key = entry["path"].replace("\\", "/")
        if key.casefold() in {p.casefold() for p in files}:
            raise ValueError("Duplicate archived source input")
        assert_bytes(child(manifest["archive"], entry["path"]), entry)
        files[key] = entry
    identity_text = manifest["sourceHead"] + "\n" + "\n".join(e["path"] + ":" + e["sha256"] for e in manifest["files"])
    if not files or (require_effective_identity and hashlib.sha256(identity_text.encode()).hexdigest() != manifest["effectiveSourceIdentity"].lower()):
        raise ValueError("Archived effective source identity mismatch")
    return files


def native_identity(manifest):
    entries = sorted((e for e in manifest["files"] if re.search(r"\.(cpp|h|cs|uproject|uplugin)$", e["path"], re.I)),
                     key=lambda e: e["path"].casefold())
    text = manifest["sourceHead"] + "\n" + "\n".join(e["path"] + ":" + e["sha256"] for e in entries)
    return hashlib.sha256(text.encode()).hexdigest(), len(entries)


def timestamp(text):
    return datetime.fromisoformat(text.replace("Z", "+00:00"))


def transport_envelope(runner, texts):
    """Shared new-runner launch invariants; legacy verifiers keep their counts."""
    processes = {p["role"]: p for p in runner["processes"]}
    if len(runner["processes"]) != 4 or set(processes) != set(ROLES) or len({p["processId"] for p in processes.values()}) != 4:
        raise ValueError("Four distinct owned transport processes required")
    if not (runner["udpPortAvailableBefore"] is True and runner["listenEndpoints"] and all(
            e["LocalPort"] == 7777 and e["OwningProcess"] == processes["host"]["processId"] for e in runner["listenEndpoints"])):
        raise ValueError("Owned UDP listen endpoint evidence required")
    for role, process in processes.items():
        args = process["args"]
        expected = Path(process["log"]).parent / f"packaged-{runner['candidate']}-{runner['attempt']}-users" / role
        suffix = f"{runner['candidate']}-{runner['attempt']}-{role}"
        if not (process["userDirectoryWasAbsent"] is True and Path(process["userDirectory"]).resolve() == expected.resolve()
                and process["engineSavedSuffix"] == suffix and args.count(f'-UserDir="{process["userDirectory"]}"') == 1
                and args.count("-enginesaveddirsuffix=" + suffix) == 1
                and sum(a.startswith("-UserDir=") for a in args) == 1
                and sum(a.startswith("-enginesaveddirsuffix=") for a in args) == 1):
            raise ValueError("Fresh isolated process settings binding required")
        is_lifecycle = runner["scenario"] in SCENARIOS
        lifecycle_args = [a for a in args if a.startswith("-BwayBarrageLifecycle=")]
        if lifecycle_args != (["-BwayBarrageLifecycle=" + runner["scenario"]] if is_lifecycle else []) or args.count("-BwayBarrageTransportChangingAim") != (1 if runner["scenario"] == "aim" else 0):
            raise ValueError("Explicit normal/aim/lifecycle arguments differ from selected mode")
        rendered = role in ("host", "client1")
        if (("-NullRHI" in args) == rendered or args.count("-MaxFPS=30") != 1
                or (rendered and not all(a in args for a in ("-d3d12", "-ResX=640", "-ResY=360", "-ForceRes")))):
            raise ValueError("Two-rendered/two-NullRHI topology changed")
        if NATIVE_FAILURE.search(texts[role]):
            raise ValueError(role + " contains an unclassified native error")


def parse(text):
    rows = []
    for line, raw in enumerate(text.splitlines(), 1):
        match = EMITTED.match(raw)
        if match:
            category, severity, body = match.groups()
            rows.append(dict(line=line, category=category, severity=severity, body=body, fields=dict(FIELD.findall(body))))
    return rows


def number(row, key):
    value = float(row["fields"][key])
    if not math.isfinite(value):
        raise ValueError("Nonfinite " + key)
    return value


def eq(row, **fields):
    return all(row["fields"].get(key) == str(value) for key, value in fields.items())


def audit(runner, texts, lag):
    checks = {}

    def check(name, result):
        checks[name] = bool(result)

    parsed = {role: parse(texts[role]) for role in ROLES}
    native_failures = {role: list(NATIVE_FAILURE.finditer(texts[role])) for role in ROLES}

    def rows(role, marker, **fields):
        prefix = "BwayBarrageLifecycle: " + marker + " "
        return [r for r in parsed[role] if r["category"] == "LogTemp" and r["severity"] == "Display"
                and r["body"].startswith(prefix) and eq(r, **fields)]

    def transport(role, marker):
        return [r for r in parsed[role] if r["category"] == "LogTemp" and r["severity"] == "Display"
                and r["body"].startswith("BwayBarrageTransport: " + marker + " ")]

    try:
        scenario = SCENARIOS[runner["scenario"]]
        count = 12 if scenario == 3 else 1
        check("runner_success_and_four_owned_exits", runner["status"] == "passed-transport-scope"
              and runner["hostReady"] and runner["allConnectedAlive"] and runner["clientExitZero"]
              and runner["allOwnedExited"] and runner["runError"] is None and runner["expectedLagMs"] == lag)
        processes = {r["role"]: r for r in runner["processes"]}
        check("exact_four_distinct_processes", set(processes) == set(ROLES) and len({r["processId"] for r in processes.values()}) == 4)
        check("owned_listen_endpoint", runner["udpPortAvailableBefore"] and bool(runner["listenEndpoints"])
              and all(e["LocalPort"] == 7777 and e["OwningProcess"] == processes["host"]["processId"] for e in runner["listenEndpoints"]))
        settings_roots = []
        for role, process in processes.items():
            expected = Path(process["log"]).parent / f"packaged-{runner['candidate']}-{runner['attempt']}-users" / role
            directory = Path(process["userDirectory"])
            suffix = f"{runner['candidate']}-{runner['attempt']}-{role}"
            settings_roots.append(directory.resolve())
            check(role + "_fresh_isolated_settings", process["userDirectoryWasAbsent"] is True and directory.resolve() == expected.resolve()
                  and process["engineSavedSuffix"] == suffix and process["args"].count(f'-UserDir="{process["userDirectory"]}"') == 1
                  and process["args"].count("-enginesaveddirsuffix=" + suffix) == 1
                  and sum(a.startswith("-UserDir=") for a in process["args"]) == 1
                  and sum(a.startswith("-enginesaveddirsuffix=") for a in process["args"]) == 1)
        check("four_distinct_settings_directories", len(set(settings_roots)) == 4)
        check("explicit_scenario_on_every_process", all("-BwayBarrageLifecycle=" + runner["scenario"] in p["args"]
              and "-BwayBarrageTransportChangingAim" not in p["args"] for p in processes.values()))
        check("two_rendered_two_nullrhi_topology", all(("-NullRHI" in processes[r]["args"]) == (r in ("client2", "client3")) for r in ROLES)
              and all("-MaxFPS=30" in p["args"] for p in processes.values())
              and all("-ResX=640" in processes[r]["args"] and "-ResY=360" in processes[r]["args"] for r in ("host", "client1")))
        check("all_clients_natural_zero_exit", all(processes[r]["exitCode"] == 0 and processes[r]["normalExit"]
              and not processes[r]["closeRequested"] and not processes[r]["forcedKill"] for r in ROLES[1:]))
        check("host_teardown_after_client_exits", processes["host"]["cleanupReason"] == "owned-host-after-all-client-exits")
        owner_roles = [r for r in ROLES if rows(r, "Mode", endpoint="owner")]
        check("exact_remote_owner", owner_roles == ["client1"])
        owner = "client1"
        source_ids = set()
        witness_ids = set()
        for role in ROLES:
            modes = rows(role, "Mode")
            check(role + "_one_exact_mode", len(modes) == 1 and eq(modes[0], scenario=scenario, guard=0)
                  and abs(number(modes[0], "frameLimit") - .2) < 1e-8 and number(modes[0], "oldGuid") > 0)
            if len(modes) == 1:
                source_ids.add(modes[0]["fields"]["source"])
                witness_ids.add(modes[0]["fields"]["witness"])
            check(role + "_no_emitted_errors", not native_failures[role]
                  and not any("BwayBarrageTransport: FAIL " in r["body"] for r in parsed[role]))
            network = transport(role, "Network")
            check(role + "_actual_driver_lag", len(network) == 1 and eq(network[0], lag=lag, lagMin=0, lagMax=0,
                  incomingMin=0, incomingMax=0, loss=0, incomingLoss=0, jitter=0, variance=0))
            complete = rows(role, "Complete")
            check(role + "_complete_and_cleanup", len(complete) == 1 and eq(complete[0], scenario=scenario, **{"pass": 1}, cleanup=1,
                  cues=count + 12, expectedCues=count + 12, terminalShots=count) and number(complete[0], "callbacksRemoved") >= 5)
            cleanup = transport(role, "Cleanup")
            check(role + "_observers_removed_and_tick_restored", len(cleanup) == 1 and eq(cleanup[0], remainingCallbacks=0, tickRestored=1, cancelDispatched=0))
            cues = rows(role, "Cue")
            check(role + "_exact_all_cues", len(cues) == count + 12 and all(eq(c, exact=1, scenario=scenario) for c in cues)
                  and [number(c, "index") for c in cues] == list(range(1, count + 13)))
            source_mode = modes[0] if len(modes) == 1 else None
            final_source = rows(role, "Roster", stage=17, player=source_mode["fields"]["source"]) if source_mode else []
            first_cues = rows(role, "Cue", cycle=1)
            reuse_cues = rows(role, "Cue", cycle=2)
            check(role + "_raw_cue_avatar_identity", source_mode is not None and len(final_source) == 1
                  and len(first_cues) == count and len(reuse_cues) == 12
                  and all(eq(c, source=source_mode["fields"]["source"], witness=source_mode["fields"]["witness"]) for c in cues)
                  and all(eq(c, pawn=source_mode["fields"]["oldPawn"], guid=source_mode["fields"]["oldGuid"]) for c in first_cues)
                  and eq(final_source[0], oldPawn=source_mode["fields"]["oldPawn"], oldGuid=source_mode["fields"]["oldGuid"])
                  and all(eq(c, pawn=final_source[0]["fields"]["pawn"], guid=final_source[0]["fields"]["guid"]) for c in reuse_cues))
            check(role + "_source_only_expected_health_changes", all(eq(r, allowed=1) and r["fields"]["player"] == r["fields"]["source"]
                  for r in rows(role, "Health")))
            for stage in (13, 14, 17):
                settled = rows(role, "Settled", stage=stage)
                expected_cues = count + (12 if stage == 17 else 0)
                check(role + f"_stage{stage}_settled", len(settled) == 1 and eq(settled[0], cues=expected_cues, expectedCues=expected_cues,
                      movementTag=0, active=0, montageActive=0, bullets=0) and number(settled[0], "samples") >= 5
                      and number(settled[0], "span") >= .25 and abs(number(settled[0], "health") - number(settled[0], "expectedHealth")) <= .01)
                roster = rows(role, "Roster", stage=stage)
                by_player = {r["fields"]["player"]: r for r in roster}
                check(role + f"_stage{stage}_four_lifetimes", len(roster) == len(by_player) == 4
                      and all((number(r, "replaced") == number(r, "expectedReplacement"))
                              and bool(number(r, "expectedReplacement")) == (scenario == 5 or (scenario == 4 and r["fields"]["player"] == r["fields"]["source"]))
                              and (r["fields"]["guid"] != r["fields"]["oldGuid"]) == bool(number(r, "expectedReplacement"))
                              and number(r, "guid") > 0 for r in roster))
                authority_roster = {r["fields"]["player"]: r for r in rows("host", "Roster", stage=stage)}
                check(role + f"_stage{stage}_authority_convergence", set(by_player) == set(authority_roster) and all(
                    all(r["fields"][k] == authority_roster[p]["fields"][k] for k in ("guid", "oldGuid", "team", "round", "replaced"))
                    and abs(number(r, "health") - number(authority_roster[p], "health")) <= .01 for p, r in by_player.items()))
            ready = rows(role, "CooldownReady")
            check(role + "_real_cooldown_ready", len(ready) == 1 and eq(ready[0], tag=0))
        check("one_source_four_witnesses", len(source_ids) == 1 and len(witness_ids) == 4)
        action = rows("host", "Intervention")
        check("one_first_shot_production_intervention", len(action) == 1 and eq(action[0], applied=1, shots=1, guard=0, scenario=scenario)
              and 0 <= number(action[0], "age") < number(action[0], "interval") == .1)
        if len(action) == 1 and scenario == 2:
            check("post_heal_damage_exceeds_precast_health", number(action[0], "afterHeal") > number(action[0], "after") > number(action[0], "prepared"))
        if scenario == 4:
            check("authority_real_death_observed", len(action) == 1 and eq(action[0], deathObserved=1)
                  and any(number(r, "after") == 0 for r in rows("host", "Health")))
        if scenario == 5:
            check("healthy_reset_no_health_damage", not rows("host", "Health") and len(action) == 1
                  and number(action[0], "before") == number(action[0], "after"))
        effects = rows("host", "HealthEffect")
        expected_reasons = {1: ["damage", "restore"], 2: ["prepare-deficit", "heal", "damage-after-heal", "restore"],
                            3: ["prepare-deficit", "heal", "restore"], 4: [], 5: []}[scenario]
        check("production_effect_sequence_and_restore", [r["fields"]["reason"] for r in effects] == expected_reasons
              and all(eq(r, guard=0) and r["fields"]["effect"] != "None"
                      and (number(r, "after") > number(r, "before")) == bool(number(r, "heal")) for r in effects))
        for role in ("host", owner):
            activations = rows(role, "Activation")
            ends = rows(role, "End")
            check(role + "_two_exact_activations", len(activations) == 2 and all(eq(r, exact=1) for r in activations)
                  and [number(r, "cycle") for r in activations] == [1, 2])
            check(role + "_two_exact_ends", len(ends) == 2 and all(eq(r, exact=1) and number(r, "activeAfter") <= 0 for r in ends)
                  and [number(r, "cycle") for r in ends] == [1, 2]
                  and eq(ends[1], cancelled=0) and (scenario == 5 or eq(ends[0], cancelled=0 if scenario == 3 else 1)))
            check(role + "_end_bound_to_activation", len(ends) == len(activations) == 2 and all(
                all(a["fields"][k] == e["fields"][k] for k in ("instance", "handle", "key", "pawn")) for a, e in zip(activations, ends)))
            rejected = rows(role, "EarlyReject")
            check(role + "_early_reactivation_rejected", len(rejected) == 1 and eq(rejected[0], rejected=1, activationsBefore=1, activationsAfter=1))
        shots = rows("host", "Shot")
        initialized = rows("host", "ShotInitialized")
        check("all_authority_shots_initialized", len(shots) == len(initialized) == count + 12
              and all(eq(s, exact=1, afterEnd=0) for s in shots) and all(eq(s, initialized=1) and number(s, "speed") == 3600 for s in initialized)
              and [number(s, "index") for s in shots] == list(range(1, count + 13)))
        ends = rows("host", "End")
        check("no_shots_after_exact_end", len(ends) == 2 and all(s["line"] < ends[int(number(s, "cycle")) - 1]["line"] for s in shots))
        tuning = rows("host", "Tuning")
        check("two_loaded_kit_checks", len(tuning) == 2 and all(eq(t, matched=1, count=12) and number(t, "interval") == .1
              and number(t, "life") == 1.5 and number(t, "cooldown") == 25 and number(t, "speed") == 3600 for t in tuning))
        captured = rows("host", "CooldownCaptured")
        expired = rows("host", "CooldownExpired")
        check("same_cooldown_expires_naturally", len(captured) == len(expired) == 1 and captured[0]["fields"]["handle"] == expired[0]["fields"]["handle"]
              and eq(expired[0], premature=0, rejected=0, natural=1) and number(captured[0], "duration") == number(expired[0], "duration") == 25
              and 25 <= number(expired[0], "age") <= 25.25)
        frames = transport("host", "FrameBound")
        retirements = rows("host", "FrameRetired")
        check("unrelaxed_200ms_frame_bound", bool(frames) and all(eq(f, valid=1, armed=1) and number(f, "limit") == .2
              and 0 <= number(f, "delta") <= .2 for f in frames) and len(retirements) == 2
              and all(eq(r, acknowledged=4) and number(r, "limit") == .2 and number(r, "peak") <= .2 for r in retirements))
        check("frame_bound_retired_after_settlement", len(retirements) == 2 and all(
            rows("host", "Settled", stage=stage)[0]["line"] < r["line"] for stage, r in zip((13, 17), retirements)))
        check("four_peer_authority_closeout", len(rows("host", "AllComplete", witnesses=4, ends=2, cleanup=1, shots=count + 12)) == 1)
    except (KeyError, ValueError, TypeError, IndexError) as error:
        check("well_formed_complete_capture", False)
        checks["parse_error_" + type(error).__name__] = False
    failed = [name for name, passed in checks.items() if not passed]
    return dict(passed=not failed, checks=checks, failedChecks=failed,
                errorCounts={role: len(native_failures[role]) for role in ROLES},
                unclassifiedNativeErrors={role: [m.group()[:600] for m in native_failures[role][:3]] for role in ROLES},
                warningCounts={role: sum(r["severity"] == "Warning" for r in parsed[role]) for role in ROLES})


def bind(path, reviewed_hash):
    runner = read_json(path)
    valid_schema = ((runner.get("schema") == "barrage-lifecycle-1" and runner.get("scenario") in SCENARIOS)
                    or (runner.get("schema") == "barrage-transport-1" and runner.get("scenario") in ("normal", "aim")))
    if not valid_schema or not re.fullmatch(r"[a-fA-F0-9]{64}", reviewed_hash):
        raise ValueError("Explicit Barrage transport schema/scenario and reviewed native SHA256 required")
    preflight_path = path.parent / f"{runner['candidate']}-{runner['attempt']}-launch-preflight.json"
    preflight = read_json(preflight_path)
    package_path = Path(preflight["package"])
    package = read_json(package_path)
    manifest_path = Path(package["sourceManifest"])
    manifest = read_json(manifest_path)
    files = source_archive(manifest)
    archive = child(manifest["archive"], NATIVE)
    entry = files[NATIVE]
    snapshot_path = Path(package["sourceSnapshot"])
    snapshot = read_json(snapshot_path)
    for kind in ("patch", "status", "untracked"):
        assert_bytes(snapshot["files"][kind]["path"], snapshot["files"][kind])
    snapshot_identity = hashlib.sha256((snapshot["sourceHead"] + "|" + "|".join(
        snapshot["files"][k]["sha256"] for k in ("patch", "status", "untracked"))).encode()).hexdigest()
    if not (package["status"] == "passed" and package["exitCode"] == 0 and package["moduleRestored"]
            and sha(archive) == reviewed_hash.lower() == entry["sha256"].lower()
                == preflight["reviewedNativeSHA256"].lower()
            and sha(package_path) == preflight["packageSHA256"].lower()
            and sha(manifest_path) == package["sourceManifestSha256"].lower() == preflight["sourceManifestSHA256"].lower()
            and package["effectiveSourceIdentity"] == manifest["effectiveSourceIdentity"] == preflight["effectiveSourceIdentity"]
            and sha(snapshot_path) == preflight["sourceSnapshotSHA256"].lower() == package["sourceSnapshotSha256"].lower()
            and snapshot_identity == package["sourceIdentity"].lower() == snapshot["sourceIdentity"].lower() == preflight["sourceIdentity"].lower()
            and package["sourceHead"] == manifest["sourceHead"] == snapshot["sourceHead"] == preflight["sourceHead"]
            and Path(preflight["sourceArchive"]).resolve() == Path(manifest["archive"]).resolve()
            and Path(preflight["sourceSnapshot"]).resolve() == snapshot_path.resolve()
            and Path(preflight["sourceManifest"]).resolve() == manifest_path.resolve()
            and preflight["candidate"] == runner["candidate"] == package["candidate"] and preflight["attempt"] == runner["attempt"]
            and preflight["scenario"] == runner["scenario"] and preflight["requestedLagMs"] == runner["expectedLagMs"]
            and preflight["runnerSHA256"].lower() == runner["runnerSHA256"].lower()
            and sha(preflight["runner"]) == runner["runnerSHA256"].lower()
            and sha(preflight["guardRunner"]) == preflight["guardRunnerSHA256"].lower()
            and sha(runner["executable"]) == runner["approvedExecutableSHA256"].lower()
                == runner["executableSHA256"].lower() == runner["executableSHA256After"].lower()):
        raise ValueError("Frozen package/native/runner/artifact binding mismatch")
    targets = set()
    final_native_identity, native_count = native_identity(manifest)
    for reference in package["compileRecords"]:
        record = read_json(reference["path"])
        if not (sha(reference["path"]) == reference["sha256"].lower() and record["status"] == "passed"
                and record["exitCode"] == 0 and not record["failureSignals"] and record["sourceHead"] == package["sourceHead"]
                and record["target"] == reference["target"] and reference["target"] not in targets):
            raise ValueError("Compile record binding mismatch")
        original = read_json(record["originalCompileRecord"])
        compile_manifest = read_json(record["nativeSourceManifest"])
        # The bound compile record hashes the whole earlier manifest and its
        # native subset identity; early archives need not have the later field.
        source_archive(compile_manifest, require_effective_identity=False)
        compiled_identity, compiled_count = native_identity(compile_manifest)
        if not (sha(record["originalCompileRecord"]) == record["originalCompileRecordSha256"].lower()
                and all(original[k] == record[k] for k in ("status", "exitCode", "sourceHead", "target", "startedUtc", "generatedUtc", "log"))
                and sha(record["nativeSourceManifest"]) == record["nativeSourceManifestSha256"].lower()
                and compile_manifest["sourceHead"] == package["sourceHead"]
                and timestamp(compile_manifest["createdUtc"]) <= timestamp(record["startedUtc"])
                and compiled_identity == final_native_identity == record["nativeSourceIdentity"].lower()
                and compiled_count == native_count == record["nativeInputCount"]
                and re.fullmatch(r"[A-Fa-f0-9]{64}", record["binarySha256"])
                and timestamp(record["binaryLastWriteUtc"]) >= timestamp(record["startedUtc"])
                and (record["target"] != "LyraGame" or record["binarySha256"].lower() == runner["approvedExecutableSHA256"].lower())):
            raise ValueError("Native archive/compile/binary binding mismatch")
        targets.add(reference["target"])
    if targets != {"LyraEditor", "LyraGame"}:
        raise ValueError("Editor and game compile records required")
    candidate_root = ROOT / "Saved" / "StagedBuilds" / runner["candidate"]
    artifact_manifest = Path(package["hashManifest"])
    if not (Path(package["stage"]).resolve() == candidate_root.resolve()
            and artifact_manifest.resolve() == (candidate_root / "sha256.csv").resolve()
            and Path(preflight["artifactManifest"]).resolve() == artifact_manifest.resolve()
            and sha(artifact_manifest) == preflight["artifactManifestSHA256"].lower()):
        raise ValueError("Candidate-root artifact manifest mismatch")
    with artifact_manifest.open(encoding="utf-8-sig", newline="") as stream:
        artifacts = list(csv.DictReader(stream))
    artifact_paths = set()
    for artifact in artifacts:
        artifact_path = child(candidate_root, artifact["path"])
        if artifact_path in artifact_paths:
            raise ValueError("Duplicate package artifact")
        assert_bytes(artifact_path, artifact)
        artifact_paths.add(artifact_path)
    actual_paths = {p.resolve() for p in candidate_root.rglob("*") if p.is_file()}
    runtime_saved = (candidate_root / "Windows" / "ProjectB" / "Saved").resolve()
    extra_paths = actual_paths - artifact_paths - {artifact_manifest.resolve()}
    unsafe_extras = {p for p in extra_paths if not p.is_relative_to(runtime_saved)
                     or p.suffix.lower() in (".exe", ".dll", ".pak", ".ucas", ".utoc", ".uasset", ".umap")}
    if not (not unsafe_extras and len(artifacts) == package["artifactCount"] and len(artifacts) > 0
            and Path(runner["executable"]).resolve() in artifact_paths and sha(runner["executable"]) == package["executableSha256"].lower()):
        raise ValueError("Complete cooked artifact binding mismatch")
    guard = read_json(preflight["settingsGuard"])
    if not (guard["restored"] and guard["status"] == "restored" and not guard["remainingProcesses"] and not guard["error"] and not guard["cleanupError"]):
        raise ValueError("Exact settings restoration and owned process cleanup required")
    if guard["existedBefore"]:
        if not (sha(guard["backup"]) == guard["beforeSHA256"].lower() == guard["restoredSHA256"].lower()
                and sha(guard["settings"]) == guard["beforeSHA256"].lower()):
            raise ValueError("Original settings bytes not restored")
    elif Path(guard["settings"]).exists():
        raise ValueError("Original settings absence not restored")
    texts = {}
    for process in runner["processes"]:
        user_dir = Path(process["userDirectory"]).resolve()
        if not user_dir.is_dir():
            raise ValueError("Per-process settings evidence directory missing")
        for settings in process["runtimeSettingsFiles"]:
            if not Path(settings["path"]).resolve().is_relative_to(user_dir):
                raise ValueError("Runtime settings escaped isolated process directory")
            assert_bytes(settings["path"], settings)
        data = Path(process["log"]).read_bytes()
        if len(data) != process["logBytes"] or hashlib.sha256(data).hexdigest() != process["logSHA256"].lower():
            raise ValueError("Saved log bytes changed")
        texts[process["role"]] = data.decode("utf-8-sig", errors="replace")
    if set(texts) != set(ROLES):
        raise ValueError("All four process logs required")
    transport_envelope(runner, texts)
    return runner, texts, dict(nativeSHA256=sha(archive), reviewedNativeSHA256=reviewed_hash.lower(), package=str(package_path),
                               manifest=str(manifest_path), preflight=str(preflight_path), guard=preflight["settingsGuard"])


def controls(runner, texts, lag):
    cases = []

    def record_case(name, altered, required):
        verdict = audit(runner, altered, lag)
        cases.append(dict(name=name, passed=not verdict["passed"] and (required is None or required in verdict["failedChecks"]), failedChecks=verdict["failedChecks"]))

    def change(name, role, marker, field=None, value=None, required=None, cycle=None):
        altered = copy.deepcopy(texts)
        cycle_filter = r"(?=[^\r\n]*\bcycle=" + str(cycle) + r"\b)" if cycle is not None else ""
        pattern = re.compile(r"(?m)^(?:\[[^\]\r\n]*\]\s*)*LogTemp: Display: BwayBarrageLifecycle: " + re.escape(marker) + " " + cycle_filter + r".*$(?:\r?\n)?")
        match = pattern.search(altered[role])
        if not match:
            raise ValueError("Missing real control input: " + marker)
        replacement = "" if field is None else re.sub(r"(?<!\w)" + re.escape(field) + r"=\S+", field + "=" + str(value), match.group(), count=1)
        if replacement == match.group():
            raise ValueError("No-op control: " + name)
        altered[role] = altered[role][:match.start()] + replacement + altered[role][match.end():]
        record_case(name, altered, required)

    change("missing authority intervention", "host", "Intervention", required="one_first_shot_production_intervention")
    change("late first-shot intervention", "host", "Intervention", "age", 1, "one_first_shot_production_intervention")
    change("wrong owner activation key", "client1", "Activation", "key", 999999, "client1_end_bound_to_activation")
    change("missing proxy completion", "client3", "Complete", required="client3_complete_and_cleanup")
    change("missing proxy cue", "client2", "Cue", required="client2_exact_all_cues")
    change("unexpected health mutation", "host", "Intervention", "applied", 0, "one_first_shot_production_intervention")
    change("premature cooldown removal", "host", "CooldownExpired", "premature", 1, "same_cooldown_expires_naturally")
    change("changed cooldown identity", "host", "CooldownExpired", "handle", "999", "same_cooldown_expires_naturally")
    change("unsafe frame bound", "host", "FrameRetired", "peak", .201, "unrelaxed_200ms_frame_bound")
    change("missing initialized shot", "host", "ShotInitialized", required="all_authority_shots_initialized")
    change("cue identity assertion false", "client1", "Cue", "exact", 0, "client1_exact_all_cues")
    change("raw original cue GUID corrupted", "client2", "Cue", "guid", 0, "client2_raw_cue_avatar_identity", cycle=1)
    change("raw reuse cue pawn corrupted", "client3", "Cue", "pawn", "WrongAvatar", "client3_raw_cue_avatar_identity", cycle=2)
    change("missing settled witness", "client3", "Settled", required="client3_stage13_settled")
    echoed = copy.deepcopy(texts)
    echoed["client3"], replacements = re.subn(r"(?m)^((?:\[[^\]\r\n]*\]\s*)*)LogTemp: Display: (BwayBarrageLifecycle: Complete )",
                                             r"\1LogPython: Display: \2", echoed["client3"], count=1)
    if replacements != 1:
        raise ValueError("Missing real native marker for echo control")
    record_case("Python echo cannot replace native completion", echoed, "client3_complete_and_cleanup")
    for name, line in (("unclassified native severity error", "LogNet: Error: Lifecycle verifier negative control"),
                       ("unclassified native error category", "LogNetError: Lifecycle verifier negative control")):
        altered = copy.deepcopy(texts)
        altered["client2"] += "\n" + line + "\n"
        record_case(name, altered, "client2_no_emitted_errors")
    return dict(passed=all(c["passed"] for c in cases), count=len(cases), cases=cases,
                scope="Negative mutations of this real accepted capture only; no runtime or fabricated passing data")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runner", type=Path)
    parser.add_argument("--expected-lag", type=int, choices=(0, 100), required=True)
    parser.add_argument("--reviewed-native-sha256", required=True)
    parser.add_argument("--controls", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = dict(passed=False, scope=__doc__.splitlines()[0], runner=str(args.runner), firstCausalError=None)
    try:
        runner, texts, binding = bind(args.runner.resolve(), args.reviewed_native_sha256)
        verdict = audit(runner, texts, args.expected_lag)
        result.update(binding=binding, audit=verdict, passed=verdict["passed"], scenario=runner["scenario"], expectedLagMs=args.expected_lag)
        if args.controls and verdict["passed"]:
            result["controls"] = controls(runner, texts, args.expected_lag)
            result["passed"] &= result["controls"]["passed"]
        result["firstCausalError"] = None if result["passed"] else (verdict["failedChecks"][0] if verdict["failedChecks"] else "negative-control-accepted")
    except (OSError, KeyError, ValueError, StopIteration, TypeError) as error:
        result["firstCausalError"] = str(error)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists():
        raise FileExistsError("Preserve existing verdict: " + str(args.output))
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": result["passed"], "firstCausalError": result["firstCausalError"], "checks": len(result.get("audit", {}).get("checks", {})), "evidence": str(args.output)}))
    raise SystemExit(0 if result["passed"] else 1)


if __name__ == "__main__":
    main()
