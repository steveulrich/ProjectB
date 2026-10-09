[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogFile,
    [string]$ProjectFile,
    [string]$EngineRoot,
    [string]$ZenDataPath,
    [string]$OutputJson,
    [switch]$ValidateOnly,
    [switch]$CreateZenDataPath,
    [switch]$SkipPlatformValidation,
    [switch]$SkipTurnkeyValidation,
    [switch]$ForceMemoryDDC
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourceHead = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve the current source revision.' }
$dirtyFileCount = @(& git -C $root status --short).Count
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the current worktree.' }
if (-not $ProjectFile) { $ProjectFile = Join-Path $root 'ProjectB.uproject' }
$project = (Resolve-Path -LiteralPath $ProjectFile -ErrorAction Stop).Path
$projectRoot = Split-Path -Parent $project

if (-not $EngineRoot) { $EngineRoot = $env:UE_ENGINE_ROOT }
$editor = $null
if ($env:UE_EDITOR_PATH -and (Test-Path -LiteralPath $env:UE_EDITOR_PATH -PathType Leaf)) {
    $editor = (Resolve-Path -LiteralPath $env:UE_EDITOR_PATH).Path
}
if (-not $editor -and $EngineRoot) {
    $editorCandidate = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
    if (Test-Path -LiteralPath $editorCandidate -PathType Leaf) {
        $editor = (Resolve-Path -LiteralPath $editorCandidate).Path
    }
}
if (-not $editor) {
    throw 'Resolve the source-built editor first by setting UE_EDITOR_PATH or UE_ENGINE_ROOT.'
}

$projectPattern = [regex]::Escape($project)
$existing = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" -ErrorAction SilentlyContinue | Where-Object {
    $_.CommandLine -and $_.CommandLine -match $projectPattern
})
if ($existing.Count -gt 0) {
    $ids = ($existing | ForEach-Object ProcessId) -join ', '
    throw "ProjectB editor is already running (PID $ids). Reuse or close it before launching another instance."
}

$logDirectory = Split-Path -Parent $LogFile
if ($logDirectory -and -not (Test-Path -LiteralPath $logDirectory)) {
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
}
$resolvedLog = [System.IO.Path]::GetFullPath($LogFile)
$platformArgument = if ($SkipPlatformValidation) { ' -Multiprocess' } else { '' }
$turnkeyArgument = if ($SkipTurnkeyValidation) { ' -Unattended' } else { '' }
$ddcArgument = if ($ForceMemoryDDC) { ' -DDC-ForceMemoryCache' } else { '' }
$resolvedZenDataPath = $null
$zenWritable = $null
$zenDataPathCreated = $false
$zenArgument = if ($ZenDataPath) {
    $resolvedZenDataPath = [System.IO.Path]::GetFullPath($ZenDataPath)
    if (-not (Test-Path -LiteralPath $resolvedZenDataPath -PathType Container)) {
        if (-not $CreateZenDataPath) {
            throw "Zen data path does not exist: $resolvedZenDataPath (pass -CreateZenDataPath for a project-local gate directory)"
        }
        try {
            New-Item -ItemType Directory -Path $resolvedZenDataPath -Force | Out-Null
            $zenDataPathCreated = $true
        } catch {
            throw "Cannot create Zen data path: $resolvedZenDataPath ($($_.Exception.Message))"
        }
    }
    $probePath = Join-Path $resolvedZenDataPath ('.codex-zen-write-probe-' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        [System.IO.File]::WriteAllText($probePath, 'ProjectB editor preflight')
        $zenWritable = $true
    } catch {
        throw "Zen data path is not writable: $resolvedZenDataPath ($($_.Exception.Message))"
    } finally {
        if (Test-Path -LiteralPath $probePath -PathType Leaf) {
            Remove-Item -LiteralPath $probePath -Force -ErrorAction SilentlyContinue
        }
    }
    ' -ZenDataPath="' + $resolvedZenDataPath + '"'
} else { '' }
$argumentList = '"" "' + $project + '" -NoSplash' + $platformArgument + $turnkeyArgument + $ddcArgument + $zenArgument + ' -abslog="' + $resolvedLog + '"'
$record = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    project = $project
    sourceHead = $sourceHead
    dirtyFileCount = $dirtyFileCount
    editor = $editor
    log = $resolvedLog
    zenDataPath = $resolvedZenDataPath
    zenDataPathCreated = $zenDataPathCreated
    zenWritable = $zenWritable
    processId = $null
    arguments = $argumentList
    scope = if ($ValidateOnly) { 'Launch validation only; no editor was started.' } elseif ($SkipPlatformValidation -or $SkipTurnkeyValidation -or $ForceMemoryDDC) { 'Direct source-built editor launch with selected startup safeguards; use only for runtime gates with separate Win64 SDK and cache evidence.' } else { 'Direct source-built editor launch. Confirm bridge readiness before PIE.' }
    nextAction = 'Wait for the bridge, then run the selected gate. Stop this PID normally during cleanup.'
}
if (-not $ValidateOnly) {
    $process = Start-Process -FilePath $editor -ArgumentList $argumentList -WorkingDirectory $projectRoot -PassThru
    $record.processId = $process.Id
}
$json = $record | ConvertTo-Json -Depth 5
if ($OutputJson) {
    $outputDirectory = Split-Path -Parent $OutputJson
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $json | Set-Content -LiteralPath $OutputJson -Encoding UTF8
}
$json
