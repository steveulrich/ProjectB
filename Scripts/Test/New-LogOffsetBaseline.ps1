[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string[]]$LogFiles = @(
        'Saved/Logs/ProjectB.log',
        'Saved/Logs/ProjectB_2.log',
        'Saved/Logs/ProjectB_3.log'
    ),

    [Parameter(Mandatory = $false)]
    [string]$HostLog,

    [Parameter(Mandatory = $true)]
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$LogFiles = @(
    foreach ($entry in $LogFiles) {
        foreach ($path in ($entry -split ',')) {
            $trimmed = $path.Trim()
            if ($trimmed) { $trimmed }
        }
    }
)

function Get-OffsetRecord {
    param([Parameter(Mandatory = $true)][string]$Path)
    $resolved = if ([System.IO.Path]::IsPathRooted($Path)) {
        [System.IO.Path]::GetFullPath($Path)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $root $Path))
    }
    $item = Get-Item -LiteralPath $resolved -ErrorAction SilentlyContinue
    [ordered]@{
        path = $Path
        exists = [bool]$item
        bytes = if ($item) { [int64]$item.Length } else { [int64]0 }
    }
}

$record = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    files = @($LogFiles | ForEach-Object { Get-OffsetRecord -Path $_ })
    host = if ($HostLog) { Get-OffsetRecord -Path $HostLog } else { $null }
    qualityNote = 'Byte-offset baseline only: use as -BaselineJson for the first readiness checkpoint in an attempt to exclude stale reused log markers.'
}
$json = $record | ConvertTo-Json -Depth 6
$parent = Split-Path -Parent $OutputJson
if ($parent -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}
Set-Content -LiteralPath $OutputJson -Value $json -Encoding UTF8
$json
