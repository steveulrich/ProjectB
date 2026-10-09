[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogFile,
    [string]$OutputJson,
    [switch]$IncludeSamples
)

$ErrorActionPreference = 'Stop'
$resolvedLog = (Resolve-Path -LiteralPath $LogFile -ErrorAction Stop).Path
$lines = @(Get-Content -LiteralPath $resolvedLog)
$markerLines = @($lines | Where-Object { $_ -match 'BwayBuildableTest:' })
$placementLines = @($lines | Where-Object { $_ -match 'LogBwayPlaceBuildable:' })

function Get-LastMatch {
    param(
        [Parameter(Mandatory)][string]$Pattern,
        [string[]]$Source = $markerLines
    )
    for ($index = $Source.Count - 1; $index -ge 0; $index--) {
        $match = [regex]::Match([string]$Source[$index], $Pattern)
        if ($match.Success) {
            return [pscustomobject]@{ Line = [string]$Source[$index]; Match = $match }
        }
    }
    return $null
}

function Get-IntGroup {
    param($Match, [string]$Name)
    if ($null -eq $Match -or -not $Match.Groups[$Name].Success) { return $null }
    return [int]$Match.Groups[$Name].Value
}

$spawn = Get-LastMatch 'SpawnResult playerId=(?<playerId>\d+) class=(?<class>\S+) spawned=(?<spawned>\d+) countBefore=(?<countBefore>\d+) countAfter=(?<countAfter>\d+) hasPlacedBefore=(?<hasPlacedBefore>\d+) hasPlacedAfter=(?<hasPlacedAfter>\d+) mesh=(?<mesh>\S+) replicated=(?<replicated>\d+) netLoad=(?<netLoad>\d+) onlyOwner=(?<onlyOwner>\d+) ownerRelevancy=(?<ownerRelevancy>\d+) dormancy=(?<dormancy>\d+)'
$quota = Get-LastMatch 'QuotaResult playerId=(?<playerId>\d+) canPlaceAgain=(?<canPlaceAgain>\d+) reason=(?<reason>.*)$'
$cancel = Get-LastMatch 'AbilityResult mode=cancel active=(?<active>\d+) placementTag=(?<placementTag>\d+) hasPlaced=(?<hasPlaced>\d+)'
$confirm = Get-LastMatch 'AbilityResult mode=confirm active=(?<active>\d+) placementTag=(?<placementTag>\d+) hasPlaced=(?<hasPlaced>\d+)'
$malformedMatches = @($markerLines | ForEach-Object {
    $match = [regex]::Match([string]$_, 'MalformedResult kind=(?<kind>empty|wrong|multi) active=(?<active>\d+) placementTag=(?<placementTag>\d+) hasPlaced=(?<hasPlaced>\d+)')
    if ($match.Success) { [pscustomobject]@{ Line = [string]$_; Match = $match } }
})
$reset = Get-LastMatch 'ResetRoundResult playerId=(?<playerId>\d+) countBefore=(?<countBefore>\d+) countAfter=(?<countAfter>\d+) hasPlacedAfter=(?<hasPlacedAfter>\d+)'
$spawnConvergence = Get-LastMatch 'Spawn PeerConvergence worlds=(?<worlds>\d+) worldsWithBuildable=(?<worldsWithBuildable>\d+) playerId=(?<playerId>\d+) pass=(?<pass>\d+)'
$resetConvergence = Get-LastMatch 'ResetRound PeerConvergence worlds=(?<worlds>\d+) worldsWithBuildable=(?<worldsWithBuildable>\d+) playerId=(?<playerId>\d+) pass=(?<pass>\d+)'
$replicatedSubmit = Get-LastMatch 'ReplicatedRejectSubmit playerId=(?<playerId>\d+) countBefore=(?<countBefore>\d+) lagMs=(?<lagMs>\d+)'
$replicatedResult = Get-LastMatch 'ReplicatedRejectResult playerId=(?<playerId>\d+) countBefore=(?<countBefore>\d+) countAfter=(?<countAfter>\d+) spawned=(?<spawned>\d+) hasPlacedAfter=(?<hasPlacedAfter>\d+) authorityAbilityActive=(?<authorityAbilityActive>\d+)'
$replicatedConfirmSubmit = Get-LastMatch 'ReplicatedConfirmSubmit playerId=(?<playerId>\d+) countBefore=(?<countBefore>\d+) targetBlocking=(?<targetBlocking>\d+)'
$replicatedConfirmResult = Get-LastMatch 'ReplicatedConfirmResult playerId=(?<playerId>\d+) countBefore=(?<countBefore>\d+) countAfter=(?<countAfter>\d+) spawned=(?<spawned>\d+) hasPlacedAfter=(?<hasPlacedAfter>\d+) authorityAbilityActive=(?<authorityAbilityActive>\d+)'
$replicatedConfirmCleanup = Get-LastMatch 'ReplicatedConfirmCleanup playerId=(?<playerId>\d+) countAfterCleanup=(?<countAfterCleanup>\d+)'
$authorityCallback = Get-LastMatch 'PlaceBuildable OnTargetDataValid Valid=(?<valid>\d+) authority=1 entries=(?<entries>\d+)' -Source $placementLines
$authorityTargetDataResult = Get-LastMatch 'PlaceBuildable TargetDataResult authority=1 accepted=(?<accepted>\d+)' -Source $placementLines
$peerSamples = @($markerLines | Where-Object { $_ -match 'PeerSample ' })
$fixtureErrors = @($lines | Where-Object { $_ -match 'BwayBuildableTest: .*' -and $_ -match 'LogTemp: (?:Error|Warning):' })

