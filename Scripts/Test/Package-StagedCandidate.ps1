<#
.SYNOPSIS
    Parameterized Win64 Development Cook/Stage/Pak runner for a pre-compiled LyraEditor + LyraGame candidate.
.DESCRIPTION
    Enforces single-package gate budget (caps.packageRuns == 1), runs Scripts/Test/Package-Preflight.ps1
    with -RequireAutomationToolLogWrite, freezes source-snapshot-package.json, temporarily aligns
    ShooterCTF UnrealEditor.modules BuildId for UAT cook admission and restores original bytes in finally,
    verifies UAT completion markers and staged LyraGame.exe SHA256 parity, and writes sha256.csv + package-result.json.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-[0-9]{8}-[0-9]{2}$')][string]$Candidate,
    [Parameter(Mandatory)][string]$EditorRecord,
    [Parameter(Mandatory)][string]$GameRecord,
    [Parameter(Mandatory)][string]$GateContractPath,
    [string]$EvidenceRoot,
    [string]$EngineRoot = 'E:\Github\UnrealEngine',
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$GateContractPath = (Resolve-Path -LiteralPath $GateContractPath).Path
$EditorRecord = (Resolve-Path -LiteralPath $EditorRecord).Path
$GameRecord = (Resolve-Path -LiteralPath $GameRecord).Path
$gateDir = Split-Path -Parent $GateContractPath
if (!$EvidenceRoot) {
    $EvidenceRoot = Join-Path $gateDir "package-$Candidate"
}
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
$stagePath = Join-Path $projectRoot "Saved/StagedBuilds/$Candidate"
$resultPath = Join-Path $evidenceRoot 'package-result.json'
$logPath = Join-Path $evidenceRoot 'package-uat.log'

function Get-FileSha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Confirm-NoUnrealProcesses {
    $live = @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
        $_.Name -match '^(LyraGame|UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool|AutomationTool)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
    })
    if ($live.Count -gt 0) {
        throw "Editor/game/build/AutomationTool processes must be closed before packaging: $(($live | ForEach-Object { "$($_.Name):$($_.ProcessId)" }) -join ', ')"
    }
}

if (Test-Path -LiteralPath $stagePath) {
    throw "Stage directory already exists ($stagePath); never overwrite a staged candidate."
}
if (Test-Path -LiteralPath $evidenceRoot) {
    throw "Package evidence directory already exists ($evidenceRoot); preserve attempt history."
}

Confirm-NoUnrealProcesses

$gate = Get-Content -LiteralPath $GateContractPath -Raw | ConvertFrom-Json
$pkgCap = [int]$gate.caps.packageRuns
$pkgUsed = [int]$gate.counters.packageRuns
$nextPkgNum = $pkgUsed + 1
$claimPath = Join-Path $gateDir ("package-attempt-{0:D2}.json" -f $nextPkgNum)
if ($pkgCap -lt 1 -or $pkgUsed -ge $pkgCap -or (Test-Path -LiteralPath $claimPath)) {
    throw "Gate package attempt budget exhausted ($pkgUsed/$pkgCap used, claim=$claimPath)."
}

