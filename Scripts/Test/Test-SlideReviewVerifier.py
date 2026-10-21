"""Synthetic evidence-parser controls only; no Unreal/process or gameplay proof."""
import argparse
import copy
import importlib.util
import json
import math
from datetime import datetime, timedelta, timezone
from pathlib import Path

SOURCE = Path(__file__).with_name("Verify-SlideReview.py")
spec = importlib.util.spec_from_file_location("slide_review", SOURCE)
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


def fixture():
    rows = []
    for peer in range(4):
        seq = 0

        def emit(event, case=0, phase=1, **fields):
            nonlocal seq
            seq += 1
            fields.setdefault('serial', phase + case*6)
            fields.setdefault('wallSeconds', round(seq*.1, 7))
            rows.append(dict(schema=1, token="synthetic-slide-review", event=event, peer=peer, role=verifier.ROLES[peer],
                             player=257, case=case, caseName=verifier.CASES[case], phaseId=phase, seq=seq, **fields))

        emit("ReviewSetup", reviewSchema=1, courseVersion=2, seededConcaveFixture=1, courseRadius=40, courseActors=4, fixtureJumpHold=.3, originalJumpHold=0, physicalInput=0)
        emit("Setup", lagMs=100, pawnName='SourcePawn'+str(peer), correctionsEnabled=1 if peer == 1 else 0, tuning=";".join(k+"=0" for k in verifier.BYPASS))
        for case in (0, 1):
            if peer == 0:
                emit('ReviewRelocationBegin', case, 1 if case == 0 else 7, serial=1 if case == 0 else 7,
                     nextSerial=2 if case == 0 else 8, targetPhase=2, intentionalTeleport=1, maxSeconds=12,
                     positionTolerance=10, pawnName='SourcePawn0', **{'from':[0,0,100], 'to':[0,0,5000]})
                emit('ReviewRelocationEnd', case, 2, elapsedSeconds=.1, peerAcks=4,
                     position=[0,0,5000], targetPosition=[0,0,5000])
            if peer < 2 and case == 1:
                emit("ReviewConcaveSeed", case, 5, seededCollisionFixture=1, productionGasEntry=1,
                     entries=1, sliding=1, courseVersion=2, radius=40, halfHeight=50,
                     position=[-2, 14000, 5050.25], velocity=[620, 0, -140], floorDistance=.25, floorNormal=[0, 0, 1])
            for tick in range(4):
                emit("ReviewSample", case, 5, moveTs=.1+tick*.04, dt=.04, replay=0, position=[tick*4, 0, 5000], velocity=[100, 0, 0], penetrating=0)
            if peer < 2:
                if case == 0:
                    emit("ReviewJump", case, 5, moveTs=.1, dt=0, replay=0, wasJumping=0, boostApplied=1,
                         pressed=1, jumpCount=0, beforeSpeed=500, afterSpeed=550, boost=1.1, minSpeed=100, maxSpeed=1000)
                    for tick in range(3):
                        emit("ReviewJump", case, 5, moveTs=.13+tick*.05, dt=0, replay=0, wasJumping=1, boostApplied=0,
                             pressed=1, jumpCount=1, beforeSpeed=550, afterSpeed=550, boost=1.1, minSpeed=100, maxSpeed=1000)
                else:
                    emit("ReviewConcave", case, 5, moveTs=.1, dt=.03, replay=0, firstHit=.2, slideConsumed=.6, finalHit=.2,
                         continuation=.32, budget=1, legacyBudget=1.32, decisive=1, beforeVelocity=[600, 0, 0],
                         start=[0, 0, 5000], end=[10, 0, 5000], distance2D=10, distanceBound=19,
                         firstNormal=[0, 0, 1], finalNormal=[.2, 0, math.sqrt(.96)])
                emit("ReviewProof", case, 6, activations=1, ends=1, entries=1,
                     boosts=1 if case==0 else 0, heldCalls=3 if case==0 else 0, heldSeconds=.1 if case==0 else 0)
            emit("Settled", case, 7, position=[12, 0, 5000], targetPosition=[12, 0, 5000], velocity=[0, 0, 0], clean=1, supported=1, penetrating=0)
        emit("ReviewCleanup", 1, 8, jumpHoldRestored=1, observerRemoved=1)
        emit("Cleanup", 1, 8, packetsRestored=1, tuningUnchanged=1, movementCallbackRemoved=1, correctionsRestored=1,
             inputReleased=1, abilitiesInactive=1, observerCallbacksRemoved=1, lagMs=0, expectedLagMs=0)
        emit("ReviewRestored", 1, 9, courseRemoved=1, courseActors=0, jumpHold=0, originalJumpHold=0)
        emit("Restored", 1, 9, position=[0, 0, 100], targetPosition=[0, 0, 100], orientationRestored=1)
        emit("Complete", 1, 10, passed=1, cases=2, cleanup=1, physicalInput=0, trajectoryParity=0)
    return rows


