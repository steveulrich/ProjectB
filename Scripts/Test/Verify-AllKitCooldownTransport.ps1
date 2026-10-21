[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HostLog,
    [Parameter(Mandatory)][string[]]$ClientLogs,
    [Parameter(Mandatory)][string]$TransportResult,
    [Parameter(Mandatory)][string]$ArtifactVerification,
    [Parameter(Mandatory)][string]$SourceAudit,
    [Parameter(Mandatory)][string]$OutputJson
)
$ErrorActionPreference = 'Stop'
$problems = [Collections.Generic.List[string]]::new()
$rows = [Collections.Generic.List[object]]::new()
$inventory = [Collections.Generic.List[object]]::new()
$paths = @($HostLog) + @($ClientLogs)
if ($ClientLogs.Count -ne 3 -or @($paths | Sort-Object -Unique).Count -ne 4) { $problems.Add('Require one host and three distinct client logs.') }
function Check([bool]$Condition, [string]$Message) { if (!$Condition) { $problems.Add($Message) } }
function Near([double]$A, [double]$B, [double]$Tolerance) { return [math]::Abs($A - $B) -le $Tolerance }
function Read-Record([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { $problems.Add("Missing record: $Path"); return $null }
    try { return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json } catch { $problems.Add("Invalid JSON: $Path"); return $null }
}
$transport = Read-Record $TransportResult
$audit = Read-Record $SourceAudit
$artifact = Read-Record $ArtifactVerification
$roleNames=@('host','client1','client2','client3')
$heroNames=@('Argus','Rawlins','Alona','Korryn')
$renderedRole=@{Rawlins='client1';Alona='client2';Korryn='client3'}[[string]$transport.renderedOwnerHero]
$lag=[int]$transport.expectedLagMs
function File-Hash([string]$Path){if(Test-Path -LiteralPath $Path -PathType Leaf){return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash};return $null}
Check ($transport.schema -eq 'allkit-cooldown-transport-1' -and $renderedRole -and $transport.renderedOwnerRole -eq $renderedRole -and $lag -in @(0,100,200)) 'Unknown transport schema, owner rotation or lag.'
for ($i = 0; $i -lt $paths.Count; $i++) {
    $path = $paths[$i]
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { $problems.Add("Missing log: $path"); continue }
    $lines = @(Get-Content -LiteralPath $path)
    $role=$roleNames[$i]; $rendered=$role -in @('host',$renderedRole)
    $process=@($transport.processes | Where-Object role -eq $role)
    Check ($process.Count -eq 1) "Log $i has no unique owned process."
    if($process.Count -eq 1){
        Check ((File-Hash $path) -eq $process[0].logSHA256) "Log $i changed after owned run."
        Check ($process[0].rendered -eq $rendered -and $process[0].hero -eq $heroNames[$i]) "Log $i rendered/hero role differs."
        Check ([datetime]$process[0].startedUtc -ge [datetime]$transport.startedUtc -and [datetime]$process[0].startedUtc -lt [datetime]$transport.finishedUtc) "Log $i process is not from this run."
        $suffix="-enginesaveddirsuffix=$($transport.token)-$role"
        Check (@($lines | Where-Object {$_ -match 'LogInit: Command Line:' -and $_.Contains($suffix) -and $_.Contains($process[0].userDir)}).Count -eq 1) "Log $i lacks the fresh per-process run identity."
    }
    $d3d=@($lines | Where-Object { $_ -match 'LogRHI: Loading RHI module D3D12RHI' }).Count -gt 0
    Check ($d3d -eq $rendered) "Log $i has the wrong rendered D3D12 evidence."
    if($rendered){Check (@($lines | Where-Object {$_ -match 'LogViewport:.*Scene viewport resized to 640x360, mode Windowed'}).Count -gt 0) "Log $i lacks observed 640x360 viewport."}
    Check (@($lines | Where-Object { $_ -match "LogNet: PktLag set to $lag\b" }).Count -gt 0) "Log $i has no observed $lag ms packet lag."
    Check (@($lines | Where-Object {$_ -notmatch 'Command Line:|ExecCmds' -and $_ -match '\bt\.MaxFPS = "30(?:\.0+)?"\s+LastSetBy:'}).Count -gt 0) "Log $i lacks observed 30 fps cap query."
    $errors = @($lines | Where-Object { $_ -match '\bError:|Ensure condition failed|Assertion failed|Fatal error' })
    $known = @($errors | Where-Object { $_ -match 'LogZenServiceInstance: Error: Failed to write security configuration' })
    $unknown = @($errors | Where-Object { $_ -notmatch 'LogZenServiceInstance: Error: Failed to write security configuration' })
    Check ($unknown.Count -eq 0) "Log $i has $($unknown.Count) actionable error/critical lines."
    $inventory.Add([pscustomobject]@{index=$i;path=$path;errors=$errors.Count;knownZenErrors=$known.Count;unknownErrors=$unknown.Count;firstError=$unknown | Select-Object -First 1;warnings=@($lines | Where-Object { $_ -match '\bWarning:' }).Count})
    foreach ($line in $lines) {
        # Match emitted category lines only, never command-line text.
        if ($line -notmatch 'LogTemp: (?:Display|Error): BwayAllKitCooldown: (.+)$') { continue }
        $body = $Matches[1]
        $split = $body.IndexOf('=')
        $kind = if ($body -match '^([A-Za-z]+(?: (?:BEGIN|EXPIRE|REMOVE|PASS))?)\s') { $Matches[1] } else { $body }
        $row = [ordered]@{log=$i;kind=$kind}
        foreach ($kv in [regex]::Matches($body, '(\w+)=([^\s]+)')) { $row[$kv.Groups[1].Value] = $kv.Groups[2].Value }
        $rows.Add([pscustomobject]$row)
        if ($body -match '^FAIL\b') { $problems.Add("Fixture failure log $i : $body") }
    }
}
function Find-Rows([string]$Kind, [int]$Case = -1, [int]$Cycle = -1, [int]$Log = -1) {
    return @($rows | Where-Object { $_.kind -eq $Kind -and ($Case -lt 0 -or [int]$_.case -eq $Case) -and ($Cycle -lt 0 -or [int]$_.cycle -eq $Cycle) -and ($Log -lt 0 -or $_.log -eq $Log) })
}
Check ($artifact -and $artifact.passed -eq $true -and $artifact.fileCount -gt 0 -and $artifact.stagedHash -eq $artifact.compiledHash -and @($artifact.changedBuildInputs).Count -eq 0) 'Artifact identity is not verified.'
Check ($artifact -and $artifact.stagedHash -match '^[0-9A-Fa-f]{64}$' -and $artifact.compiledHash -match '^[0-9A-Fa-f]{64}$') 'Artifact identity hashes are missing or malformed.'
Check ($transport -and $transport.status -eq 'passed-transport-scope' -and $transport.clientExitZero -eq $true -and $transport.allConnectedAlive -eq $true -and $null -eq $transport.runError) 'Four-live transport and normal client exit proof is incomplete.'
Check ($artifact -and $transport -and $artifact.candidate -eq $transport.candidate) 'Artifact and runtime candidates differ.'
Check ($transport -and @($transport.processes).Count -eq 4 -and @($transport.processes | Where-Object { $_.role -like 'client*' -and $null -ne $_.exitCode -and $_.exitCode -eq 0 }).Count -eq 3) 'Missing four process records or three explicit zero client exits.'
Check ($transport -and (@($transport.processes.role | Sort-Object) -join ',') -eq 'client1,client2,client3,host' -and $transport.latencyCommand -eq "Net PktLag=$lag") 'Topology/latency contract does not match the requested gate.'
Check ($transport.hostReady -eq $true -and $transport.udpReadiness.port -eq 7777 -and $transport.udpReadiness.processId -eq ($transport.processes | Where-Object role -eq 'host').processId -and @($transport.startupReadiness).Count -eq 3) 'Actual owned UDP and client readiness are missing.'
Check ($transport.allOwnedExited -eq $true -and $transport.settingsRestored -eq $true -and $transport.provenanceRechecked -eq $true) 'Process/settings/provenance cleanup is incomplete.'
Check ((File-Hash $ArtifactVerification) -eq $transport.artifactVerificationSHA256 -and (File-Hash $SourceAudit) -eq $transport.configurationSHA256) 'Frozen provenance/configuration evidence changed.'
Check ($artifact.schema -eq 'allkit-cooldown-provenance-1' -and $artifact.configurationSHA256 -eq $transport.configurationSHA256 -and $artifact.packageResultSHA256 -eq $transport.packageResultSHA256 -and $artifact.effectiveSourceIdentity -eq $transport.effectiveSourceIdentity -and @($artifact.compileRecords).Count -eq 2 -and @($artifact.bindings).Count -eq 7) 'Bound package/compile/configuration provenance is incomplete.'
Check ($audit.schema -eq 'allkit-cooldown-config-1' -and $audit.status -eq 'validated-authored-config' -and $audit.candidate -eq $transport.candidate -and $audit.effectiveSourceIdentity -eq $transport.effectiveSourceIdentity -and $audit.packageResultSHA256 -eq $transport.packageResultSHA256) 'Independent candidate configuration validation is missing.'
if ($transport) {
    for ($i=0; $i -lt $paths.Count; $i++) { Check (@($transport.processes | Where-Object { [IO.Path]::GetFullPath($_.log) -eq [IO.Path]::GetFullPath($paths[$i]) }).Count -eq 1) "Log $i does not belong to transport record." }
    Check (@($transport.processes.userDir | Sort-Object -Unique).Count -eq 4 -and @($transport.processes.processId | Sort-Object -Unique).Count -eq 4) 'Process IDs/UserDirs are not distinct.'
    foreach ($process in $transport.processes) {
        $rendered=$process.role -in @('host',$renderedRole)
        Check ((@($process.args) -match '(?i)^-nullrhi$').Count -eq $(if($rendered){0}else{1}) -and (@($process.args) -contains '-MaxFPS=30')) "$($process.role) has wrong NullRHI/cap topology."
        if($rendered){foreach($arg in @('-d3d12','-windowed','-ResX=640','-ResY=360','-ForceRes')){Check (@($process.args) -contains $arg) "$($process.role) lacks $arg."}}
        if($process.role -ne 'host'){Check ($process.normalExit -eq $true -and !$process.forcedKill -and !$process.closeRequested) "$($process.role) was forcibly cleaned up."}
    }
    $hostProcess=@($transport.processes | Where-Object role -eq 'host')
    Check ($hostProcess.Count -eq 1 -and $null -ne $hostProcess[0].exitCode -and $transport.hostTeardown -eq 'clients-probe-destroyed-normal-exit; host-owned-teardown-after-all-client-exits') 'Owned host teardown is not recorded separately after client completion.'
}
Check ($audit -and $audit.abilityCount -eq 16) 'Missing 16-family source audit.'
$selected = Find-Rows 'Selected' -Log 0
Check ($selected.Count -eq 4 -and @($selected.player | Sort-Object -Unique).Count -eq 4 -and @($selected.hero | Sort-Object -Unique).Count -eq 4) 'Production selection did not establish four distinct heroes/players.'
$plans = Find-Rows 'Plan' -Log 0
Check ($plans.Count -eq 32) 'Expected exactly 32 authority plans.'
$families = [Collections.Generic.List[string]]::new()
for ($case = 0; $case -lt 16; $case++) {
    for ($cycle = 1; $cycle -le 2; $cycle++) {
        $planRows = Find-Rows 'Plan' $case $cycle 0
        if ($planRows.Count -ne 1) { $problems.Add("Plan $case/$cycle missing or duplicate."); continue }
        $plan = $planRows[0]
        $expected = [double]$plan.base * (1.0 - [double]$plan.bonus)
        $reference=@($audit.abilities | Where-Object {[int]$_.case -eq $case})
        Check ($reference.Count -eq 1 -and $reference[0].hero -eq $plan.hero -and (Near ([double]$reference[0].baseDuration) ([double]$plan.base) 0.001) -and (Near ([double]$audit.cooldownReduction) ([double]$plan.bonus) 0.001)) "Plan $case/$cycle differs from independently exported configuration."
        Check ((Near $expected ([double]$plan.expected) 0.001) -and $expected -gt 0 -and [double]$plan.bonus -gt 0 -and [double]$plan.bonus -lt 0.5) "Plan $case/$cycle formula invalid."
        Check ([int]$plan.serial -eq ($case*2+$cycle) -and [int]$plan.clearRoute -eq 1300) "Plan $case/$cycle sequence/route invalid."
        Check (@($selected | Where-Object { $_.hero -eq $plan.hero -and $_.player -eq $plan.source }).Count -eq 1) "Plan $case/$cycle source disagrees with selection."
        Check ($plan.source -ne $plan.enemy -and $plan.source -ne $plan.ally -and $plan.enemy -ne $plan.ally) "Plan $case/$cycle targets invalid."
        Check (@($audit.abilities | Where-Object { $_.hero -eq $plan.hero -and $_.nativeClass -eq ('U'+$plan.family) -and $_.cooldownField -eq $plan.field }).Count -eq 1) "Plan $case/$cycle family/config field is outside audit."
        if ($cycle -eq 1) { $families.Add($plan.family) }
        $activations = Find-Rows 'Activate' $case $cycle
        Check ($activations.Count -eq 1) "Owner activation $case/$cycle missing or duplicate."
        $ownerLog = if ($activations.Count -eq 1) { [int]$activations[0].log } else { -2 }
        if ($ownerLog -ge 0) {
            Check ($activations[0].player -eq $plan.source -and (Near ([double]$activations[0].expected) $expected 0.001)) "Activation $case/$cycle owner/duration differs."
            Check ($ownerLog -eq [int][math]::Floor($case/4) -and [int]$activations[0].mode -eq $(if($ownerLog -eq 0){2}else{3})) "Activation $case/$cycle is not the expected local owner/network mode."
        }
        $begins = Find-Rows 'Authority BEGIN' $case $cycle 0
        Check ($begins.Count -eq 1) "Authority BEGIN $case/$cycle missing or duplicate."
        if ($begins.Count -eq 1) { Check ($begins[0].player -eq $plan.source -and (Near ([double]$begins[0].duration) $expected 0.01)) "Authority duration $case/$cycle invalid." }
        $removals = Find-Rows 'Authority REMOVE' $case $cycle 0
        Check ($removals.Count -eq 1) "Natural effect removal $case/$cycle missing or duplicate."
        if ($removals.Count -eq 1) { Check ($removals[0].player -eq $plan.source -and $removals[0].natural -eq '1' -and $removals[0].predictionRejected -eq '0' -and (Near ([double]$removals[0].expected) $expected 0.001) -and (Near ([double]$removals[0].elapsed) $expected 0.75)) "Natural removal $case/$cycle invalid." }
        foreach ($kind in @('Authority EXPIRE','Owner EXPIRE')) {
            $events = Find-Rows $kind $case $cycle
            Check ($events.Count -eq 1) "$kind $case/$cycle missing or duplicate."
            if ($events.Count -eq 1) {
                $event=$events[0]; $tolerance=if ($kind -eq 'Authority EXPIRE') {0.75} else {1.5}
                Check ($event.player -eq $plan.source -and (Near ([double]$event.expected) $expected 0.001) -and (Near ([double]$event.elapsed) $expected $tolerance) -and $event.inactive -eq '1') "$kind $case/$cycle timing/cleanup invalid."
                Check ($event.log -eq $(if ($kind -eq 'Authority EXPIRE') {0} else {$ownerLog})) "$kind $case/$cycle logged by wrong process."
                if ($kind -eq 'Authority EXPIRE') { Check ($event.lingering -eq '0' -and $event.movementRestored -eq '1') "Authority cleanup $case/$cycle incomplete." }
                else { Check ($event.controls -eq '1') "Ready HUD $case/$cycle missing." }
            }
        }
        $rejected = Find-Rows 'Rejected' $case $cycle
        Check ($rejected.Count -eq 1 -and $rejected[0].log -eq $ownerLog -and $rejected[0].player -eq $plan.source -and $rejected[0].inactive -eq '1') "Cooldown rejection $case/$cycle invalid."
        $hud = @(Find-Rows 'HUD' $case $cycle | Sort-Object sample)
        Check ($hud.Count -eq 2 -and $hud[0].sample -eq '1' -and $hud[1].sample -eq '2') "HUD samples $case/$cycle incomplete."
        foreach ($sample in $hud) {
            Check ($sample.log -eq $ownerLog -and $sample.player -eq $plan.source -and $sample.inactive -eq '1' -and $sample.movementRestored -eq '1' -and
                (Near ([double]$sample.duration) $expected 0.01) -and [double]$sample.remaining -gt 0 -and
                (Near ([double]$sample.countdown) ([double]$sample.remaining) 0.35) -and
                (Near ([double]$sample.percent) ([double]$sample.remaining/$expected) 0.03)) "HUD sample $case/$cycle invalid."
        }
        if ($hud.Count -eq 2) { Check ([double]$hud[1].remaining -lt [double]$hud[0].remaining) "HUD countdown $case/$cycle did not decrease." }
        $acks = @(Find-Rows 'Ack' $case $cycle 0 | Sort-Object stage)
        Check ($acks.Count -eq 3 -and ($acks.stage -join ',') -eq '0,1,2') "Owner acknowledgement chain $case/$cycle incomplete."
        foreach ($ack in $acks) { Check ($ack.player -eq $plan.source) "Acknowledgement $case/$cycle source invalid." }
        $authorityPeers = Find-Rows 'Peer' $case $cycle 0
        $observers = [Collections.Generic.List[string]]::new()
        for ($i=0; $i -lt 4; $i++) {
            $peers = Find-Rows 'Peer' $case $cycle $i
            Check ($peers.Count -eq 4 -and @($peers.player | Sort-Object -Unique).Count -eq 4 -and @($peers.observer | Sort-Object -Unique).Count -eq 1) "Peer rows $case/$cycle log $i incomplete."
            if ($peers.Count) { $observers.Add($peers[0].observer); if ($i -eq $ownerLog) { Check ($peers[0].observer -eq $plan.source) "Owner process $case/$cycle observes a different player." } }
            foreach ($peer in $peers) {
                Check ($peer.source -eq $plan.source -and (Near ([double]$peer.reduction) ([double]$plan.bonus) 0.001) -and [double]$peer.health -gt 0 -and $peer.maxHealth -eq '5000.0000') "Peer values $case/$cycle log $i invalid."
                Check (@($authorityPeers | Where-Object { $_.player -eq $peer.player -and $_.team -eq $peer.team }).Count -eq 1) "Peer team $case/$cycle log $i disagrees with authority."
            }
        }
        Check (@($observers | Sort-Object -Unique).Count -eq 4) "Observers $case/$cycle are not four distinct players."
    }
}
Check (@($families | Sort-Object -Unique).Count -eq 16) 'The plans do not cover 16 distinct ability families.'
Check ((Find-Rows 'Activate').Count -eq 32 -and (Find-Rows 'HUD').Count -eq 64 -and (Find-Rows 'Peer').Count -eq 512) 'Exact activation/HUD/peer totals differ from 32/64/512.'
$authorityPass=Find-Rows 'Authority PASS' -Log 0
Check ($authorityPass.Count -eq 1 -and $authorityPass[0].cases -eq '16' -and $authorityPass[0].cycles -eq '32') 'Missing final authority pass.'
$cleanup = Find-Rows 'Cleanup' -Log 0
Check ($cleanup.Count -eq 1 -and $cleanup[0].players -eq '4' -and $cleanup[0].failed -eq '0') 'Fixture cleanup did not restore all four players.'
for ($i=0; $i -lt 4; $i++) {
    $kind=if($i -eq 0){'Host PASS'}else{'Client PASS'}
    $pass=Find-Rows $kind -Log $i
    Check ($pass.Count -eq 1 -and $pass[0].cases -eq '16' -and $pass[0].cycles -eq '32' -and $pass[0].restoredPlayers -eq '4') "Final pass missing in log $i."
    $teardown=Find-Rows 'Teardown' -Log $i
    Check ($teardown.Count -eq 1 -and $teardown[0].passed -eq '1' -and $teardown[0].failed -eq '0') "Normal probe teardown missing in log $i."
}
$result=[ordered]@{generatedUtc=[datetime]::UtcNow.ToString('o');status=if($problems.Count){'unverified'}else{'passed'};candidate=$transport.candidate;renderedOwnerHero=$transport.renderedOwnerHero;scope='16 families, two owner activations each; independently configured cooldown formula; widget countdown/rejection; natural expiry; inactive/zone/projectile cleanup; Flock movement restoration; replicated upgrade/health setup. Two rendered and two NullRHI processes; rendered-owner scope is the named rotation only. No physical-input, visual, full combat-balance or external-LAN claim.';counts=@{plans=$plans.Count;families=@($families | Sort-Object -Unique).Count;activations=(Find-Rows 'Activate').Count;hudSamples=(Find-Rows 'HUD').Count;peerRows=(Find-Rows 'Peer').Count;problems=$problems.Count};firstCausalError=$problems | Select-Object -First 1;problems=@($problems);logs=@($inventory);rows=@($rows);transport=$TransportResult;artifact=$ArtifactVerification;sourceAudit=$SourceAudit}
$result.verifierSha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
$result.evidenceClass=$transport.evidenceClass
if($transport.evidenceClass -eq 'synthetic-checker-fixture'){
    $result.scope='Synthetic copied-log checker self-test only. No candidate, authored configuration, runtime, rendering or gameplay acceptance.'
    if($result.status -eq 'passed'){$result.status='passed-checker-fixture'}
}
$result.sourceAuditSha256=if(Test-Path -LiteralPath $SourceAudit -PathType Leaf){(Get-FileHash -LiteralPath $SourceAudit -Algorithm SHA256).Hash}else{$null}
$firstFixtureFailure=@($rows | Where-Object kind -eq 'FAIL' | Select-Object -First 1)
if($firstFixtureFailure.Count){$result.firstCausalError="Fixture log $($firstFixtureFailure[0].log): $($firstFixtureFailure[0].reason)"}
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
[pscustomobject]@{status=$result.status;candidate=$result.candidate;counts=$result.counts;firstCausalError=$result.firstCausalError;evidence=$OutputJson} | ConvertTo-Json -Depth 4 -Compress
