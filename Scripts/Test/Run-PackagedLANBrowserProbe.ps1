#Requires -Version 5.1
<##
.SYNOPSIS
    Run an owned four-process packaged LAN browser acceptance fixture.
.DESCRIPTION
    Use a frozen artifact whose compile/package provenance has already been verified.
    Connected covers browser join and shared return/rehost. Rediscovery additionally
    requires complete client disconnect and fresh browser search/entry rejoin.
    All processes render with 200 ms packet lag. Physical input and second-PC LAN
    are separate gates. Output paths must be fresh; the runner never overwrites a run.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-\d{8}-\d{2}$')][string]$Candidate,
    [Parameter(Mandatory)][ValidateSet('Connected','Rediscovery','SelectionEdges')][string]$Variant,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[a-z0-9][a-z0-9-]*$')][string]$Attempt,
    [string[]]$DiagnosticExecCommands = @(),
    [string[]]$HostDiagnosticExecCommands = @(),
    [string[]]$ClientDiagnosticExecCommands = @(),
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
foreach ($diagnostic in @($DiagnosticExecCommands + $HostDiagnosticExecCommands + $ClientDiagnosticExecCommands)) {
    if ([string]::IsNullOrWhiteSpace($diagnostic) -or $diagnostic -match '["\r\n]') {
        throw 'Diagnostic console commands must be nonempty single lines without quotes.'
    }
}
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Binaries/Win64/LyraGame.exe"
$evidenceRoot=if([IO.Path]::IsPathRooted($EvidenceDirectory)){[IO.Path]::GetFullPath($EvidenceDirectory)}else{[IO.Path]::GetFullPath((Join-Path $root $EvidenceDirectory))}
$logPrefix="packaged-$Candidate-$($Variant.ToLowerInvariant())-$Attempt"
$resultPath=Join-Path $evidenceRoot "$logPrefix-result.json"
$hostCommand=if($Variant -eq 'Rediscovery'){'bway.Test.LANBrowserReconnectHost'}else{'bway.Test.LANBrowserHost'}
$clientCommand=if($Variant -eq 'Rediscovery'){'bway.Test.LANBrowserReconnectClient'}else{'bway.Test.LANBrowserClient'}
$renderedClientRole = if ($Variant -eq 'SelectionEdges') { 'client3' } else { 'client1' }
$fixtureScope="Actual LAN browser entry joins and connected return/rehost; variant=$Variant; topology=2 rendered D3D12 (640x360, host+$renderedClientRole) + 2 NullRHI witnesses; 200ms packet lag; optional synthetic round completion; physical and second-PC proof separate"
$gameplayPattern='LogTemp: Display: BwayLANBrowser: Roster lap=2 phase=playing'
$passPattern='LogTemp: Display: BwayLANBrowser: PASS .*laps=2'
if($Variant -eq 'SelectionEdges'){
    $hostCommand='bway.Test.SelectionEdgesHost'
    $clientCommand='bway.Test.SelectionEdgesClient'
    $gameplayPattern='LogTemp: Display: BwaySelectionEdges: Gameplay '
    $passPattern='LogTemp: Display: BwaySelectionEdges: PASS '
    $fixtureScope="Remote selection rejection, repeated locks, selection-time disconnect/browser rejoin, unique team rosters after travel; synthetic 120-second selection hold; topology=2 rendered D3D12 (640x360, host+$renderedClientRole) + 2 NullRHI witnesses at 200ms packet lag; physical and second-PC proof separate"
}
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) {throw 'Missing staged executable.'}
$existing=@(@($resultPath)+@('host','client1','client2','client3' | ForEach-Object {Join-Path $evidenceRoot "$logPrefix-$_.log"}) | Where-Object {Test-Path -LiteralPath $_})
if($existing.Count){throw 'Preserve the existing attempt result or logs; choose a fresh attempt name.'}
if($ValidateOnly){
    [pscustomobject]@{ready=$true;candidate=$Candidate;variant=$Variant;exe=$exe;result=$resultPath;hostCommand=$hostCommand;clientCommand=$clientCommand;diagnosticExecCommands=@($DiagnosticExecCommands);hostDiagnosticExecCommands=@($HostDiagnosticExecCommands);clientDiagnosticExecCommands=@($ClientDiagnosticExecCommands);packetLag=200;scope='Path and command validation only; no process launch or artifact provenance claim'} | ConvertTo-Json -Compress
    return
}
if (@(Get-Process -Name LyraGame,UnrealEditor -ErrorAction SilentlyContinue).Count) {throw 'An editor/game process is already active.'}
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$owned=[Collections.Generic.List[object]]::new()
$records=[Collections.Generic.List[object]]::new()
$observations=@{}
$hostReady=$false; $allConnectedAlive=$false; $runError=$null
$started=[datetime]::UtcNow.ToString('o')

