"""Verify saved slide-review evidence. Never launches Unreal or claims physical input."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CASES = ("HeldJumpReview", "ConcaveContactReview")
ROLES = ("authority", "owner", "proxy", "proxy")
PREFIX = "BwaySlideTrajectory: "
LOG = re.compile(r"^(?:\[[^\]\r\n]*\])*\s*(Log\w+): (?:(Error|Warning|Display|Verbose|VeryVerbose): )?(.*)$")
BYPASS = ("ServerCorrectionBypass", "ClientCorrectionBypass", "AcceptClientPosition", "GlobalClientAuthority")
UTC_PREFIX = re.compile(r"^\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]")
NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"
VECTOR = rf"X=({NUMBER}) Y=({NUMBER}) Z=({NUMBER})"
CORRECTION = re.compile(rf"^\*\*\* (Client|Server): Error for (\S+) at Time=({NUMBER}) is\s+({NUMBER}) "
                        rf"LocDiff\({VECTOR}\) ClientLoc\({VECTOR}\) ServerLoc\({VECTOR}\)(?: .*)?$")


def require(value, reason):
    if not value:
        raise ValueError(reason)


def num(value):
    require(type(value) in (int, float) and math.isfinite(value), "Missing/nonfinite numeric evidence")
    return value


def vec(value):
    require(isinstance(value, list) and len(value) == 3, "Invalid evidence vector")
    return [num(x) for x in value]


def distance(a, b, dims=3):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(vec(a)[:dims], vec(b)[:dims])))


def one(rows, event, case=None):
    found = [r for r in rows if r["event"] == event and (case is None or r["case"] == case)]
    require(len(found) == 1, f"Missing/duplicate {event} case={case}")
    return found[0]


def log_utc(line):
    match = UTC_PREFIX.match(line)
    return datetime.strptime(match[1], "%Y.%m.%d-%H.%M.%S:%f").replace(tzinfo=timezone.utc).timestamp() if match else None


def relocation_windows(rows, timestamped=False):
    """Only the native authority's two bounded, acknowledged setup teleports."""
    authority = [r for r in rows if r['peer'] == 0]
    setup = one(authority, 'Setup')
    require(isinstance(setup.get('pawnName'), str) and setup['pawnName'], 'Missing source pawn identity')
    windows = []
    for case in (0, 1):
        begin, end = (one(rows, event, case) for event in ('ReviewRelocationBegin', 'ReviewRelocationEnd'))
        require(begin['peer'] == end['peer'] == 0 and begin['role'] == end['role'] == 'authority', 'Non-authority relocation marker')
        require(begin['phaseId'] == (1 if case == 0 else 7) and begin.get('targetPhase') == end['phaseId'] == 2
                and type(begin.get('serial')) is int and type(begin.get('nextSerial')) is int
                and begin['serial'] + 1 == begin['nextSerial'] == end['serial'], 'Relocation phase/serial mismatch')
        require(begin.get('intentionalTeleport') == 1 and begin.get('maxSeconds') == 12
                and begin.get('positionTolerance') == 10 and end.get('peerAcks') == 4
                and begin.get('pawnName') == setup['pawnName'], 'Unbounded/unacknowledged relocation identity')
        require(setup['seq'] < begin['seq'] < end['seq'], 'Relocation order mismatch')
        samples = [r for r in authority if r['event'] == 'ReviewSample' and r['case'] == case]
        require(samples and end['seq'] < min(r['seq'] for r in samples), 'Relocation ended after movement began')
        if case:
            require(one(authority, 'Settled', 0)['seq'] < begin['seq'], 'Second relocation preceded first case settling')
        elapsed = num(end.get('wallSeconds')) - num(begin.get('wallSeconds'))
        require(0 < elapsed <= 12 and abs(elapsed - num(end.get('elapsedSeconds'))) <= .002,
                'Relocation exceeded deadline or elapsed evidence disagrees')
        vec(begin['from']); vec(begin['to'])
        require(distance(begin['to'], end['targetPosition']) <= .001
                and distance(end['position'], begin['to']) <= 10, 'Relocation destination mismatch')
        if timestamped:
            require(begin.get('_utc') is not None and end.get('_utc') is not None,
                    'Relocation requires native UTC timestamps')
            require(begin['_logPath'] == end['_logPath'] == setup['_logPath']
                    and begin['_line'] < end['_line'] and 0 <= end['_utc'] - begin['_utc'] <= 12
                    and abs(end['_utc'] - begin['_utc'] - elapsed) <= .002,
                    'Relocation UTC/monotonic duration mismatch')
        windows.append((begin, end))
    return windows