$spawnRecord = if ($spawn) {
    [ordered]@{
        playerId = Get-IntGroup $spawn.Match 'playerId'
        class = $spawn.Match.Groups['class'].Value
        spawned = Get-IntGroup $spawn.Match 'spawned'
        countBefore = Get-IntGroup $spawn.Match 'countBefore'
        countAfter = Get-IntGroup $spawn.Match 'countAfter'
        hasPlacedBefore = Get-IntGroup $spawn.Match 'hasPlacedBefore'
        hasPlacedAfter = Get-IntGroup $spawn.Match 'hasPlacedAfter'
        mesh = $spawn.Match.Groups['mesh'].Value
        replicated = Get-IntGroup $spawn.Match 'replicated'
        netLoad = Get-IntGroup $spawn.Match 'netLoad'
        onlyOwner = Get-IntGroup $spawn.Match 'onlyOwner'
        ownerRelevancy = Get-IntGroup $spawn.Match 'ownerRelevancy'
        dormancy = Get-IntGroup $spawn.Match 'dormancy'
    }
} else { $null }

$quotaRecord = if ($quota) {
    [ordered]@{
        playerId = Get-IntGroup $quota.Match 'playerId'
        canPlaceAgain = Get-IntGroup $quota.Match 'canPlaceAgain'
        reason = $quota.Match.Groups['reason'].Value.Trim()
    }
} else { $null }

$cancelRecord = if ($cancel) {
    [ordered]@{
        active = Get-IntGroup $cancel.Match 'active'
        placementTag = Get-IntGroup $cancel.Match 'placementTag'
        hasPlaced = Get-IntGroup $cancel.Match 'hasPlaced'
    }
} else { $null }

$confirmRecord = if ($confirm) {
    [ordered]@{
        active = Get-IntGroup $confirm.Match 'active'
        placementTag = Get-IntGroup $confirm.Match 'placementTag'
        hasPlaced = Get-IntGroup $confirm.Match 'hasPlaced'
    }
} else { $null }

$resetRecord = if ($reset) {
    [ordered]@{
        playerId = Get-IntGroup $reset.Match 'playerId'
        countBefore = Get-IntGroup $reset.Match 'countBefore'
        countAfter = Get-IntGroup $reset.Match 'countAfter'
        hasPlacedAfter = Get-IntGroup $reset.Match 'hasPlacedAfter'
    }
} else { $null }