$head = (& git -C $projectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve current git HEAD.' }

$edRec = Get-Content -LiteralPath $EditorRecord -Raw | ConvertFrom-Json
$gmRec = Get-Content -LiteralPath $GameRecord -Raw | ConvertFrom-Json
foreach ($pair in @(@{ rec = $edRec; target = 'LyraEditor'; path = $EditorRecord }, @{ rec = $gmRec; target = 'LyraGame'; path = $GameRecord })) {
    $r = $pair.rec
    if ($r.status -ne 'passed' -or $r.exitCode -ne 0 -or $r.target -ne $pair.target -or $r.sourceHead -ne $head -or @($r.failureSignals).Count -gt 0) {
        throw "Compile record $($pair.path) is not a clean passed $($pair.target) build on HEAD $head."
    }
}

$editorDll = Join-Path $projectRoot 'Plugins/GameFeatures/BreakawayCore/Binaries/Win64/UnrealEditor-BreakawayCoreRuntime.dll'
$gameExe = Join-Path $projectRoot 'Binaries/Win64/LyraGame.exe'
$requiredPaths = @(
    (Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'),
    (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'),
    (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.modules'),
    (Join-Path $projectRoot 'Binaries/Win64/UnrealEditor.modules'),
    (Join-Path $projectRoot 'Plugins/GameFeatures/ShooterCTF/Binaries/Win64/UnrealEditor.modules'),
    $editorDll,
    $gameExe
)
foreach ($req in $requiredPaths) {
    if (!(Test-Path -LiteralPath $req -PathType Leaf)) {
        throw "Missing package prerequisite: $req"
    }
}

$engineBuildId = (Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.modules') -Raw | ConvertFrom-Json).BuildId
$projectBuildId = (Get-Content -LiteralPath (Join-Path $projectRoot 'Binaries/Win64/UnrealEditor.modules') -Raw | ConvertFrom-Json).BuildId
if (!$engineBuildId -or $projectBuildId -ne $engineBuildId) {
    throw "Engine/project editor BuildId mismatch ($engineBuildId vs $projectBuildId)."
}

$freeBytes = ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($projectRoot))).AvailableFreeSpace
if ($freeBytes -lt 10GB) {
    throw 'At least 10 GiB free space is required before packaging.'
}

$gameHash = Get-FileSha256 $gameExe
$editorDllHash = Get-FileSha256 $editorDll

if ($ValidateOnly) {
    [ordered]@{
        status            = 'validated-read-only-inputs'
        candidate         = $Candidate
        stage             = $stagePath
        evidenceRoot      = $evidenceRoot
        sourceHead        = $head
        lyraGameSha256    = $gameHash
        editorDllSha256   = $editorDllHash
        packageRunsUsed   = [int]$gate.counters.packageRuns
        packageRunsCap    = [int]$gate.caps.packageRuns
        freeGiB           = [math]::Round($freeBytes / 1GB, 2)
    } | ConvertTo-Json -Depth 5
    return
}

$null = New-Item -ItemType Directory -Path $evidenceRoot -Force
$preflightPath = Join-Path $evidenceRoot 'package-preflight.json'
& (Join-Path $PSScriptRoot 'Package-Preflight.ps1') `
    -Candidate $Candidate `
    -CompiledTargetRecord $GameRecord `
    -RequireAutomationToolLogWrite `
    -OutputJson $preflightPath | Out-Null
$preflight = Get-Content -LiteralPath $preflightPath -Raw | ConvertFrom-Json
if (-not $preflight.ready) {
    throw "Package-Preflight.ps1 reported ready=false; see $preflightPath"
}

$snapshotPrefix = Join-Path $evidenceRoot 'source-snapshot-package'
& (Join-Path $PSScriptRoot 'New-SourceSnapshot.ps1') -OutputPrefix $snapshotPrefix | Out-Null
$frozenSnapshotPath = "$snapshotPrefix.json"
$snapshot = Get-Content -LiteralPath $frozenSnapshotPath -Raw | ConvertFrom-Json

# Reserve package attempt in gate-contract.json + atomic claim file
$gate = Get-Content -LiteralPath $GateContractPath -Raw | ConvertFrom-Json
$gate.candidate = $Candidate
$gate.counters.packageRuns = $nextPkgNum
$gate | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $GateContractPath -Encoding UTF8

$claimStream = [IO.File]::Open($claimPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try {
    $claimObj = [ordered]@{
        schemaVersion      = 1
        gateId             = $gate.gateId
        candidate          = $Candidate
        packageRunNumber   = $nextPkgNum
        sourceHead         = $head
        sourceIdentity     = $snapshot.sourceIdentity
        lyraGameSha256     = $gameHash
        editorDllSha256    = $editorDllHash
        stage              = $stagePath
        evidenceRoot       = $evidenceRoot
        createdUtc         = [datetime]::UtcNow.ToString('o')
        runnerPid          = $PID
    }
    $claimBytes = [Text.Encoding]::UTF8.GetBytes(($claimObj | ConvertTo-Json -Depth 6))
    $claimStream.Write($claimBytes, 0, $claimBytes.Length)
    $claimStream.Flush($true)
} finally {
    $claimStream.Dispose()
}

$modulePath = Join-Path $projectRoot 'Plugins/GameFeatures/ShooterCTF/Binaries/Win64/UnrealEditor.modules'
$moduleBytes = [IO.File]::ReadAllBytes($modulePath)
$moduleBackup = Join-Path $evidenceRoot 'ShooterCTF.modules.before'
[IO.File]::WriteAllBytes($moduleBackup, $moduleBytes)

$result = [ordered]@{
    status               = 'running'
    candidate            = $Candidate
    sourceHead           = $head
    sourceIdentity       = $snapshot.sourceIdentity
    sourceSnapshot       = $frozenSnapshotPath
    sourceSnapshotSha256 = (Get-FileSha256 $frozenSnapshotPath)
    compileRecords       = @(
        [ordered]@{ target = 'LyraEditor'; path = $EditorRecord; sha256 = (Get-FileSha256 $EditorRecord); binaryPath = $editorDll; binarySha256 = $editorDllHash },
        [ordered]@{ target = 'LyraGame';   path = $GameRecord;   sha256 = (Get-FileSha256 $GameRecord);   binaryPath = $gameExe;   binarySha256 = $gameHash }
    )
    producer             = $PSCommandPath
    producerSha256       = (Get-FileSha256 $PSCommandPath)
    startedUtc           = [datetime]::UtcNow.ToString('o')
    runnerPid            = $PID
    stage                = $stagePath
    log                  = $logPath
    gateContract         = $GateContractPath
    packageRunNumber     = $nextPkgNum
    packageAttemptClaim  = $claimPath
    exitCode             = $null
    firstCausalError     = $null
    moduleRestored       = $false
    executableSha256     = $null
    artifactCount        = 0
    hashManifest         = $null
    scope                = 'Win64 Development Cook/Stage/Pak only; multiplayer runtime acceptance is separate.'
}

function Save-PackageResult {
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath "$resultPath.partial" -Encoding UTF8
    Move-Item -LiteralPath "$resultPath.partial" -Destination $resultPath -Force
}
Save-PackageResult

try {
    $moduleObj = Get-Content -LiteralPath $modulePath -Raw | ConvertFrom-Json
    if ($moduleObj.BuildId -ne $engineBuildId) {
        $moduleObj.BuildId = $engineBuildId
        $moduleObj | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $modulePath -Encoding UTF8
    }

    Confirm-NoUnrealProcesses
    if ((Get-FileSha256 $gameExe) -ne $gameHash) {
        throw 'Compiled Binaries/Win64/LyraGame.exe changed before UAT launch.'
    }

    $runUat = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
    $uproject = Join-Path $projectRoot 'ProjectB.uproject'
    & $runUat -nocompileuat "-ScriptsForProject=$uproject" BuildCookRun `
        "-project=$uproject" -target=LyraGame -platform=Win64 -clientconfig=Development `
        -skipbuild -cook -stage -pak -utf8output -unattended `
        "-stagingdirectory=$stagePath" *> $logPath
    $result.exitCode = $LASTEXITCODE
    if ($result.exitCode -ne 0) {
        throw "BuildCookRun exited with code $($result.exitCode)."
    }

    $logText = [IO.File]::ReadAllText($logPath)
    foreach ($marker in @('COOK COMMAND COMPLETED', 'STAGE COMMAND COMPLETED', 'BUILD SUCCESSFUL', 'AutomationTool exiting with ExitCode=0')) {
        if (!$logText.Contains($marker)) {
            throw "Missing expected UAT marker: $marker"
        }
    }
    if ($logText -match '(?im)BUILD FAILED|Fatal error:|Unhandled Exception|Ensure condition failed') {
        throw 'Explicit fatal/failure signal found in package-uat.log.'
    }

    $stagedExe = Join-Path $stagePath 'Windows/ProjectB/Binaries/Win64/LyraGame.exe'
    if (!(Test-Path -LiteralPath $stagedExe -PathType Leaf)) {
        throw "Staged executable missing: $stagedExe"
    }
    if ((Get-FileSha256 $stagedExe) -ne $gameHash) {
        throw 'Staged LyraGame.exe SHA256 differs from compiled Binaries/Win64/LyraGame.exe.'
    }

    $artifacts = @(Get-ChildItem -LiteralPath $stagePath -File -Recurse | ForEach-Object {
        [pscustomobject]@{
            path   = $_.FullName.Substring($stagePath.Length + 1)
            bytes  = $_.Length
            sha256 = (Get-FileSha256 $_.FullName)
        }
    })
    $artifacts | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'package-artifacts.json') -Encoding UTF8
    $hashManifest = Join-Path $stagePath 'sha256.csv'
    $artifacts | Export-Csv -LiteralPath $hashManifest -NoTypeInformation -Encoding UTF8

    $result.hashManifest = $hashManifest
    $result.executableSha256 = $gameHash
    $result.artifactCount = $artifacts.Count
    $result.status = 'passed'
} catch {
    $result.status = 'failed'
    $result.firstCausalError = $_.Exception.Message
} finally {
    try {
        [IO.File]::WriteAllBytes($modulePath, $moduleBytes)
        $result.moduleRestored = ((Get-FileSha256 $modulePath) -eq (Get-FileSha256 $moduleBackup))
        if (-not $result.moduleRestored) {
            throw 'ShooterCTF UnrealEditor.modules bytes were not restored.'
        }
    } catch {
        $result.status = 'failed'
        if (-not $result.firstCausalError) { $result.firstCausalError = $_.Exception.Message }
    }
    $result.finishedUtc = [datetime]::UtcNow.ToString('o')
    Save-PackageResult
}

([pscustomobject]$result) | Select-Object status, candidate, exitCode, executableSha256, artifactCount, moduleRestored, firstCausalError | ConvertTo-Json -Compress
if ($result.status -ne 'passed') { exit 1 }