def classify_corrections(rows, corrections):
    windows = relocation_windows(rows, timestamped=True)
    setups = {peer: one([r for r in rows if r['peer'] == peer], 'Setup') for peer in (0, 1)}
    classified = []
    for correction in corrections:
        require(correction['category'] == 'LogNetPlayerMovement' and correction['level'] == 'Warning', 'Non-native movement correction')
        match = CORRECTION.fullmatch(correction['body'])
        require(match is not None and correction['utc'] is not None, 'Malformed/untimestamped movement correction')
        side, pawn = match[1], match[2]
        values = [float(v) for v in match.groups()[2:]]
        require(all(math.isfinite(v) for v in values), 'Nonfinite correction')
        move_ts, error = values[:2]
        delta, client, server = values[2:5], values[5:8], values[8:11]
        setup = setups[0 if side == 'Server' else 1]
        require(pawn == setup.get('pawnName') and correction['path'] == setup['_logPath'], 'Correction source pawn/process mismatch')
        expected_delta = [s-c if side == 'Server' else c-s for c, s in zip(client, server)]
        require(move_ts >= 0 and error >= 0 and abs(error-distance(client, server)) <= .01
                and distance(delta, expected_delta) <= .01, 'Correction magnitude/vector disagrees with actual coordinates')
        classification, case = 'within-10cm', None
        if error > 10:
            matches = []
            for begin, end in windows:
                within = begin['_utc'] <= correction['utc'] <= end['_utc']
                if correction['path'] == begin['_logPath']:
                    within = within and begin['_line'] < correction['line'] < end['_line']
                if within and distance(server, begin['to']) <= 10 and min(distance(client, begin['from']), distance(client, begin['to'])) <= 10:
                    require(abs(error-distance(begin['from'], begin['to'])) <= 20.01, 'Correction differs from expected teleport displacement')
                    matches.append(begin)
            require(len(matches) == 1, 'Unbounded/unclassified movement correction')
            classification, case = 'observed-intentional-relocation', matches[0]['case']
        classified.append({'path': correction['path'], 'line': correction['line'], 'side': side, 'pawn': pawn,
                           'errorCm': error, 'classification': classification, 'case': case})
    return classified