function Convert-Convergence {
    param($Record)
    if ($null -eq $Record) { return $null }
    return [ordered]@{
        worlds = Get-IntGroup $Record.Match 'worlds'
        worldsWithBuildable = Get-IntGroup $Record.Match 'worldsWithBuildable'
        playerId = Get-IntGroup $Record.Match 'playerId'
        pass = Get-IntGroup $Record.Match 'pass'
    }
}

$spawnPeerRecord = Convert-Convergence $spawnConvergence
$resetPeerRecord = Convert-Convergence $resetConvergence
$spawnPass = $null -ne $spawnRecord -and $spawnRecord.spawned -eq 1 -and $spawnRecord.countAfter -eq ($spawnRecord.countBefore + 1) -and $spawnRecord.hasPlacedAfter -eq 1 -and $spawnRecord.replicated -eq 1 -and $spawnRecord.netLoad -eq 1
$quotaPass = $null -ne $quotaRecord -and $quotaRecord.canPlaceAgain -eq 0
$spawnPeerPass = $null -ne $spawnPeerRecord -and $spawnPeerRecord.pass -eq 1 -and $spawnPeerRecord.worlds -eq 4 -and $spawnPeerRecord.worldsWithBuildable -eq 4
$cancelPass = $null -ne $cancelRecord -and $cancelRecord.active -eq 0 -and $cancelRecord.placementTag -eq 0 -and $cancelRecord.hasPlaced -eq 0
$resetPass = $null -ne $resetRecord -and $resetRecord.countBefore -ge 1 -and $resetRecord.countAfter -eq $resetRecord.countBefore -and $resetRecord.hasPlacedAfter -eq 0
$resetPeerPass = $null -ne $resetPeerRecord -and $resetPeerRecord.pass -eq 1 -and $resetPeerRecord.worlds -eq 4 -and $resetPeerRecord.worldsWithBuildable -eq 4
$invalidConfirmObserved = $null -ne $confirmRecord -and $confirmRecord.active -eq 1 -and $confirmRecord.placementTag -eq 1 -and $confirmRecord.hasPlaced -eq 0
$malformedRecords = [ordered]@{}
foreach ($kind in @('empty', 'wrong', 'multi')) {
    $record = $malformedMatches | Where-Object { $_.Match.Groups['kind'].Value -eq $kind } | Select-Object -Last 1
    if ($null -ne $record) {
        $malformedRecords[$kind] = [ordered]@{
            active = Get-IntGroup $record.Match 'active'
            placementTag = Get-IntGroup $record.Match 'placementTag'
            hasPlaced = Get-IntGroup $record.Match 'hasPlaced'
        }
    } else {
        $malformedRecords[$kind] = $null
    }
}
$malformedPass = [ordered]@{}
foreach ($kind in @('empty', 'wrong', 'multi')) {
    $record = $malformedRecords[$kind]
    $malformedPass[$kind] = $null -ne $record -and $record.active -eq 0 -and $record.placementTag -eq 0 -and $record.hasPlaced -eq 0
}
$malformedAllPass = $malformedPass.empty -and $malformedPass.wrong -and $malformedPass.multi
$replicatedSubmitRecord = if ($replicatedSubmit) {
    [ordered]@{
        playerId = Get-IntGroup $replicatedSubmit.Match 'playerId'
        countBefore = Get-IntGroup $replicatedSubmit.Match 'countBefore'
        lagMs = Get-IntGroup $replicatedSubmit.Match 'lagMs'
    }
} else { $null }
$replicatedResultRecord = if ($replicatedResult) {
    [ordered]@{
        playerId = Get-IntGroup $replicatedResult.Match 'playerId'
        countBefore = Get-IntGroup $replicatedResult.Match 'countBefore'
        countAfter = Get-IntGroup $replicatedResult.Match 'countAfter'
        spawned = Get-IntGroup $replicatedResult.Match 'spawned'
        hasPlacedAfter = Get-IntGroup $replicatedResult.Match 'hasPlacedAfter'
        authorityAbilityActive = Get-IntGroup $replicatedResult.Match 'authorityAbilityActive'
    }
} else { $null }
$replicatedQuotaRejectPass = $null -ne $replicatedSubmitRecord -and $replicatedSubmitRecord.lagMs -eq 100 -and $null -ne $replicatedResultRecord -and $replicatedResultRecord.playerId -eq $replicatedSubmitRecord.playerId -and $replicatedResultRecord.countAfter -eq $replicatedResultRecord.countBefore -and $replicatedResultRecord.spawned -eq 0 -and $replicatedResultRecord.hasPlacedAfter -eq 1 -and $replicatedResultRecord.authorityAbilityActive -eq 0 -and $null -ne $authorityCallback -and (Get-IntGroup $authorityCallback.Match 'valid') -eq 1 -and (Get-IntGroup $authorityCallback.Match 'entries') -eq 1 -and $null -ne $authorityTargetDataResult -and (Get-IntGroup $authorityTargetDataResult.Match 'accepted') -eq 0
$replicatedConfirmSubmitRecord = if ($replicatedConfirmSubmit) {
    [ordered]@{
        playerId = Get-IntGroup $replicatedConfirmSubmit.Match 'playerId'
        countBefore = Get-IntGroup $replicatedConfirmSubmit.Match 'countBefore'
        targetBlocking = Get-IntGroup $replicatedConfirmSubmit.Match 'targetBlocking'
    }
} else { $null }
$replicatedConfirmResultRecord = if ($replicatedConfirmResult) {
    [ordered]@{
        playerId = Get-IntGroup $replicatedConfirmResult.Match 'playerId'
        countBefore = Get-IntGroup $replicatedConfirmResult.Match 'countBefore'
        countAfter = Get-IntGroup $replicatedConfirmResult.Match 'countAfter'
        spawned = Get-IntGroup $replicatedConfirmResult.Match 'spawned'
        hasPlacedAfter = Get-IntGroup $replicatedConfirmResult.Match 'hasPlacedAfter'
        authorityAbilityActive = Get-IntGroup $replicatedConfirmResult.Match 'authorityAbilityActive'
    }
} else { $null }
$replicatedConfirmCleanupRecord = if ($replicatedConfirmCleanup) {
    [ordered]@{
        playerId = Get-IntGroup $replicatedConfirmCleanup.Match 'playerId'
        countAfterCleanup = Get-IntGroup $replicatedConfirmCleanup.Match 'countAfterCleanup'
    }
} else { $null }
$replicatedConfirmPeerConvergence = Get-LastMatch 'ReplicatedConfirm PeerConvergence worlds=(?<worlds>\d+) worldsWithBuildable=(?<worldsWithBuildable>\d+) playerId=(?<playerId>\d+) pass=(?<pass>\d+)'
$replicatedConfirmPeerRecord = Convert-Convergence $replicatedConfirmPeerConvergence
$replicatedValidConfirmPass = $null -ne $replicatedConfirmSubmitRecord -and $replicatedConfirmSubmitRecord.targetBlocking -eq 1 -and $null -ne $replicatedConfirmResultRecord -and $replicatedConfirmResultRecord.playerId -eq $replicatedConfirmSubmitRecord.playerId -and $replicatedConfirmResultRecord.countAfter -eq ($replicatedConfirmResultRecord.countBefore + 1) -and $replicatedConfirmResultRecord.spawned -eq 1 -and $replicatedConfirmResultRecord.hasPlacedAfter -eq 1 -and $replicatedConfirmResultRecord.authorityAbilityActive -eq 0 -and $null -ne $replicatedConfirmCleanupRecord -and $replicatedConfirmCleanupRecord.countAfterCleanup -eq 0 -and $null -ne $replicatedConfirmPeerRecord -and $replicatedConfirmPeerRecord.pass -eq 1 -and $replicatedConfirmPeerRecord.worlds -eq 4 -and $replicatedConfirmPeerRecord.worldsWithBuildable -eq 4 -and $null -ne $authorityCallback -and (Get-IntGroup $authorityCallback.Match 'valid') -eq 1 -and (Get-IntGroup $authorityCallback.Match 'entries') -eq 1 -and $null -ne $authorityTargetDataResult -and (Get-IntGroup $authorityTargetDataResult.Match 'accepted') -eq 1

