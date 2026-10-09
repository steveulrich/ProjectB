[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Candidate,
    [string]$SourceHead,
    [string]$StagePath,
    [string[]]$EvidenceLog = @(),
    [string]$OutputJson,
    [string]$Profile = 'ShortMatch',
    [switch]$SummaryOnly
)
$ErrorActionPreference = 'Stop'
$config = Import-PowerShellDataFile (Join-Path $PSScriptRoot 'BwayTestConfig.psd1')
$profileConfig = $config.TestProfiles[$Profile]
if (!$profileConfig) { throw "Unknown test profile '$Profile'." }
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$stage = if ($StagePath) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($StagePath) } else { Join-Path $root "Saved/StagedBuilds/$Candidate" }
$executable = Join-Path $stage "Windows/ProjectB/Binaries/Win64/$($config.GameBinary)"
# SourceHead must come from this candidate's build record, never today's checkout.
$logs = @($EvidenceLog | Where-Object { $_ } | ForEach-Object {
    $path = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($_)
    $exists = Test-Path -LiteralPath $path -PathType Leaf
    $contents = if ($exists) { [IO.File]::ReadAllText($path) } else { '' }
    [ordered]@{
        path = $path
        readableAndNonempty = $exists -and ![string]::IsNullOrWhiteSpace($contents)
        missingPatterns = @($profileConfig.RequiredPatterns | Where-Object { $contents.IndexOf($_, [StringComparison]::OrdinalIgnoreCase) -lt 0 })
        forbiddenMatches = @($profileConfig.ForbiddenPatterns | Where-Object { $contents.IndexOf($_, [StringComparison]::OrdinalIgnoreCase) -ge 0 })
    }
})
$gates = [ordered]@{
    sourceRevisionProvided = $SourceHead -match '^[0-9a-fA-F]{40}$'
    packagedExecutable = Test-Path -LiteralPath $executable -PathType Leaf
    evidencePresent = $logs.Count -gt 0 -and @($logs | Where-Object { !$_.readableAndNonempty }).Count -eq 0
    requiredPatternsInEveryLog = $logs.Count -gt 0 -and @($logs | Where-Object { $_.missingPatterns.Count -gt 0 }).Count -eq 0
    noForbiddenPatterns = $logs.Count -gt 0 -and @($logs | Where-Object { !$_.readableAndNonempty -or $_.forbiddenMatches.Count -gt 0 }).Count -eq 0
}
$failedGates = @($gates.Keys | Where-Object { !$gates[$_] })
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    scope = 'Startup log screening only; gameplay, clean exit, and candidate/log association require separate evidence.'
    candidate = $Candidate
    profile = $Profile
    sourceBaseRevision = $SourceHead
    stagePath = $stage
    evidence = $logs
    gates = $gates
    passed = $failedGates.Count -eq 0
    nextAction = if ($failedGates.Count) { 'Inspect: ' + ($failedGates -join ', ') } else { 'Continue pending gameplay gates in AI_Planning/SLICE_STATUS.md.' }
}
$json = $result | ConvertTo-Json -Depth 8
if ($OutputJson) {
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $result.outputJson = [System.IO.Path]::GetFullPath($outputPath)
    $json = $result | ConvertTo-Json -Depth 8
    $json | Set-Content -LiteralPath $outputPath -Encoding UTF8
}
if ($SummaryOnly) {
    [ordered]@{
        scope = 'Compact startup-screen projection; the complete per-log report remains in outputJson.'
        candidate = $result.candidate
        profile = $result.profile
        sourceBaseRevision = $result.sourceBaseRevision
        passed = $result.passed
        gates = $result.gates
        evidenceCount = @($result.evidence).Count
        failedGates = @($failedGates)
        outputJson = $result.outputJson
        nextAction = $result.nextAction
    } | ConvertTo-Json -Depth 8 -Compress
} else {
    $json
}
if (!$result.passed) { exit 1 }
exit 0