def select(rows, event, peer=0, case=None):
    return next(r for r in rows if r["event"] == event and r["peer"] == peer and (case is None or r["case"] == case))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    controls = []

    def check(name, mutate=lambda rows: None, expected_failure=True):
        rows = copy.deepcopy(fixture()); mutate(rows)
        error = None
        try:
            verifier.verify_rows(rows)
        except (ValueError, TypeError, KeyError, IndexError) as exc:
            error = str(exc)
        controls.append({"name": name, "passed": bool(error) == expected_failure, "observedError": error})

    check("valid-four-peer-evidence", expected_failure=False)
    check("missing-proxy", lambda r: r.__setitem__(slice(None), [x for x in r if x["peer"] != 3]))
    check("held-input-released", lambda r: select(r, "ReviewJump").update(pressed=0, wasJumping=1))
    check("momentum-compounds", lambda r: next(x for x in r if x["event"] == "ReviewJump" and x.get("wasJumping") == 1).update(afterSpeed=605))
    check("repeat-boost", lambda r: next(x for x in r if x["event"] == "ReviewJump" and x.get("wasJumping") == 1).update(boostApplied=1))
    check("missing-held-samples", lambda r: r.__setitem__(slice(None), [x for x in r if not (x["peer"] == 0 and x["event"] == "ReviewJump" and x.get("wasJumping") == 1)]))
    check('duplicate-held-timestamp', lambda r: [x.update(moveTs=.13) for x in r if x['event']=='ReviewJump' and x.get('wasJumping')==1])
    check('backward-held-timestamp', lambda r: [x.update(moveTs=1-x['moveTs']) for x in r if x['event']=='ReviewJump'])
    check('short-span-despite-positive-dt', lambda r: [x.update(moveTs=x['moveTs']/10,dt=.5) for x in r if x['event']=='ReviewJump'])
    check('inflated-native-held-count', lambda r: select(r,'ReviewProof',0,0).update(heldCalls=99))
    check('inflated-native-held-duration', lambda r: select(r,'ReviewProof',0,0).update(heldSeconds=1))
    check('negative-raw-jump-dt', lambda r: select(r,'ReviewJump').update(dt=-1))
    check('zero-movement-dt-still-invalid', lambda r: select(r,'ReviewSample').update(dt=0))
    def held_before_launch(rows):
        for peer in (0,1):
            jumps = [r for r in rows if r['peer']==peer and r['event']=='ReviewJump']
            keys = ('wasJumping','boostApplied','pressed','jumpCount','beforeSpeed','afterSpeed','boost','minSpeed','maxSpeed')
            first, second = ({key:jumps[i][key] for key in keys} for i in (0,1))
            jumps[0].update(second); jumps[1].update(first)
            held = [r for r in jumps if r['wasJumping']==1]
            select(rows,'ReviewProof',peer,0)['heldSeconds'] = held[-1]['moveTs']-held[0]['moveTs']
    check('held-callback-before-first-boost',held_before_launch)
    check('jump-callback-outside-active-phase',lambda r: select(r,'ReviewJump').update(phaseId=7))
    check('missing-relocation-end', lambda r: r.remove(select(r,'ReviewRelocationEnd',0,0)))
    check('proxy-relocation', lambda r: select(r,'ReviewRelocationBegin').update(peer=2,role='proxy'))
    check('unacknowledged-relocation', lambda r: select(r,'ReviewRelocationEnd').update(peerAcks=3))
    check('mismatched-next-serial', lambda r: select(r,'ReviewRelocationBegin').update(nextSerial=3))
    check('oversized-relocation-deadline', lambda r: select(r,'ReviewRelocationBegin').update(maxSeconds=100))
    check('elapsed-duration-mismatch', lambda r: select(r,'ReviewRelocationEnd').update(elapsedSeconds=1))
    check('relocation-wrong-source-pawn', lambda r: select(r,'ReviewRelocationBegin').update(pawnName='OtherPawn'))
    check('relocation-wrong-destination', lambda r: select(r,'ReviewRelocationEnd').update(targetPosition=[1,0,5000]))
    check('case1-begin-wrong-previous-phase', lambda r: select(r,'ReviewRelocationBegin',0,1).update(phaseId=1))
    check("missing-collision-seed", lambda r: r.__setitem__(slice(None), [x for x in r if not (x["peer"] == 1 and x["event"] == "ReviewConcaveSeed")]))
    check("seed-outside-natural-entry", lambda r: select(r, "ReviewConcaveSeed").update(productionGasEntry=0))
    check("second-entry-seed", lambda r: select(r, "ReviewConcaveSeed").update(entries=2))
    check("seed-after-concave-result", lambda r: select(r, "ReviewConcaveSeed").update(seq=10000))
    check("wrong-course-version", lambda r: select(r, "ReviewSetup").update(courseVersion=1))
    check("changed-initial-velocity", lambda r: select(r, "ReviewConcaveSeed").update(velocity=[1200, 0, -140]))
    check("changed-initial-position", lambda r: select(r, "ReviewConcaveSeed").update(position=[-100, 14000, 5050.25]))
    check("seed-not-on-flat-floor", lambda r: select(r, "ReviewConcaveSeed").update(floorNormal=[-.2, 0, .98]))
    check("owner-server-capsule-mismatch", lambda r: select(r, "ReviewConcaveSeed", 1).update(radius=41))
    check("proxy-seeded-state", lambda r: select(r, "ReviewSample", 2, 1).update(event="ReviewConcaveSeed"))
    check("proxy-course-radius-mismatch", lambda r: select(r, "ReviewSetup", 2).update(courseRadius=39.99))
    check("executor-course-radius-mismatch", lambda r: select(r, "ReviewSetup", 1).update(courseRadius=39.99))
    check("missing-real-concave-branch", lambda r: r.__setitem__(slice(None), [x for x in r if not (x["peer"] == 1 and x["event"] == "ReviewConcave")]))
    check("last-hit-time-regression", lambda r: select(r, "ReviewConcave").update(continuation=.64, budget=1.32))
    check("nondecisive-single-contact", lambda r: select(r, "ReviewConcave").update(finalHit=.6, legacyBudget=1, decisive=0))
    check("displacement-overrun", lambda r: select(r, "ReviewConcave").update(end=[30, 0, 5000], distance2D=30))
    check("fabricated-distance-bound", lambda r: select(r, "ReviewConcave").update(distanceBound=200))
    check("fabricated-concave-normal", lambda r: select(r, "ReviewConcave").update(finalNormal=[0, 0, 1]))
    check("owner-divergence", lambda r: select(r, "ReviewSample", 1).update(position=[100, 0, 5000]))
    check("replay-is-not-fresh-proof", lambda r: [x.update(replay=1) for x in r if x["peer"] == 1 and x["event"] == "ReviewSample"])
    check("missing-timestamps", lambda r: [x.update(moveTs=-1) for x in r if x["event"] == "ReviewSample"])
    check("correction-bypass", lambda r: select(r, "Setup").update(tuning=";".join(k+"=1" for k in verifier.BYPASS)))
    check("residual-collision", lambda r: select(r, "ReviewRestored").update(courseRemoved=0, courseActors=4))
    check("hold-setting-not-restored", lambda r: select(r, "ReviewRestored").update(jumpHold=.3))
    check("cleanup-failure", lambda r: select(r, "Cleanup").update(abilitiesInactive=0))
    check("proxy-not-settled", lambda r: select(r, "Settled", 3).update(position=[100, 0, 5000]))
    check("duplicate-completion", lambda r: r.append(copy.deepcopy(select(r, "Complete"))))
    check("stationary-only-pairing", lambda r: [x.update(position=[0, 0, 5000]) for x in r if x["event"] == "ReviewSample"])
    # Concave endpoint control phases may differ at the same simulated move.
    # Keep first-case filtering and all count/span/error bounds unchanged.
    def concave_samples(rows, peer=None):
        return [r for r in rows if r['event'] == 'ReviewSample' and r['case'] == 1
                and (peer is None or r['peer'] == peer)]
    check('concave-matching-cross-phase', lambda r: [x.update(phaseId=6) for x in concave_samples(r, 0)], False)
    check('concave-both-release-held', lambda r: [x.update(phaseId=6) for x in concave_samples(r)], False)
    check('held-case-still-phase5-only', lambda r: [x.update(phaseId=6) for x in r if x['event']=='ReviewSample' and x['case']==0])
    check('concave-wrong-phase', lambda r: [x.update(phaseId=7) for x in concave_samples(r, 0)])
    check('concave-missing-owner-case', lambda r: r.__setitem__(slice(None), [x for x in r if x not in concave_samples(r, 1)]))
    check('concave-mismatched-owner-case', lambda r: [x.update(case=0, caseName=verifier.CASES[0]) for x in concave_samples(r, 1)])
    check('concave-mismatched-move-timestamps', lambda r: [x.update(moveTs=x['moveTs']+1) for x in concave_samples(r, 1)])
    check('concave-cross-phase-replay', lambda r: [x.update(phaseId=6, replay=1) for x in concave_samples(r, 0)])
    check('concave-cross-phase-position-divergence', lambda r: [x.update(phaseId=6, position=[100,0,5000]) for x in concave_samples(r, 0)])
    check('concave-cross-phase-velocity-divergence', lambda r: [x.update(phaseId=6, velocity=[200,0,0]) for x in concave_samples(r, 0)])
    check('concave-stationary-authority-substitution', lambda r: [x.update(phaseId=6, velocity=[0,0,0]) for x in concave_samples(r, 0)])
    check('concave-stationary-owner-substitution', lambda r: [x.update(velocity=[0,0,0]) for x in concave_samples(r, 1)])
    check('concave-one-authority-stop-cannot-hide-divergence', lambda r: concave_samples(r, 0)[1].update(velocity=[0,0,0]))
    check('concave-one-owner-stop-cannot-hide-divergence', lambda r: concave_samples(r, 1)[1].update(velocity=[0,0,0]))
    check('concave-two-stationary-pairs-cannot-count', lambda r: [x.update(velocity=[0,0,0]) for p in (0,1) for x in concave_samples(r, p)[1:3]])
    check('concave-speed-threshold-not-moving', lambda r: [x.update(velocity=[10,0,0]) for x in concave_samples(r)])
    check('concave-pair-count-still-three', lambda r: [x.update(moveTs=x['moveTs']+1) for x in concave_samples(r, 1)[2:]])
    check('concave-pair-span-still-60ms', lambda r: [x.update(moveTs=.1+(x['moveTs']-.1)/10) for x in concave_samples(r)])
    def move_concave_samples(rows, peer, event, after):
        stream = [r for r in rows if r['peer'] == peer]
        samples = concave_samples(stream)
        stream = [r for r in stream if r not in samples]
        boundary = select(stream, event, peer, 1)
        at = stream.index(boundary) + int(after)
        stream[at:at] = samples
        for seq, row in enumerate(stream, 1):
            row['seq'] = seq
        rows[:] = [r for r in rows if r['peer'] != peer] + stream
    check('concave-authority-before-own-seed', lambda r: move_concave_samples(r, 0, 'ReviewConcaveSeed', False))
    check('concave-owner-before-own-seed', lambda r: move_concave_samples(r, 1, 'ReviewConcaveSeed', False))
    check('concave-authority-after-own-proof', lambda r: move_concave_samples(r, 0, 'ReviewProof', True))
    check('concave-owner-after-own-proof', lambda r: move_concave_samples(r, 1, 'ReviewProof', True))
    # Exercise the actual file parser and native barrier checks as well as rows.
    def stamp(seconds):
        dt = datetime(2026,10,17,tzinfo=timezone.utc) + timedelta(seconds=seconds)
        return '['+dt.strftime('%Y.%m.%d-%H.%M.%S:')+f'{dt.microsecond//1000:03d}][001]'

    def correction(seconds=.35, side='Server', pawn='SourcePawn0', error=4900,
                   delta='X=0.000 Y=0.000 Z=4900.000', client='X=0.000 Y=0.000 Z=100.000',
                   server='X=0.000 Y=0.000 Z=5000.000', category='LogNetPlayerMovement'):
        return (stamp(seconds)+f'{category}: Warning: *** {side}: Error for {pawn} at Time=1.000 is {error:.3f} '
                f'LocDiff({delta}) ClientLoc({client}) ServerLoc({server}) Base: None')

    lines = []
    for row in fixture():
        if row["event"] == "Complete":
            kind = "Host" if row["peer"] == 0 else "Client"
            lines.append(f"LogTemp: Display: BwaySlidePackaged: {kind} PASS peer={row['peer']} player=257 cases=2 cleanup=1")
        lines.append(stamp(row['wallSeconds'])+"LogTemp: Display: BwaySlideTrajectory: " + json.dumps(row))
    lines.append("LogTemp: Display: BwaySlidePackaged: COMPLETE passed=1 player=257 cases=2 peers=4 cleanupAcks=4 finalAcks=4 authorityTransformRestored=1")
    text = "\n".join(lines) + "\n"
    inputs = args.output.parent / (args.output.stem + "-inputs")
    inputs.mkdir(parents=True, exist_ok=True)

    def parser_check(name, contents, expected_failure):
        path = inputs / (name + ".log"); path.write_text(contents, encoding="utf-8")
        error = None
        try:
            result = verifier.verify(logs=[path], token="synthetic-slide-review")
            expected_count = contents.count('*** Client: Error for') + contents.count('*** Server: Error for')
            verifier.require(result['correctionCount'] == len(result['corrections']) == expected_count, 'Correction omitted from result')
        except (ValueError, TypeError, KeyError, IndexError) as exc:
            error = str(exc)
        controls.append({"name": name, "passed": bool(error) == expected_failure, "observedError": error})

    parser_check("native-log-positive", text, False)
    parser_check("echoed-json-not-native-evidence", text.replace("LogTemp: Display:", "LogPython: Display:"), True)
    parser_check("missing-final-native-barrier", "\n".join(lines[:-1]), True)
    parser_check("wrong-token", text.replace("synthetic-slide-review", "unrelated-attempt"), True)
    parser_check("runtime-error", text + "LogTemp: Error: fixture failure\n", True)
    parser_check("excessive-correction", text + "LogCharacterMovement: Warning: *** Client: Error for Pawn at Time=1.0 is 25.0 LocDiff(x)\n", True)
    begin_line = next(line for line in lines if '"event": "ReviewRelocationBegin"' in line)
    def insert(line):
        return text.replace(begin_line,begin_line+'\n'+line,1)
    parser_check('observed-authority-teleport',insert(correction()),False)
    parser_check('observed-owner-teleport',insert(correction(side='Client',pawn='SourcePawn1',delta='X=0.000 Y=0.000 Z=-4900.000')),False)
    parser_check('zero-correction-counted',insert(correction(error=0,delta='X=0.000 Y=0.000 Z=0.000',client='X=0.000 Y=0.000 Z=5000.000')),False)
    parser_check('teleport-before-native-begin',insert(correction(seconds=.29)),True)
    parser_check('teleport-after-native-end',insert(correction(seconds=.41)),True)
    parser_check('teleport-after-end-line-same-time',text+correction(seconds=.4)+'\n',True)
    parser_check('teleport-missing-utc',insert(correction().split(']',2)[2]),True)
    parser_check('teleport-missing-marker-utc',insert(correction()).replace(stamp(.3),'',1),True)
    parser_check('teleport-other-pawn',insert(correction(pawn='OtherPawn')),True)
    parser_check('teleport-other-category',insert(correction(category='LogPython')),True)
    parser_check('teleport-inconsistent-magnitude',insert(correction(error=49)),True)
    parser_check('teleport-inconsistent-vector',insert(correction(delta='X=0.000 Y=0.000 Z=-4900.000')),True)
    parser_check('teleport-unrelated-client-location',insert(correction(error=4800,delta='X=0.000 Y=0.000 Z=4800.000',client='X=0.000 Y=0.000 Z=200.000')),True)
    parser_check('teleport-unrelated-server-location',insert(correction(error=5000,delta='X=0.000 Y=0.000 Z=5000.000',server='X=0.000 Y=0.000 Z=5100.000')),True)
    parser_check('small-correction-outside-relocation',text+correction(seconds=8,error=5,delta='X=0.000 Y=0.000 Z=5.000',client='X=0.000 Y=0.000 Z=4995.000')+'\n',False)
    # Real packaged output has four separate logs: use authority UTC windows,
    # never compare peer-relative wallSeconds or line numbers across files.
    paths = []
    for peer in range(4):
        peer_lines = []
        for row in fixture():
            if row['peer'] != peer:
                continue
            peer_lines.append(stamp(row['wallSeconds'])+'LogTemp: Display: BwaySlideTrajectory: '+json.dumps(row))
            if row['event'] == 'Setup' and peer == 1:
                peer_lines.append(correction(side='Client',pawn='SourcePawn1',delta='X=0.000 Y=0.000 Z=-4900.000'))
        kind = 'Host' if peer == 0 else 'Client'
        peer_lines.append(f'LogTemp: Display: BwaySlidePackaged: {kind} PASS peer={peer} player=257 cases=2 cleanup=1')
        if peer == 0:
            peer_lines.append(lines[-1])
        path = inputs / ('separate-peer-'+str(peer)+'.log')
        path.write_text('\n'.join(peer_lines)+'\n',encoding='utf-8'); paths.append(path)
    error = None
    try:
        result = verifier.verify(logs=paths,token='synthetic-slide-review')
        verifier.require(result['correctionCount'] == result['relocationCorrectionCount'] == 1, 'Cross-log relocation was not counted')
    except (ValueError, TypeError, KeyError, IndexError) as exc:
        error = str(exc)
    controls.append({'name':'separate-four-log-utc-relocation','passed':error is None,'observedError':error})
    out = {"passed": all(c["passed"] for c in controls), "count": len(controls), "controls": controls,
           "scope": "Synthetic parser positive and mutation controls only; no gameplay or native compilation evidence."}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": out["passed"], "count": out["count"], "failed": [c for c in controls if not c["passed"]], "output": str(args.output)}))
    return 0 if out["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