function Start-Game([string]$Role,[string]$Command) {
    $log=Join-Path $evidenceRoot "$logPrefix-$Role.log"
    if (Test-Path -LiteralPath $log) {throw "Preserve existing log $log"}
    $isRenderedPeer = ($Role -eq 'host' -or $Role -eq $renderedClientRole)
    if ($isRenderedPeer) {
        $winX = if ($Role -eq 'host') { 20 } else { 680 }
        $renderArgs = @('-d3d12', "-WinX=$winX", '-WinY=20', '-ResX=640', '-ResY=360', '-ForceRes', '-Windowed', '-MaxFPS=30')
        $winStyle = 'Normal'
    } else {
        $renderArgs = @('-NullRHI', '-MaxFPS=30')
        $winStyle = 'Hidden'
    }
    $arguments=@('/Game/System/FrontEnd/Maps/L_LyraFrontEnd') + $renderArgs + @('-nosound','-nosplash','-unattended','-seconds=700',"-abslog=`"$log`"",'-PktLag=200',"-ExecCmds=`"$Command`"")
    $roleDiagnostics = @($DiagnosticExecCommands)
    if ($Role -eq 'host') { $roleDiagnostics += @($HostDiagnosticExecCommands) }
    elseif ($Role -like 'client*') { $roleDiagnostics += @($ClientDiagnosticExecCommands) }
    foreach ($diagnostic in $roleDiagnostics) { $arguments += "-ExecCmds=`"$diagnostic`"" }
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle $winStyle -PassThru
    $null=$process.Handle
    $owned.Add($process)
    $records.Add([ordered]@{role=$Role;processId=$process.Id;rendered=$isRenderedPeer;rhiRequested=if($isRenderedPeer){'D3D12'}else{'NullRHI'};log=$log;args=$arguments})
    $observations[$Role]=@{offset=0L;text=''}
    return $process
}
function Read-Appended($Record) {
    if (!(Test-Path -LiteralPath $Record.log)) {return ''}
    $state=$observations[$Record.role]
    $stream=[IO.File]::Open($Record.log,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    try {
        if ($stream.Length -lt $state.offset) {throw 'Unexpected log rotation.'}
        $null=$stream.Seek($state.offset,[IO.SeekOrigin]::Begin)
        $reader=[IO.StreamReader]::new($stream)
        $state.text+=$reader.ReadToEnd()
        $state.offset=$stream.Position
    } finally {$stream.Dispose()}
    return $state.text
}
function Wait-Marker([string]$Role,[string]$Pattern,[int]$Timeout=150) {
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while($watch.Elapsed.TotalSeconds -lt $Timeout) {
        foreach($record in $records) {
            $text=Read-Appended $record
            if ($text -match '(?m)^.*LogTemp: Error: BwayLANBrowser: FAIL.*$') {throw $Matches[0]}
            if ($text -match '(?m)^.*(?:Fatal error|Assertion failed|Ensure condition failed).*$') {throw $Matches[0]}
        }
        if ($observations[$Role].text -match $Pattern) {return}
        $id=($records | Where-Object role -eq $Role).processId
        $process=@($owned | Where-Object Id -eq $id)[0]
        if ($process.HasExited) {throw "$Role exited $($process.ExitCode) before $Pattern"}
        Start-Sleep -Milliseconds 250
    }
    throw "$Role timed out waiting for $Pattern"
}
$hostProcess=$null
try {
    $hostProcess=Start-Game 'host' $hostCommand
    Wait-Marker 'host' 'LogNet: .*GameNetDriver.*listening on port 7777' 60
    Wait-Marker 'host' 'LogNet: PktLag set to 200' 5
    $hostReady=$true
    # Launch all clients before waiting on staging. A rendered client can take
    # long enough to load that a sequential launch lets the host travel before
    # the last client has joined, leaving its development probe stuck at Step 2.
    foreach($index in 1..3) {
        $roleCommand=if($Variant -eq 'SelectionEdges'){"$clientCommand $index"}else{$clientCommand}
        $null=Start-Game "client$index" $roleCommand
    }
    foreach($index in 1..3) {
        Wait-Marker "client$index" 'LogTemp: Display: BwayLANBrowser: Staging lap=1 mode=3' 65
        Wait-Marker "client$index" 'LogNet: PktLag set to 200' 5
    }
    foreach($record in $records) {
        Wait-Marker $record.role $gameplayPattern 350
    }
    $allConnectedAlive=$owned.Count -eq 4 -and @($owned | Where-Object HasExited).Count -eq 0
    if (!$allConnectedAlive) {throw 'Four-peer gameplay checkpoint was not live.'}
    foreach($record in $records) {Wait-Marker $record.role $passPattern 35}
    foreach($process in $owned) {
        if($process.Id -ne $hostProcess.Id -and !$process.WaitForExit(30000)) {throw 'Client did not exit normally after fixture pass.'}
    }
} catch {$runError=$_.Exception.Message}
finally {
    foreach($process in $owned) {
        if(!$process.HasExited) {
            try {$null=$process.CloseMainWindow();$null=$process.WaitForExit(3000)} catch {}
            if(!$process.HasExited) {$process.Kill();$null=$process.WaitForExit(5000)}
        }
    }
}
foreach($record in $records) {
    $process=@($owned | Where-Object Id -eq $record.processId)[0]
    $record.exitCode=if($process.HasExited -and $null -ne $process.ExitCode){[int]$process.ExitCode}else{$null}
}
$clients=@($records | Where-Object role -like 'client*')
$clientExitZero=$clients.Count -eq 3 -and @($clients | Where-Object {$null -eq $_.exitCode -or $_.exitCode -ne 0}).Count -eq 0
$result=[ordered]@{candidate=$candidate;scope=$fixtureScope;startedUtc=$started;endedUtc=[datetime]::UtcNow.ToString('o');hostReady=$hostReady;allConnectedAlive=$allConnectedAlive;clientExitZero=$clientExitZero;processes=@($records);hostTeardown='owned host cleanup after client normal exits, or failed fixture cleanup';runError=$runError;status=if($hostReady -and $allConnectedAlive -and $clientExitZero -and !$runError){'passed-transport-scope'}else{'failed-transport-scope'}}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resultPath -Encoding UTF8
[pscustomobject]$result | Select-Object candidate,status,hostReady,allConnectedAlive,clientExitZero,runError | ConvertTo-Json -Compress
if($runError) {exit 1}
