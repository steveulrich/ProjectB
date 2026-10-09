[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Gate,
    [string]$Label = 'capture',
    [string]$SourceDirectory,
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $SourceDirectory) {
    $SourceDirectory = Join-Path $root 'Saved\Logs'
}
$source = (Resolve-Path -LiteralPath $SourceDirectory -ErrorAction Stop).Path
$destination = Join-Path $root (Join-Path 'Saved\Logs' $Gate)
New-Item -ItemType Directory -Path $destination -Force | Out-Null

$records = [System.Collections.Generic.List[object]]::new()
foreach ($name in @('ProjectB.log', 'ProjectB_2.log', 'ProjectB_3.log')) {
    $inputPath = Join-Path $source $name
    if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf)) {
        $records.Add([ordered]@{ source = $inputPath; destination = $null; copied = $false; reason = 'missing' })
        continue
    }

    $stem = [System.IO.Path]::GetFileNameWithoutExtension($name)
    $outputPath = Join-Path $destination ("$Label-$stem.log")
    Copy-Item -LiteralPath $inputPath -Destination $outputPath -Force
    $item = Get-Item -LiteralPath $outputPath
    $records.Add([ordered]@{
        source = $inputPath
        destination = $item.FullName
        copied = $true
        bytes = $item.Length
        lastWriteTime = $item.LastWriteTime.ToString('o')
    })
}

$summary = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    gate = $Gate
    label = $Label
    sourceDirectory = $source
    destinationDirectory = $destination
    copiedCount = @($records | Where-Object copied).Count
    records = @($records.ToArray())
    scope = 'Preserve the three separate-process PIE child logs before another launch or teardown; this does not interpret gameplay evidence.'
}
if ($OutputJson) {
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $summary.outputJson = [System.IO.Path]::GetFullPath($outputPath)
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
}

$summary | ConvertTo-Json -Depth 8 -Compress