$brief = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    log = $resolvedLog
    status = if ($spawnPass -and $quotaPass -and $spawnPeerPass -and $cancelPass -and $resetPass -and $resetPeerPass -and $malformedAllPass) { 'passed-scope-limited' } else { 'partial' }
    scope = 'native same-process four-world PIE; includes synthetic client-to-server valid target confirmation when present; physical input, packaged UI/LAN, and second-PC proof are outside this brief'
    counts = [ordered]@{
        totalLines = $lines.Count
        markerLines = $markerLines.Count
        peerSamples = $peerSamples.Count
        fixtureErrors = $fixtureErrors.Count
    }
    pass = [ordered]@{
        spawn = $spawnPass
        quota = $quotaPass
        spawnPeerConvergence = $spawnPeerPass
        cancelCleanup = $cancelPass
        resetPersistence = $resetPass
        resetPeerConvergence = $resetPeerPass
        malformedEmpty = $malformedPass.empty
        malformedWrong = $malformedPass.wrong
        malformedMulti = $malformedPass.multi
        replicatedQuotaReject = $replicatedQuotaRejectPass
        replicatedValidConfirm = $replicatedValidConfirmPass
    }
    spawn = $spawnRecord
    quota = $quotaRecord
    cancel = $cancelRecord
    confirm = $confirmRecord
    invalidConfirmObserved = $invalidConfirmObserved
    malformed = $malformedRecords
    replicatedQuotaReject = [ordered]@{
        submit = $replicatedSubmitRecord
        authorityCallback = if ($authorityCallback) { [ordered]@{ valid = Get-IntGroup $authorityCallback.Match 'valid'; entries = Get-IntGroup $authorityCallback.Match 'entries' } } else { $null }
        targetDataResult = if ($authorityTargetDataResult) { [ordered]@{ accepted = Get-IntGroup $authorityTargetDataResult.Match 'accepted' } } else { $null }
        result = $replicatedResultRecord
    }
    replicatedValidConfirm = [ordered]@{
        submit = $replicatedConfirmSubmitRecord
        authorityCallback = if ($authorityCallback) { [ordered]@{ valid = Get-IntGroup $authorityCallback.Match 'valid'; entries = Get-IntGroup $authorityCallback.Match 'entries' } } else { $null }
        targetDataResult = if ($authorityTargetDataResult) { [ordered]@{ accepted = Get-IntGroup $authorityTargetDataResult.Match 'accepted' } } else { $null }
        result = $replicatedConfirmResultRecord
        peerConvergence = $replicatedConfirmPeerRecord
        cleanup = $replicatedConfirmCleanupRecord
    }
    reset = $resetRecord
    spawnPeerConvergence = $spawnPeerRecord
    resetPeerConvergence = $resetPeerRecord
    firstFixtureError = if ($fixtureErrors.Count -gt 0) { [string]$fixtureErrors[0] } else { $null }
}

if ($IncludeSamples) {
    $brief.peerSampleLines = $peerSamples
}

if ($OutputJson) {
    $outputDirectory = [System.IO.Path]::GetDirectoryName($OutputJson)
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) {
        $OutputJson
    } elseif ([string]::IsNullOrWhiteSpace($outputDirectory)) {
        Join-Path (Split-Path -Parent $resolvedLog) $OutputJson
    } else {
        Join-Path (Get-Location) $OutputJson
    }
    $parent = Split-Path -Parent $outputPath
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    $brief | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $outputPath -Encoding UTF8
    $brief.outputJson = (Resolve-Path -LiteralPath $outputPath).Path
}

$brief | ConvertTo-Json -Depth 20 -Compress
