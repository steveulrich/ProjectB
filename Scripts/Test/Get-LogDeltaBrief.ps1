[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$LogFile,
    [Parameter(Mandatory)][string]$StatePath,
    [string[]]$Pattern = @(),
    [ValidateRange(0, 8)][int]$ExampleCount = 2,
    [switch]$Reset,
    [switch]$QuietUnchanged,
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Resolve-ProjectBPath {
    param([Parameter(Mandatory)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $root $Path))
}

$stateFile = Resolve-ProjectBPath -Path $StatePath
$stateDirectory = Split-Path -Parent $stateFile
if ($stateDirectory -and -not (Test-Path -LiteralPath $stateDirectory)) {
    New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
}

$state = [ordered]@{ schema = 1; files = [ordered]@{} }
if (-not $Reset -and (Test-Path -LiteralPath $stateFile -PathType Leaf)) {
    try {
        $loaded = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
        if ($loaded.schema -eq 1 -and $loaded.files) {
            $state = [ordered]@{ schema = 1; files = [ordered]@{} }
            foreach ($property in $loaded.files.PSObject.Properties) {
                $state.files[$property.Name] = [ordered]@{
                    offset = [int64]$property.Value.offset
                    lineCount = [int64]$property.Value.lineCount
                }
            }
        }
    } catch {
        $state = [ordered]@{ schema = 1; files = [ordered]@{} }
    }
}

$knownNonFatalErrorPatterns = @(
    'LogZenServiceInstance: Error: Failed to write security configuration'
)
$criticalPattern = 'Ensure condition failed|Assertion failed|Fatal error|Accessed None'
$reports = foreach ($logName in $LogFile) {
    $path = Resolve-ProjectBPath -Path $logName
    $item = Get-Item -LiteralPath $path -ErrorAction Stop
    $key = $item.FullName
    $previous = $null
    if ($state.files.Contains($key)) { $previous = $state.files[$key] }
    $previousOffset = if ($previous) { [int64]$previous.offset } else { 0L }
    $startOffset = $previousOffset
    $resetForTruncation = $startOffset -gt $item.Length
    if ($resetForTruncation) { $startOffset = 0L }
    $reportedPreviousOffset = if ($resetForTruncation) { 0L } else { $previousOffset }
    $previousLineCount = if ($previous -and -not $resetForTruncation) { [int64]$previous.lineCount } else { 0L }

    $lineCount = 0L
    $newErrors = 0
    $knownNonFatalErrors = 0
    $unknownErrors = 0
    $newCritical = 0
    $errorExamples = [System.Collections.Generic.List[object]]::new()
    $criticalExamples = [System.Collections.Generic.List[object]]::new()
    $patternResults = @($Pattern | ForEach-Object {
        [ordered]@{ pattern = $_; count = 0; first = $null }
    })

    $stream = [System.IO.FileStream]::new(
        $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    try {
        # PowerShell redirects native build output as UTF-16. Detect the BOM at
        # the beginning even when this poll resumes from a later byte offset.
        $encodingProbe = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::UTF8, $true, 128, $true)
        try {
            [void]$encodingProbe.Peek()
            $encoding = $encodingProbe.CurrentEncoding
        } finally { $encodingProbe.Dispose() }
        $stream.Seek($startOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $discardPartialLine = $false
        if ($startOffset -gt 0L) {
            $newline = $encoding.GetBytes("`n")
            $previousNewline = [byte[]]::new($newline.Length)
            if ($startOffset -ge $newline.Length) {
                $stream.Seek($startOffset - $newline.Length, [System.IO.SeekOrigin]::Begin) | Out-Null
                $read = $stream.Read($previousNewline, 0, $previousNewline.Length)
                $discardPartialLine = $read -ne $newline.Length -or
                    [System.BitConverter]::ToString($previousNewline) -ne [System.BitConverter]::ToString($newline)
            } else { $discardPartialLine = $true }
            $stream.Seek($startOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        }
        $reader = [System.IO.StreamReader]::new($stream, $encoding, $true, 4096, $true)
        try {
            if ($discardPartialLine) { [void]$reader.ReadLine() }
            while ($null -ne ($line = $reader.ReadLine())) {
                $lineCount++
                if ($line -match ': Error:') {
                    $newErrors++
                    $known = $false
                    foreach ($knownPattern in $knownNonFatalErrorPatterns) {
                        if ($line -match $knownPattern) { $known = $true; break }
                    }
                    if ($known) {
                        $knownNonFatalErrors++
                    } else {
                        $unknownErrors++
                        if ($errorExamples.Count -lt $ExampleCount) {
                            $errorExamples.Add([ordered]@{ line = [int64]$lineCount + $previousLineCount; text = $line })
                        }
                    }
                }
                if ($line -match $criticalPattern) {
                    $newCritical++
                    if ($criticalExamples.Count -lt $ExampleCount) {
                        $criticalExamples.Add([ordered]@{ line = [int64]$lineCount + $previousLineCount; text = $line })
                    }
                }
                for ($index = 0; $index -lt $patternResults.Count; $index++) {
                    if ($line -match $patternResults[$index].pattern) {
                        $patternResults[$index].count++
                        if ($null -eq $patternResults[$index].first) { $patternResults[$index].first = $line }
                    }
                }
            }
        } finally { $reader.Dispose() }
    } finally { $stream.Dispose() }

    $state.files[$key] = [ordered]@{
        offset = [int64]$item.Length
        lineCount = [int64]($lineCount + $previousLineCount)
        encoding = $encoding.WebName
    }
    [ordered]@{
        path = $key
        encoding = $encoding.WebName
        resetForTruncation = $resetForTruncation
        previousOffset = $reportedPreviousOffset
        newBytes = [math]::Max(0, [int64]$item.Length - $reportedPreviousOffset)
        newLines = $lineCount
        totalLines = $state.files[$key].lineCount
        newErrors = $newErrors
        knownNonFatalErrors = $knownNonFatalErrors
        unknownErrors = $unknownErrors
        newCritical = $newCritical
        errorExamples = @($errorExamples.ToArray())
        criticalExamples = @($criticalExamples.ToArray())
        patternCounts = @($patternResults)
    }
}

$stateJson = $state | ConvertTo-Json -Depth 8
$tempState = "$stateFile.$([guid]::NewGuid().ToString('N')).tmp"
$stateJson | Set-Content -LiteralPath $tempState -Encoding UTF8
Move-Item -LiteralPath $tempState -Destination $stateFile -Force

$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    statePath = $stateFile
    scope = 'Incremental log inventory. Reads only appended lines after the saved byte offsets; full logs remain unchanged and this does not establish acceptance.'
    logs = @($reports)
}
$result.changed = [bool](@($reports | Where-Object {
    $_.resetForTruncation -or $_.newBytes -gt 0 -or $_.newLines -gt 0
}).Count)

if ($OutputJson) {
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $result.outputJson = [System.IO.Path]::GetFullPath($outputPath)
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outputPath -Encoding UTF8
}

$consoleResult = $result
if ($QuietUnchanged -and -not $result.changed) {
    $consoleResult = [ordered]@{
        generatedUtc = $result.generatedUtc
        statePath = $result.statePath
        scope = 'No appended log data since the saved offsets. Full state and logs remain on disk; this does not establish acceptance.'
        changed = $false
        logs = @($reports | ForEach-Object {
            [ordered]@{ path = $_.path; newBytes = $_.newBytes; newLines = $_.newLines; totalLines = $_.totalLines }
        })
    }
}
$consoleResult | ConvertTo-Json -Depth 10 -Compress
