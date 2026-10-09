[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HostLog,
    [Parameter(Mandatory)][string]$ClientLog,
    [Parameter(Mandatory)][string]$Candidate,
    [Parameter(Mandatory)][string]$OutputJson,
    [string]$SourceSnapshotPath
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Resolve-LogPath {
    param([Parameter(Mandatory)][string]$Path)
    $resolved = if ([System.IO.Path]::IsPathRooted($Path)) {
        $Path
    } else {
        Join-Path $root $Path
    }
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "Packaged shop log not found: $resolved"
    }
    return (Resolve-Path -LiteralPath $resolved).Path
}

function Get-Matches {
    param([string[]]$Lines, [Parameter(Mandatory)][string]$Pattern)
    return @($Lines | Where-Object { $_ -match $Pattern })
}

$hostPath = Resolve-LogPath -Path $HostLog
$clientPath = Resolve-LogPath -Path $ClientLog
$sourceSnapshot = $null
if ($SourceSnapshotPath) {
    $snapshotPath = Resolve-LogPath -Path $SourceSnapshotPath
    $sourceSnapshot = Get-Content -LiteralPath $snapshotPath -Raw | ConvertFrom-Json
}
$hostLines = @(Get-Content -LiteralPath $hostPath)
$clientLines = @(Get-Content -LiteralPath $clientPath)
$hostErrors = @(Get-Matches -Lines $hostLines -Pattern 'LogTemp: Error:|LogWindows: Error:')
$clientErrors = @(Get-Matches -Lines $clientLines -Pattern 'LogTemp: Error:|LogWindows: Error:')
$hostCritical = @(Get-Matches -Lines $hostLines -Pattern 'Critical error|Fatal error|Unhandled Exception')
$clientCritical = @(Get-Matches -Lines $clientLines -Pattern 'Critical error|Fatal error|Unhandled Exception')
$hostUnknown = @(Get-Matches -Lines $hostLines -Pattern 'Unknown error|UnknownError')
$clientUnknown = @(Get-Matches -Lines $clientLines -Pattern 'Unknown error|UnknownError')
$hostReady = @(Get-Matches -Lines $hostLines -Pattern 'GameNetDriver.*Listen|Browse.*listen|BwayMatchFlow: Resolved').Count -gt 0
$seeded = @(Get-Matches -Lines $hostLines -Pattern 'LogTemp: .*BwayShopTest: Seeded remote player gold').Count -gt 0
$hostSeedLines = @(Get-Matches -Lines $hostLines -Pattern 'LogTemp: .*BwayShopTest: Seeded remote player gold')
$invoked = @(Get-Matches -Lines $clientLines -Pattern 'LogTemp: .*BwayShopTest: Remote widget purchase invoked')
$states = @(Get-Matches -Lines $clientLines -Pattern 'LogTemp: .*BwayShopTest: Remote purchase state')
$passes = @(Get-Matches -Lines $clientLines -Pattern 'LogTemp: .*BwayShopTest: Remote purchase PASS')
$testExits = @(Get-Matches -Lines $clientLines -Pattern 'LogExit: .*TestExit: BwayShopTest: Remote purchase PASS')
$hostSeedMarker = if ($hostSeedLines.Count -gt 0) { [string]$hostSeedLines[0] } else { $null }
$invokedMarker = if ($invoked.Count -gt 0) { [string]$invoked[-1] } else { $null }
$stateMarker = if ($states.Count -gt 0) { [string]$states[-1] } else { $null }
$passMarker = if ($passes.Count -gt 0) { [string]$passes[-1] } else { $null }
$testExitMarker = if ($testExits.Count -gt 0) { [string]$testExits[-1] } else { $null }

$purchase = $null
if ($states.Count -gt 0) {
    $stateMatch = [regex]::Match($states[-1], 'id=(?<id>\S+) rank=(?<rank>-?\d+) beforeGold=(?<before>-?[\d.]+) gold=(?<gold>-?[\d.]+) cost=(?<cost>-?\d+) expectedGold=(?<expected>-?[\d.]+) walletDeltaOk=(?<wallet>\d)')
    if ($stateMatch.Success) {
        $purchase = [ordered]@{
            id = $stateMatch.Groups['id'].Value
            rank = [int]$stateMatch.Groups['rank'].Value
            beforeGold = [double]$stateMatch.Groups['before'].Value
            gold = [double]$stateMatch.Groups['gold'].Value
            cost = [int]$stateMatch.Groups['cost'].Value
            expectedGold = [double]$stateMatch.Groups['expected'].Value
            walletDeltaOk = $stateMatch.Groups['wallet'].Value -eq '1'
        }
    }
}

$status = if ($hostReady -and $seeded -and $passes.Count -gt 0 -and $testExits.Count -gt 0 -and $purchase -and $purchase.rank -ge 1 -and $purchase.walletDeltaOk -and $hostErrors.Count -eq 0 -and $clientErrors.Count -eq 0 -and $hostCritical.Count -eq 0 -and $clientCritical.Count -eq 0 -and $hostUnknown.Count -eq 0 -and $clientUnknown.Count -eq 0) {
    'passed-scope-limited'
} else {
    'failed-scope'
}

$record = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    candidate = $Candidate
    status = $status
    scope = 'Packaged listen-server client-originated upgrade purchase through the production CommonUI widget/RPC path'
    hostLog = $hostPath
    clientLog = $clientPath
    sourceSnapshot = if ($sourceSnapshot) {
        [ordered]@{ path = $snapshotPath; sourceIdentity = $sourceSnapshot.sourceIdentity; sourceHead = $sourceSnapshot.sourceHead; dirtyFileCount = $sourceSnapshot.dirtyFileCount }
    } else { $null }
    hostReady = $hostReady
    remoteWalletSeeded = $seeded
    clientPurchaseInvoked = $invoked.Count -gt 0
    clientPass = $passes.Count -gt 0
    clientTestExit = $testExits.Count -gt 0
    purchase = $purchase
    markerCounts = [ordered]@{ invoked = $invoked.Count; state = $states.Count; pass = $passes.Count; testExit = $testExits.Count }
    logChecks = [ordered]@{
        hostErrorLines = $hostErrors.Count
        clientErrorLines = $clientErrors.Count
        hostUnknownErrorLines = $hostUnknown.Count
        clientUnknownErrorLines = $clientUnknown.Count
        hostCriticalLines = $hostCritical.Count
        clientCriticalLines = $clientCritical.Count
    }
    markerEvidence = [ordered]@{
        hostSeed = $hostSeedMarker
        invoked = $invokedMarker
        state = $stateMarker
        pass = $passMarker
        testExit = $testExitMarker
    }
    quality = 'Structured log brief only; physical input, normal selection, buildable UI, second-PC LAN, and clean-device deployment remain separate gates.'
}

$outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
[ordered]@{
    status = $record.status
    candidate = $record.candidate
    hostReady = $record.hostReady
    clientPass = $record.clientPass
    purchaseId = if ($record.purchase) { $record.purchase.id } else { $null }
    purchaseRank = if ($record.purchase) { $record.purchase.rank } else { $null }
    clientErrorLines = $record.logChecks.clientErrorLines
    outputJson = $outputPath
} | ConvertTo-Json -Compress
