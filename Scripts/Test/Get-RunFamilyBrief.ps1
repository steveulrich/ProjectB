[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [Parameter(Mandatory)][string]$FilePattern,
    [string]$OutputJson,
    [ValidateRange(1, 12)][int]$SignatureCount = 8
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$directory = if ([System.IO.Path]::IsPathRooted($EvidenceDirectory)) {
    (Resolve-Path -LiteralPath $EvidenceDirectory -ErrorAction Stop).Path
} else {
    (Resolve-Path -LiteralPath (Join-Path $root $EvidenceDirectory) -ErrorAction Stop).Path
}

$knownNonFatalErrorPatterns = @(
    'LogZenServiceInstance: Error: Failed to write security configuration'
)
$criticalPattern = 'Ensure condition failed|Assertion failed|Fatal error|Accessed None'
$files = @(Get-ChildItem -LiteralPath $directory -Filter $FilePattern -File | Sort-Object LastWriteTimeUtc)
$signatureMap = @{}
$records = foreach ($item in $files) {
    $lineCount = 0L
    $errorLines = 0
    $knownNonFatalErrorLines = 0
    $unknownErrorLines = 0
    $criticalLines = 0
    $reader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
        $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite))
    try {
        while ($null -ne ($line = $reader.ReadLine())) {
            $lineCount++
            if ($line -match ': Error:') {
                $errorLines++
                $known = $false
                foreach ($knownPattern in $knownNonFatalErrorPatterns) {
                    if ($line -match $knownPattern) { $known = $true; break }
                }
                if ($known) { $knownNonFatalErrorLines++ } else { $unknownErrorLines++ }
                $signature = $line -replace '^\[[^]]+\]\[[^]]+\]\s*', ''
                if ($signatureMap.ContainsKey($signature)) {
                    $signatureMap[$signature].count++
                } else {
                    $signatureMap[$signature] = [ordered]@{ count = 1; example = $line }
                }
            }
            if ($line -match $criticalPattern) { $criticalLines++ }
        }
    } finally { $reader.Dispose() }
    [ordered]@{
        name = $item.Name
        bytes = $item.Length
        lines = $lineCount
        errorLines = $errorLines
        knownNonFatalErrorLines = $knownNonFatalErrorLines
        unknownErrorLines = $unknownErrorLines
        criticalLines = $criticalLines
        lastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
    }
}

$signatures = @($signatureMap.GetEnumerator() | Sort-Object { $_.Value.count } -Descending | Select-Object -First $SignatureCount | ForEach-Object {
    [ordered]@{ count = $_.Value.count; example = $_.Value.example }
})
$totals = [ordered]@{
    totalBytes = 0L
    totalLines = 0L
    errorLines = 0L
    knownNonFatalErrorLines = 0L
    unknownErrorLines = 0L
    criticalLines = 0L
}
foreach ($record in $records) {
    $totals.totalBytes += [int64]$record.bytes
    $totals.totalLines += [int64]$record.lines
    $totals.errorLines += [int64]$record.errorLines
    $totals.knownNonFatalErrorLines += [int64]$record.knownNonFatalErrorLines
    $totals.unknownErrorLines += [int64]$record.unknownErrorLines
    $totals.criticalLines += [int64]$record.criticalLines
}
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    evidenceDirectory = $directory
    filePattern = $FilePattern
    scope = 'Run-family inventory. It summarizes matching attempts and repeated error signatures without establishing acceptance or replacing the selected gate evidence.'
    attemptCount = $records.Count
    totalBytes = $totals.totalBytes
    totalLines = $totals.totalLines
    errorLines = $totals.errorLines
    knownNonFatalErrorLines = $totals.knownNonFatalErrorLines
    unknownErrorLines = $totals.unknownErrorLines
    criticalLines = $totals.criticalLines
    latest = if ($records.Count) { $records[-1] } else { $null }
    errorSignatures = $signatures
    files = @($records)
}
if ($OutputJson) {
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $result.outputJson = [System.IO.Path]::GetFullPath($outputPath)
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outputPath -Encoding UTF8
}
$result | ConvertTo-Json -Depth 10 -Compress
