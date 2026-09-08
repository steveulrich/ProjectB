[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Candidate,
    [string]$OutputJson,
    [switch]$RequireCandidate
)
$ErrorActionPreference = 'Stop'
$config = Import-PowerShellDataFile (Join-Path $PSScriptRoot 'BwayTestConfig.psd1')
. (Join-Path $PSScriptRoot 'Resolve-UnrealEngine.ps1')
$engine = Resolve-UnrealEngine -EngineAssociation $config.EngineAssociation -DefaultEngineRoot $config.DefaultEngineRoot
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$candidatePath = if ($Candidate) { Join-Path $root "Saved/StagedBuilds/$Candidate" } else { $null }
$executable = if ($candidatePath) { Join-Path $candidatePath "Windows/ProjectB/Binaries/Win64/$($config.GameBinary)" } else { $null }
$head = & git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve source revision.' }
$dirty = @(& git -C $root status --short)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source changes.' }
$checks = [ordered]@{
    projectFile = Test-Path -LiteralPath (Join-Path $root 'ProjectB.uproject') -PathType Leaf
    engineEditor = Test-Path -LiteralPath (Join-Path $engine.EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -PathType Leaf
    buildTool = Test-Path -LiteralPath $engine.BuildBat -PathType Leaf
}
$artifactExists = [bool]($executable -and (Test-Path -LiteralPath $executable -PathType Leaf))
if ($RequireCandidate) { $checks.candidateExecutable = $artifactExists }
$ready = $checks.Values -notcontains $false
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    currentWorktreeHead = $head.Trim()
    engineRoot = $engine.EngineRoot
    candidate = $Candidate
    candidatePath = $candidatePath
    artifactExists = $artifactExists
    dirtyFileCount = $dirty.Count
    dirtyFiles = @($dirty | Select-Object -First 20)
    checks = $checks
    ready = $ready
    nextAction = if (!$ready) { 'Resolve failed checks.' } elseif ($RequireCandidate) { 'Run candidate tests; match evidence to its build manifest.' } else { 'Inspect active build/editor processes and unsaved assets before starting a build.' }
}
$json = $result | ConvertTo-Json -Depth 6
if ($OutputJson) { $json | Set-Content -LiteralPath $OutputJson -Encoding UTF8 }
$json
if (!$ready) { exit 1 }
exit 0
