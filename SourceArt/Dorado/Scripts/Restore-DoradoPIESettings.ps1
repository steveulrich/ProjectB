param([string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $ProjectRoot).Path
$project = Join-Path $root 'ProjectB.uproject'
$editors = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($project) })
if ($editors.Count) { throw 'Close the owned ProjectB editor normally before restoring its on-disk PIE settings.' }
$evidence = Join-Path $root 'Saved/Logs/dorado-reconstruction-20261030'
$baseline = Get-Content -LiteralPath (Join-Path $evidence 'pie-settings-baseline.json') -Raw | ConvertFrom-Json
$path = Join-Path $root 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
$before = [IO.File]::ReadAllText($path)
$after = $before
$changes = @()
foreach ($key in @('PlayNumberOfClients', 'RunUnderOneProcess', 'AdditionalServerGameOptions', 'bLaunchSeparateServer')) {
    $value = [string]$baseline.$key
    $pattern = '(?m)^' + [regex]::Escape($key) + '=([^\r\n]*)'
    $settingMatches = [regex]::Matches($after, $pattern)
    if ($settingMatches.Count -ne 1) { throw "Expected one unambiguous setting: $key" }
    $oldValue = $settingMatches[0].Groups[1].Value
    if ($oldValue -ne $value) {
        if ($key -eq 'AdditionalServerGameOptions' -and $oldValue -ne '?Experience=B_BW_Experience_CaptureTheRelic?SkipHeroSelection=1?NumBots=0?Hero=Argus?PrematchDuration=1?WarmupDuration=1?RoundDuration=1800?PointsToWin=9') {
            throw 'PIE options changed outside the Dorado fixture; preserve them and resolve the conflict.'
        }
        $after = [regex]::Replace($after, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{param($m) $key + '=' + $value})
        $changes += @{key=$key; before=$oldValue; after=$value}
    }
}
if ($after -ne $before) {
    $backup = Join-Path $evidence ('pie-settings-before-disk-restore-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.ini')
    Copy-Item -LiteralPath $path -Destination $backup
    [IO.File]::WriteAllText($path, $after, [Text.UTF8Encoding]::new($false))
}
$actual = [IO.File]::ReadAllText($path)
if ($actual -cne $after) { throw 'Settings write verification failed.' }
$report = @{generated_utc=[DateTime]::UtcNow.ToString('o'); path=$path; restored=$true; changes=$changes; scope='Only four recorded PIE settings in generated user INI; editor closed; other text preserved.'}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidence 'pie-settings-disk-restored.json') -Encoding utf8
$report | ConvertTo-Json -Depth 5
