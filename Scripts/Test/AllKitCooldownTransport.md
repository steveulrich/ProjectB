# Validate packaged all-kit cooldown transport

Use this helper only after the selected Goal 1 contract authorizes the run and the
candidate owner releases the package and process handles. It does not build,
package, or launch an editor. Follow the [packaged transport runbook](../../AI_Planning/Runbooks/packaged-transport-shop.md).

Each invocation runs all 16 ability families twice: 32 owner activations, 64 widget
samples, and 512 replicated peer rows. The host renders Argus. Choose one remote
rendered owner with `-RenderedOwnerHero Rawlins`, `Alona`, or `Korryn`; the other
two owners use NullRHI. Repeat a rotation only when its scope is required and its
run is authorized. The helper never starts three rotations automatically.

The evidence covers synthetic owner activation, authority cooldown duration,
rejection while cooling down, widget state, natural expiry, inactive and actor
cleanup, Flock movement restoration, and final fixture restoration. NullRHI widget
samples remain required. Missing samples fail the gate. This is not physical
input, visual presentation, full combat balance, or second-PC LAN acceptance.

## Export current authored configuration

In a fresh, owned editor whose source/assets match the successful package, run:

```python
exec(open(r"E:/Unreal Projects/ProjectB/Scripts/Test/Export-AllKitCooldownConfiguration.py").read())
export_configuration(r"<package-result.json>", r"<fresh Saved/Logs/cooldown-config.json>")
```

The read-only export reads all 16 authored cooldown fields and the production
catalog's rank-one cooldown reduction. It records the five asset hashes and the
package identity. It rejects dirty assets and existing output files. It does not
save assets. Close the owned editor normally before transport preflight.

## Preflight and run one rotation

Use PowerShell 7 from the project root. Supply the successful immutable package
record produced by the relic package helper, including its bound `LyraEditor`
and `LyraGame` compile records. The guard checks the full artifact manifest and
changed-input archive, and explicitly binds the unchanged cooldown probe and
exported assets through their HEAD blobs and working-byte hashes.

```powershell
$run = @{
    Candidate = '<LAN-YYYYMMDD-N>'
    Attempt = 'cooldown-rawlins-lag200-r1'
    RenderedOwnerHero = 'Rawlins'
    ExpectedLagMs = 200
    ExpectedExeSHA256 = '<64-character executable SHA256>'
    PackageResultPath = '<package-result.json>'
    ConfigurationPath = '<fresh cooldown-config.json>'
    GateContractPath = '<selected Goal 1 contract.json>'
}
& Scripts/Test/Run-AllKitCooldownTransport.ps1 @run -ValidateOnly
# Run only after candidate/handle handoff and run authorization:
& Scripts/Test/Run-AllKitCooldownTransport.ps1 @run
```

The runner increments the contract's `packagedRuns` count once, uses fresh
per-process UserDirs and unique Saved suffixes, and waits for an emitted listen
marker plus UDP 7777 owned by the host PID. Startup is staggered. The two rendered
processes use D3D12 at 640×360 and 30 fps; both witnesses use NullRHI at 30 fps.
The verifier requires emitted packet-lag, viewport, and cap-query evidence, in
addition to recorded launch settings. The unchanged probe has no native run-token
field; fresh owned log paths, process start times, UserDirs, Saved suffixes, and
final log hashes bind each process to its attempt.

The native fixture has a 1,500-second deadline. The runner's overall wait budget
is 1,660 seconds, including at most 60 seconds for natural client exits after the
four-live checkpoint, followed by at most 40 seconds of owned-process teardown.
Only the runner's own processes
are closed or killed. Forced client cleanup fails. Host teardown is recorded
separately. Original staged settings bytes and existence are restored, then
package/source/configuration hashes are checked again.

## Verify and test corruption rejection

The runner writes `allkit-cooldown-<candidate>-<attempt>-result.json`,
`-provenance.json`, four logs, and `-brief.json` beneath the evidence directory.
The brief must report `passed` and exact counts of 32/16/32/64/512 for
plans/families/activations/widget samples/peer rows. A transport-only result is
insufficient without the brief.

After an actual passing run, test copied evidence with a fresh output directory:

```powershell
& Scripts/Test/Test-AllKitCooldownVerifier.ps1 `
    -TransportResult '<allkit-cooldown-candidate-attempt-result.json>' `
    -OutputDirectory 'E:/Unreal Projects/ProjectB/Saved/Logs/<fresh-controls-directory>'
```

This preserves the 28 original corruption controls and adds 10 checks for
independent configuration, compile/HEAD binding, UDP readiness, UserDir reuse,
settings restoration, viewport, cap-query evidence, and owner network mode. The
control helper rehashes copied inputs so semantic assertions must detect each
corruption. It never edits original evidence. Synthetic checker inputs are
labeled `synthetic-checker-fixture`; a passing self-test reports
`passed-checker-fixture` and cannot count as a candidate runtime pass.

The historical `Get-AllKitCooldownBrief.ps1` remains unchanged for its original
four-rendered, 200 ms evidence scope.
