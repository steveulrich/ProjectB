[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ContractPath,
    [Parameter(Mandatory)][ValidateSet('compile', 'runtime', 'package')][string]$Phase,
    [Parameter(Mandatory)][ValidateSet('passed', 'partial', 'blocked', 'not-run')][string]$PhaseStatus,
    [string]$Scope,
    [string[]]$Evidence = @(),
    [string]$EvidenceCsv,
    [string[]]$ProofGaps = @(),
    [string]$ProofGapsCsv,
    [string]$NextAction,
    [ValidateSet('pending', 'compile-pass', 'runtime-pass-unverified', 'package-pass-unverified', 'blocked')][string]$OverallStatus
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$path = if ([System.IO.Path]::IsPathRooted($ContractPath)) {
    (Resolve-Path -LiteralPath $ContractPath -ErrorAction Stop).Path
} else {
    (Resolve-Path -LiteralPath (Join-Path $root $ContractPath) -ErrorAction Stop).Path
}
$record = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
$phaseRecord = $record.PSObject.Properties[$Phase].Value
if ($null -eq $phaseRecord) {
    $record | Add-Member -NotePropertyName $Phase -NotePropertyValue ([pscustomobject]@{})
    $phaseRecord = $record.PSObject.Properties[$Phase].Value
}
function Set-RecordProperty {
    param([Parameter(Mandatory)]$Object, [Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)]$Value)
    $property = $Object.PSObject.Properties[$Name]
    if ($property) { $property.Value = $Value } else { $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value }
}
Set-RecordProperty -Object $phaseRecord -Name 'status' -Value $PhaseStatus
if ($PSBoundParameters.ContainsKey('Scope')) { Set-RecordProperty -Object $phaseRecord -Name 'scope' -Value $Scope }
if ($PSBoundParameters.ContainsKey('EvidenceCsv')) {
    $Evidence = @($EvidenceCsv -split '\s*,\s*' | Where-Object { $_ })
}
if ($PSBoundParameters.ContainsKey('Evidence')) { Set-RecordProperty -Object $phaseRecord -Name 'evidence' -Value @($Evidence) }
if ($PSBoundParameters.ContainsKey('EvidenceCsv')) { Set-RecordProperty -Object $phaseRecord -Name 'evidence' -Value @($Evidence) }
if ($PSBoundParameters.ContainsKey('ProofGapsCsv')) {
    $ProofGaps = @($ProofGapsCsv -split '\s*,\s*' | Where-Object { $_ })
}
if ($PSBoundParameters.ContainsKey('ProofGaps')) { Set-RecordProperty -Object $phaseRecord -Name 'proofGaps' -Value @($ProofGaps) }
if ($PSBoundParameters.ContainsKey('ProofGapsCsv')) { Set-RecordProperty -Object $phaseRecord -Name 'proofGaps' -Value @($ProofGaps) }
Set-RecordProperty -Object $phaseRecord -Name 'updatedUtc' -Value ([DateTime]::UtcNow.ToString('o'))
if ($OverallStatus) { Set-RecordProperty -Object $record -Name 'status' -Value $OverallStatus }
if ($NextAction) { Set-RecordProperty -Object $record -Name 'nextAction' -Value $NextAction }
$temporaryPath = "$path.$([guid]::NewGuid().ToString('N')).partial"
try {
    $record | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $temporaryPath -Encoding UTF8
    Move-Item -LiteralPath $temporaryPath -Destination $path -Force
} finally {
    if (Test-Path -LiteralPath $temporaryPath -PathType Leaf) {
        Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
    }
}

[ordered]@{
    contract = $path
    phase = $Phase
    phaseStatus = $PhaseStatus
    overallStatus = $record.status
    nextAction = $record.nextAction
    scope = $phaseRecord.scope
    proofGaps = @($phaseRecord.proofGaps)
} | ConvertTo-Json -Depth 6 -Compress
