[CmdletBinding()]
param([Parameter(Mandatory)][string]$TransportResult,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(!$root.StartsWith((Join-Path $project 'Saved')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Controls must remain under project Saved.'}
if(Test-Path -LiteralPath $root){throw 'Preserve previous controls.'}
$sourceRun=(Resolve-Path -LiteralPath $TransportResult).Path
$original=Get-Content -LiteralPath $sourceRun -Raw | ConvertFrom-Json
$sourceArtifact=$original.artifactVerification
$sourceAudit=$original.configuration
$checker=Join-Path $PSScriptRoot 'Verify-AllKitCooldownTransport.ps1'
function Invoke-Check($run,[string]$transport,[string]$artifact,[string]$audit,[string]$output) {
    & $checker -HostLog ($run.processes | Where-Object role -eq 'host').log -ClientLogs @($run.processes | Where-Object role -like 'client*' | Sort-Object role | ForEach-Object {$_.log}) -TransportResult $transport -ArtifactVerification $artifact -SourceAudit $audit -OutputJson $output | Out-Null
    return Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
}
New-Item -ItemType Directory -Path $root | Out-Null
$baseline=Invoke-Check $original $sourceRun $sourceArtifact $sourceAudit (Join-Path $root 'baseline.json')
if($baseline.status -notin @('passed','passed-checker-fixture')) { throw "Baseline must pass its declared scope before mutation controls: $($baseline.firstCausalError)" }
$tests=@(
    @{name='missing-plan';role='host';pattern='(?m)^.*LogTemp: Display: BwayAllKitCooldown: Plan case=0 cycle=1 .*(?:\r?\n|$)';replace=''},
    @{name='wrong-family';role='host';pattern='(BwayAllKitCooldown: Plan case=0 cycle=1 .*family=)\S+';replace='${1}WrongFamily'},
    @{name='wrong-duration';role='host';pattern='(BwayAllKitCooldown: Authority BEGIN case=0 cycle=1 .*duration=)[\d.]+';replace='${1}999.0000'},
    @{name='early-expiry';role='host';pattern='(BwayAllKitCooldown: Authority EXPIRE case=0 cycle=1 .*elapsed=)[\d.]+';replace='${1}0.1000'},
    @{name='premature-removal';role='host';pattern='(BwayAllKitCooldown: Authority REMOVE case=0 cycle=1 .*natural=)1';replace='${1}0'},
    @{name='lingering-actor';role='host';pattern='(BwayAllKitCooldown: Authority EXPIRE case=0 cycle=1 .*lingering=)0';replace='${1}1'},
    @{name='active-after-expiry';role='host';pattern='(BwayAllKitCooldown: Authority EXPIRE case=0 cycle=1 .*inactive=)1';replace='${1}0'},
    @{name='owner-identity';role='client1';pattern='(BwayAllKitCooldown: Activate case=4 cycle=1 player=)\d+';replace='${1}99999'},
    @{name='missing-hud';role='host';pattern='(?m)^.*LogTemp: Display: BwayAllKitCooldown: HUD case=0 cycle=1 .*sample=2 .*(?:\r?\n|$)';replace=''},
    @{name='wrong-hud-percent';role='host';pattern='(BwayAllKitCooldown: HUD case=0 cycle=1 .*percent=)[\d.]+';replace='${1}9.0000'},
    @{name='wrong-hud-countdown';role='host';pattern='(BwayAllKitCooldown: HUD case=0 cycle=1 .*countdown=)[\d.]+';replace='${1}999.0000'},
    @{name='missing-rejection';role='host';pattern='(?m)^.*LogTemp: Display: BwayAllKitCooldown: Rejected case=0 cycle=1 .*(?:\r?\n|$)';replace=''},
    @{name='flock-restoration';role='client3';pattern='(BwayAllKitCooldown: HUD case=15 cycle=1 .*movementRestored=)1';replace='${1}0'},
    @{name='wrong-peer-team';role='client1';pattern='(BwayAllKitCooldown: Peer case=0 cycle=1 .*team=)\d+';replace='${1}999'},
    @{name='wrong-peer-bonus';role='client1';pattern='(BwayAllKitCooldown: Peer case=0 cycle=1 .*reduction=)[\d.]+';replace='${1}0.9999'},
    @{name='wrong-ack';role='host';pattern='(BwayAllKitCooldown: Ack case=0 cycle=1 .*stage=)1';replace='${1}9'},
    @{name='incomplete-cleanup';role='host';pattern='(BwayAllKitCooldown: Cleanup players=)4';replace='${1}3'},
    @{name='missing-teardown';role='client2';pattern='(?m)^.*LogTemp: Display: BwayAllKitCooldown: Teardown .*(?:\r?\n|$)';replace=''},
    @{name='wrong-cycle-total';role='host';pattern='(BwayAllKitCooldown: Authority PASS cases=16 cycles=)32';replace='${1}31'},
    @{name='missing-lag';role='client2';pattern="(?m)^.*LogNet: PktLag set to $($original.expectedLagMs)\b.*(?:\r?\n|$)";replace='';all=$true},
    @{name='missing-renderer';role=$original.renderedOwnerRole;pattern='(?m)^.*LogRHI: Loading RHI module D3D12RHI.*(?:\r?\n|$)';replace='';all=$true},
    @{name='runtime-error';role='host';pattern='(LogTemp: Display: BwayAllKitCooldown: Authority PASS)';replace='LogTemp: Error: injected verification control';},
    @{name='null-client-exit';json='exit'},
    @{name='no-four-live';json='alive'},
    @{name='bad-artifact';json='artifact'},
    @{name='hash-mismatch';json='hash'},
    @{name='bad-source-audit';json='audit'},
    @{name='nullrhi';json='nullrhi'},
    @{name='wrong-config-duration';json='config'},
    @{name='wrong-config-bonus';json='bonus'},
    @{name='missing-compile-binding';json='compile'},
    @{name='missing-HEAD-binding';json='binding'},
    @{name='missing-UDP-readiness';json='udp'},
    @{name='reused-user-directory';json='userdir'},
    @{name='missing-settings-restoration';json='settings'},
    @{name='wrong-viewport';role=$original.renderedOwnerRole;pattern='(LogViewport:.*Scene viewport resized to )640x360';replace='${1}800x450';all=$true},
    @{name='missing-cap-query';role='host';pattern='(?m)^.*\bt\.MaxFPS = "30(?:\.0+)?"\s+LastSetBy:.*(?:\r?\n|$)';replace='';all=$true},
    @{name='wrong-owner-mode';role='client1';pattern='(BwayAllKitCooldown: Activate case=4 cycle=1 .*mode=)3';replace='${1}2'}
)
$results=@()
foreach($test in $tests) {
    $folder=Join-Path $root $test.name
    New-Item -ItemType Directory -Path $folder | Out-Null
    $run=Get-Content -LiteralPath $sourceRun -Raw | ConvertFrom-Json
    foreach($process in $run.processes) {
        $dest=Join-Path $folder "$($process.role).log"
        Copy-Item -LiteralPath $process.log -Destination $dest
        $process.log=$dest
    }
    $artifact=Get-Content -LiteralPath $sourceArtifact -Raw | ConvertFrom-Json
    $audit=Get-Content -LiteralPath $sourceAudit -Raw | ConvertFrom-Json
    if($test.role) {
        $file=($run.processes | Where-Object role -eq $test.role).log
        $text=[IO.File]::ReadAllText($file)
        $regex=[regex]::new($test.pattern)
        if(!$regex.IsMatch($text)) {throw "Control $($test.name) did not match actual evidence."}
        $mutated=if($test.all) {$regex.Replace($text,$test.replace)} else {$regex.Replace($text,$test.replace,1)}
        [IO.File]::WriteAllText($file,$mutated)
    }
    switch($test.json) {
        'exit' {($run.processes | Where-Object role -eq 'client1').exitCode=$null}
        'alive' {$run.allConnectedAlive=$false}
        'artifact' {$artifact.passed=$false}
        'hash' {$artifact.stagedHash='WRONG'}
        'audit' {$audit.abilities[0].cooldownField='WrongField'}
        'nullrhi' {($run.processes | Where-Object role -eq $original.renderedOwnerRole).args+= '-nullrhi'}
        'config' {$audit.abilities[0].baseDuration=999}
        'bonus' {$audit.cooldownReduction=0.49}
        'compile' {$artifact.compileRecords=@($artifact.compileRecords | Where-Object target -ne 'LyraGame')}
        'binding' {$artifact.bindings=@($artifact.bindings | Where-Object path -NotMatch 'BwayAllKitCooldownProbe\.cpp$')}
        'udp' {$run.udpReadiness=$null}
        'userdir' {$run.processes[1].userDir=$run.processes[0].userDir}
        'settings' {$run.settingsRestored=$false}
    }
    $runPath=Join-Path $folder 'transport.json'; $artifactPath=Join-Path $folder 'artifact.json'; $auditPath=Join-Path $folder 'audit.json'
    # Rebind copied bytes so the intended semantic check, rather than a generic
    # digest mismatch, rejects each corruption. Originals remain untouched.
    foreach($process in $run.processes){$process.logSHA256=(Get-FileHash -LiteralPath $process.log).Hash}
    $audit | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $auditPath -Encoding UTF8
    $run.configurationSHA256=(Get-FileHash -LiteralPath $auditPath).Hash
    $artifact.configurationSHA256=$run.configurationSHA256
    $artifact | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $artifactPath -Encoding UTF8
    $run.artifactVerificationSHA256=(Get-FileHash -LiteralPath $artifactPath).Hash
    $run | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $runPath -Encoding UTF8
    $result=Invoke-Check $run $runPath $artifactPath $auditPath (Join-Path $folder 'brief.json')
    $results+=[pscustomobject]@{name=$test.name;rejected=$result.status -eq 'unverified';firstCausalError=$result.firstCausalError}
}
$record=[ordered]@{candidate=$original.candidate;scope='Copied-evidence checker controls; never gameplay evidence';originalControls=28;addedControls=10;baseline=$baseline.status;count=$results.Count;passed=$results.Count -eq 38 -and @($results | Where-Object {!$_.rejected}).Count -eq 0;checks=$results;generatedUtc=[datetime]::UtcNow.ToString('o')}
$record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'controls.json') -Encoding UTF8
[pscustomobject]$record | Select-Object candidate,baseline,count,passed | ConvertTo-Json -Compress
if(!$record.passed) {throw 'An evidence mutation was incorrectly accepted.'}