def verify_rows(rows):
    require(rows and {r.get("peer") for r in rows} == {0, 1, 2, 3}, "Missing four peer evidence")
    require(len({r.get("player") for r in rows}) == 1, "Source player changed")
    peers = {p: [r for r in rows if r["peer"] == p] for p in range(4)}
    for peer, records in peers.items():
        require(all(r.get("schema") == 1 and r.get("role") == ROLES[peer] and r.get("case") in (0, 1)
                    and r.get("caseName") == CASES[r["case"]] for r in records), "Schema/role/case identity mismatch")
        seq = [num(r.get("seq")) for r in records]
        require(seq == sorted(set(seq)), "Duplicate or reordered peer events")
        setup = one(records, "ReviewSetup")
        require(setup.get("reviewSchema") == 1 and setup.get("courseVersion") == 2 and setup.get("seededConcaveFixture") == 1 and setup.get("courseActors") == 4
                and setup.get("fixtureJumpHold") == 0.3 and setup.get("physicalInput") == 0, "Wrong review fixture setup")
        normal_setup = one(records, "Setup")
        require(normal_setup.get("lagMs") == 100, "Missing 100ms lag")
        tuning = dict(item.split("=", 1) for item in normal_setup["tuning"].split(";"))
        require(all(float(tuning[key]) == 0 for key in BYPASS), "Movement correction bypass enabled")
        if peer == 1:
            require(normal_setup.get("correctionsEnabled") == 1, "Owner correction observation missing")
        cleanup = one(records, "Cleanup")
        for field in ("packetsRestored", "tuningUnchanged", "movementCallbackRemoved", "correctionsRestored", "inputReleased", "abilitiesInactive", "observerCallbacksRemoved"):
            require(cleanup.get(field) == 1, f"Cleanup failed: {field}")
        require(cleanup.get("lagMs") == cleanup.get("expectedLagMs"), "Packet settings not restored")
        review_cleanup = one(records, "ReviewCleanup")
        require(review_cleanup.get("jumpHoldRestored") == 1 and review_cleanup.get("observerRemoved") == 1, "Review observer/hold restoration failed")
        restored = one(records, "ReviewRestored")
        require(restored.get("courseRemoved") == 1 and restored.get("courseActors") == 0, "Temporary collision survived cleanup")
        require(restored.get("jumpHold") == restored.get("originalJumpHold") == setup.get("originalJumpHold"), "Jump hold changed after restoration")
        transform = one(records, "Restored")
        require(distance(transform["position"], transform["targetPosition"]) <= 10 and transform.get("orientationRestored") == 1, "Original transform not restored")
        complete = one(records, "Complete")
        require(complete.get("passed") == 1 and complete.get("cases") == 2 and complete.get("cleanup") == 1
                and complete.get("physicalInput") == 0 and complete.get("trajectoryParity") == 0, "Missing scoped completion")
        require(records[-1] is complete and review_cleanup["seq"] < cleanup["seq"] < restored["seq"] < transform["seq"] < complete["seq"], "Completion preceded restoration")
        for case in (0, 1):
            settled = one(records, "Settled", case)
            require(settled.get("clean") == 1 and settled.get("penetrating") == 0 and settled.get("supported") == 1
                    and distance(settled["position"], settled["targetPosition"]) <= 10
                    and distance(settled["velocity"], [0, 0, 0]) <= 10, "Peer not settled")
            require(settled["seq"] < review_cleanup["seq"], "Case completed after cleanup")
            samples = [r for r in records if r["event"] == "ReviewSample" and r["case"] == case]
            require(3 <= len(samples) <= 2000, "Missing/bounded movement samples")
            for r in samples:
                vec(r["position"]); vec(r["velocity"])
                require(num(r["dt"]) > 0 and r.get("penetrating") == 0, "Invalid movement/penetration sample")
                require(r["seq"] < review_cleanup["seq"], "Movement callback survived cleanup")
            if peer < 2:
                proof = one(records, "ReviewProof", case)
                require(proof["seq"] < settled["seq"] and all(proof.get(k) == 1 for k in ("activations", "ends", "entries")), "Lifecycle proof missing")
        seeds = [r for r in records if r["event"] == "ReviewConcaveSeed"]
        if peer >= 2:
            require(not seeds, "Proxy must not seed collision state")
            continue
        require(len(seeds) == 1, "Missing/duplicate natural-entry collision seed")
        seed = seeds[0]
        require(seed["case"] == 1 and seed.get("phaseId") == 5 and seed.get("courseVersion") == 2
                and all(seed.get(k) == 1 for k in ("seededCollisionFixture", "productionGasEntry", "entries", "sliding")),
                "Seed was not the first natural production slide entry")
        height, radius = num(seed["halfHeight"]), num(seed["radius"])
        require(height >= radius > 0 and abs(radius-num(setup["courseRadius"])) <= .001
                and distance(seed["position"], [-2, 14000, 5000+height+.25]) <= .001
                and distance(seed["velocity"], [620, 0, -140]) <= .001
                and -0.5 <= num(seed["floorDistance"]) <= 2.4 and vec(seed["floorNormal"])[2] > .999,
                "Incorrect collision fixture initial state")
        jumps = [r for r in records if r["event"] == "ReviewJump" and r["case"] == 0 and r.get("replay") == 0]
        boosts = [r for r in jumps if r.get("boostApplied") == 1]
        held = [r for r in jumps if r.get("wasJumping") == 1]
        jump_times = [num(r.get('moveTs')) for r in jumps]
        require(jump_times and min(jump_times) >= 0 and jump_times == sorted(set(jump_times)), 'Duplicate/backward jump timestamps')
        require(all(num(r.get('dt')) >= 0 for r in jumps), 'Invalid raw jump callback dt')
        held_times = [num(r.get('moveTs')) for r in held]
        held_span = held_times[-1] - held_times[0] if held_times else 0
        require(len(boosts) == 1 and len(held) >= 3 and held_span >= 0.08, "Held jump was not exercised or boost repeated")
        jump_proof = one(records, 'ReviewProof', 0)
        require(jump_proof.get('boosts') == 1 and jump_proof.get('heldCalls') == len(held)
                and abs(num(jump_proof.get('heldSeconds')) - held_span) <= .000001
                and max(r['seq'] for r in jumps) < jump_proof['seq'], 'Held proof disagrees with observed timestamps/count')
        launch = boosts[0]
        require(jumps[0] is launch and all(r['seq'] > launch['seq'] and r['moveTs'] > launch['moveTs'] for r in held),
                'Held jump preceded the first launch boost')
        require(all(r['phaseId'] == 5 for r in jumps), 'Jump callback outside active review phase')
        expected = max(num(launch["minSpeed"]), min(num(launch["maxSpeed"]), num(launch["beforeSpeed"]) * num(launch["boost"])))
        require(abs(num(launch["afterSpeed"]) - expected) <= 0.1 and launch.get("wasJumping") == 0, "First boost differs from production tuning")
        for r in held:
            require(r.get("boostApplied") == 0 and r.get("pressed") == 1 and r.get("jumpCount") == 1
                    and num(r["afterSpeed"]) <= num(launch["afterSpeed"]) + 1, "Held jump compounded or input not held")
        concave = [r for r in records if r["event"] == "ReviewConcave" and r["case"] == 1 and r.get("replay") == 0]
        decisive = 0
        for r in concave:
            require(seed["seq"] < r["seq"], "Concave result preceded natural-entry seed")
            first, consumed, final, remaining = (num(r[k]) for k in ("firstHit", "slideConsumed", "finalHit", "continuation"))
            require(all(0 <= f <= 1 for f in (first, consumed, final, remaining)), "Invalid contact time fraction")
            budget = first + (1 - first) * consumed + remaining
            legacy = first + (1 - first) * consumed + (1 - first) * (1 - final)
            require(budget <= 1.0001 and abs(budget - num(r["budget"])) < 0.0001, "Concave step exceeds its time budget")
            require(abs(remaining - (1 - first) * (1 - consumed)) < 0.0001 and abs(legacy - num(r["legacyBudget"])) < 0.0001, "Contact accounting inconsistent")
            a, b = vec(r["firstNormal"]), vec(r["finalNormal"])
            require(a[2] >= 0.8 and b[2] >= 0.8 and sum(x*y for x, y in zip(a, b)) < 0.999, "Concave floor contacts missing")
            bound = distance(r["beforeVelocity"], [0, 0, 0]) * num(r["dt"]) + 1
            travel = distance(r["start"], r["end"], 2)
            require(travel <= bound + 0.001 and abs(travel - num(r["distance2D"])) < 0.001
                    and abs(bound - num(r["distanceBound"])) < 0.001, "Independent displacement bound failed")
            decisive += int(legacy > 1.001 and consumed > final + 0.001)
        require(decisive > 0, "No actual multi-contact step distinguishes the old timing bug")
    require(len({num(one(peers[p], "ReviewSetup")["courseRadius"]) for p in range(4)}) == 1,
            "Peer collision courses use different radii")
    a_seed, o_seed = (one(peers[p], "ReviewConcaveSeed", 1) for p in (0, 1))
    require(distance(a_seed["position"], o_seed["position"]) <= .001
            and distance(a_seed["velocity"], o_seed["velocity"]) <= .001
            and a_seed["radius"] == o_seed["radius"] and a_seed["halfHeight"] == o_seed["halfHeight"],
            "Owner/server collision seed differs")
    pairs = []
    for case in (0, 1):
        def movement_samples(peer):
            if case == 0:
                return [r for r in peers[peer] if r["event"] == "ReviewSample" and r["case"] == case
                        and r.get("replay") == 0 and r.get("phaseId") == 5]
            # Phase is replicated control state; the same client move may be
            # Execute (5) on the owner and ReleaseHeld (6) on authority.
            # Bound each stream by its own natural-entry seed and lifecycle
            # proof. Retain stationary candidates so they cannot hide an
            # endpoint velocity disagreement at a matching timestamp.
            seed, proof = (one(peers[peer], event, case) for event in ("ReviewConcaveSeed", "ReviewProof"))
            return [r for r in peers[peer] if r["event"] == "ReviewSample" and r["case"] == case
                    and r.get("replay") == 0 and r.get("phaseId") in (5, 6)
                    and seed["seq"] < r["seq"] < proof["seq"]]
        authority, owner = movement_samples(0), movement_samples(1)
        matched = {}
        for a in authority:
            near = [o for o in owner if abs(num(a["moveTs"]) - num(o["moveTs"])) <= 0.0001]
            if near:
                o = near[-1]
                require(distance(a["position"], o["position"]) <= 10 and distance(a["velocity"], o["velocity"]) <= 60, "Owner/server moving-state divergence")
                if case == 0 or (distance(a["velocity"], [0, 0, 0]) > 10
                                 and distance(o["velocity"], [0, 0, 0]) > 10):
                    matched[a["moveTs"]] = True
        require(len(matched) >= 3 and max(matched) - min(matched) >= 0.06, "Insufficient timestamp-paired moving-state evidence")
        require(any(distance(a["position"], authority[0]["position"]) >= 3 for a in authority), "Stationary samples cannot prove moving-state convergence")
        pairs.append({"case": CASES[case], "pairedSamples": len(matched)})
    relocation_windows(rows)
    return {"cases": pairs, "peers": 4, "physicalInput": False}


