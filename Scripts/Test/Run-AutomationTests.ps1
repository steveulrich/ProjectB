[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TestFilter,
    [string]$OutputPrefix = "Saved/Logs/automation-$([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))",
    [ValidateRange(30, 1800)][int]$TimeoutSeconds = 180,
    [switch]$ForceMemoryDDC,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$config = Import-PowerShellDataFile (Join-Path $PSScriptRoot 'BwayTestConfig.psd1')
. (Join-Path $PSScriptRoot 'Resolve-UnrealEngine.ps1')
$engine = Resolve-UnrealEngine -EngineAssociation $config.EngineAssociation -DefaultEngineRoot $config.DefaultEngineRoot
$editor = $engine.EditorCmd
$project = $config.ProjectFile
$prefix = if ([System.IO.Path]::IsPathRooted($OutputPrefix)) { $OutputPrefix } else { Join-Path $root $OutputPrefix }
$prefix = [System.IO.Path]::GetFullPath($prefix)
$outputDirectory = Split-Path -Parent $prefix
if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
$paths = [ordered]@{
    log = "$prefix.log"
    stdout = "$prefix.stdout"
    stderr = "$prefix.stderr"
    report = "$prefix-report"
    outputJson = "$prefix.json"
}
if (Test-Path -LiteralPath $paths.report) {
    throw "Automation report directory already exists; choose a fresh OutputPrefix: $($paths.report)"
}
$checks = [ordered]@{
    project = Test-Path -LiteralPath $project -PathType Leaf
    editor = Test-Path -LiteralPath $editor -PathType Leaf
    reportPathFresh = -not (Test-Path -LiteralPath $paths.report)
}
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    testFilter = $TestFilter
    timeoutSeconds = $TimeoutSeconds
    forceMemoryDDC = [bool]$ForceMemoryDDC
    validateOnly = [bool]$ValidateOnly
    engine = $engine.EngineRoot
    project = $project
    paths = $paths
    checks = $checks
    status = if ($checks.Values -notcontains $false) { 'ready' } else { 'blocked' }
}
if ($ValidateOnly) {
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $paths.outputJson -Encoding UTF8
    $result | ConvertTo-Json -Depth 8 -Compress
    if ($result.status -ne 'ready') { exit 1 }
    exit 0
}
if ($checks.Values -contains $false) {
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $paths.outputJson -Encoding UTF8
    $result | ConvertTo-Json -Depth 8 -Compress
    exit 1
}
if (@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count -gt 0) {
    throw 'An Unreal editor process is already running.'
}
$quotedProject = '"' + $project + '"'
$args = "$quotedProject -unattended -nosplash -nullrhi -nosound -nop4 -log"
if ($ForceMemoryDDC) { $args += ' -DDC-ForceMemoryCache' }
$args += " `"-ExecCmds=Automation RunTests $TestFilter;Quit`" `"-ReportExportPath=$($paths.report)`" `"-abslog=$($paths.log)`""
$startedUtc = [DateTime]::UtcNow
$process = Start-Process -FilePath $editor -ArgumentList $args -RedirectStandardOutput $paths.stdout -RedirectStandardError $paths.stderr -PassThru -WindowStyle Hidden
$result.pid = $process.Id
$finished = $process.WaitForExit($TimeoutSeconds * 1000)
if (-not $finished) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $result.status = 'timeout'
    $result.elapsedSeconds = [math]::Round(([DateTime]::UtcNow - $startedUtc).TotalSeconds, 2)
    $result.cleanup = 'exact editor PID stopped after timeout'
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $paths.outputJson -Encoding UTF8
    $result | ConvertTo-Json -Depth 10 -Compress
    exit 1
}
$result.processExitCode = $process.ExitCode
$result.elapsedSeconds = [math]::Round(([DateTime]::UtcNow - $startedUtc).TotalSeconds, 2)
$reportIndex = Join-Path $paths.report 'index.json'
$reportData = $null
if (Test-Path -LiteralPath $reportIndex -PathType Leaf) {
    try { $reportData = Get-Content -LiteralPath $reportIndex -Raw | ConvertFrom-Json } catch { $result.reportParseError = $_.Exception.Message }
}
$result.report = if ($reportData) {
    [ordered]@{
        path = $reportIndex
        succeeded = $reportData.succeeded
        succeededWithWarnings = $reportData.succeededWithWarnings
        failed = $reportData.failed
        notRun = $reportData.notRun
        tests = @($reportData.tests | ForEach-Object { [ordered]@{ path = $_.fullTestPath; state = $_.state; errors = $_.errors; warnings = $_.warnings } })
    }
} else { $null }
$logText = if (Test-Path -LiteralPath $paths.log -PathType Leaf) { Get-Content -LiteralPath $paths.log -Raw } else { '' }
$result.frameworkExitCodeMarker = if ($logText -match 'TEST COMPLETE\. EXIT CODE: 0') { 0 } else { $null }
$testsPass = $reportData -and $reportData.failed -eq 0 -and $reportData.notRun -eq 0 -and $reportData.succeeded -gt 0 -and $reportData.succeededWithWarnings -eq 0
$result.status = if ($testsPass -and $result.frameworkExitCodeMarker -eq 0 -and $result.processExitCode -eq 0) {
    'passed'
} elseif ($testsPass -and $result.frameworkExitCodeMarker -eq 0) {
    'passed-test-report-process-anomaly'
} else {
    'failed'
}
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $paths.outputJson -Encoding UTF8
$result | ConvertTo-Json -Depth 12 -Compress
if ($result.status -ne 'passed') { exit 1 }
exit 0