def verify(result_path=None, logs=None, token=None):
    record = json.loads(Path(result_path).read_text(encoding="utf-8-sig")) if result_path else None
    if record:
        require(record.get("reviewRegression") is True and record.get("status") in ("completed-native-slide-transport-scope", "passed-slide-review-scope")
                and not record.get("firstCausalError") and not record.get("errors"), "Runner did not complete review transport")
        processes = record["processes"]
        require(len(processes) == 4 and {p["reportedPeer"] for p in processes} == {0, 1, 2, 3}, "Wrong process topology")
        for p in processes:
            args = p["arguments"]
            require(p.get("targetHz") == 30 and "-MaxFPS=30" in args, "Uncapped review process")
            require(("-d3d12" in args) == (p["reportedPeer"] in (0, 1)) and ("-NullRHI" in args) == (p["reportedPeer"] in (2, 3)), "Active owner/host were not both rendered")
            if p["reportedPeer"] != 0:
                require(p.get("exitCode") == 0 and p.get("cleanup") == "fixture-normal-exit", "Client teardown failed")
        logs = [Path(p["log"]) for p in processes]; token = record["token"]
    require(logs and token, "Logs and fresh token required")
    rows, files, native, corrections = [], [], [], []
    for path in logs:
        content = Path(path).read_bytes(); lines = content.decode("utf-8-sig", errors="replace").splitlines()
        active = False
        for line_number, line in enumerate(lines, 1):
            parsed = LOG.fullmatch(line)
            native_body = parsed[3] if parsed and parsed[1] == "LogTemp" and parsed[2] == "Display" else ""
            if native_body.startswith(PREFIX):
                row = json.loads(native_body[len(PREFIX):])
                if row.get("token") != token:
                    continue
                row.update(_utc=log_utc(line), _logPath=str(path), _line=line_number)
                active = True; rows.append(row)
            if active:
                require(not re.search(r"Fatal error:|Assertion failed:|Ensure condition failed:|Log\w+: Error:|BwaySlidePackaged: (?:FAIL|TIMEOUT)", line), "Runtime error during review fixture: " + line[:240])
                if native_body.startswith("BwaySlidePackaged:"):
                    native.append(native_body)
                if "*** Client: Error for" in line or "*** Server: Error for" in line:
                    require(parsed is not None, 'Unclassified movement correction log')
                    corrections.append({'path': str(path), 'line': line_number, 'utc': log_utc(line),
                                        'category': parsed[1], 'level': parsed[2], 'body': parsed[3]})
        files.append({"path": str(path), "sha256": hashlib.sha256(content).hexdigest()})
    result = verify_rows(rows)
    classified = classify_corrections(rows, corrections)
    for peer in range(4):
        kind = "Host" if peer == 0 else "Client"
        markers = [s for s in native if re.search(fr"BwaySlidePackaged: {kind} PASS peer={peer} player=\d+ cases=2 cleanup=1", s)]
        require(len(markers) == 1, "Missing/duplicate native peer PASS")
    require(sum(bool(re.search(r"BwaySlidePackaged: COMPLETE passed=1 player=\d+ cases=2 peers=4 cleanupAcks=4 finalAcks=4 authorityTransformRestored=1", s)) for s in native) == 1, "Missing/duplicate native completion barrier")
    result.update(token=token, logs=files, correctionCount=len(corrections), corrections=classified,
                  relocationCorrectionCount=sum(c['classification'] == 'observed-intentional-relocation' for c in classified),
                  topology="four-process" if record else "same-process PIE",
                  scope="Synthetic owner/server held-jump and seeded concave-collision regression with settled proxies; no authored concave run-up, physical input, visual feel, second-PC LAN, or historical trajectory parity.")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--result", type=Path); source.add_argument("--logs", nargs="+", type=Path)
    parser.add_argument("--token"); parser.add_argument("--scope", choices=("pie",))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = {"passed": False, "firstCausalError": None, "verifierSha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    try:
        require(not args.logs or args.scope == "pie", "Raw logs require explicit --scope pie")
        out.update(verify(args.result, args.logs, args.token)); out["passed"] = True
    except (OSError, ValueError, TypeError, KeyError, IndexError) as exc:
        out["firstCausalError"] = str(exc)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(out, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"passed": out["passed"], "firstCausalError": out["firstCausalError"], "output": str(args.output)}))
    return 0 if out["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
