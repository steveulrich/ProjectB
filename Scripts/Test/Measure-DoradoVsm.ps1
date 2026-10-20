[CmdletBinding()]
param(
    # Staged candidate root, e.g. Saved\StagedBuilds\LAN-20261018-01
    [Parameter(Mandatory)][string]$Candidate,
    [Parameter(Mandatory)][string]$OutputDir,
    # Short label for this configuration (used in file names).
    [Parameter(Mandatory)][string]$Profile,
    # Console variable overrides applied at device-profile priority (above DefaultEngine/Scalability ini),
    # e.g. 'r.Shadow.Virtual.ResolutionLodBiasDirectional=-1.5'. Empty = cooked configuration as shipped.
    [string[]]$CvarOverrides = @(),
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [int]$MaxFps = 60,
    [int]$CaptureFrames = 3600,
    [int]$TimeoutSeconds = 150,
    [int[]]$ScreenshotAtSeconds = @(40, 70),
    [string]$Hero = 'Korryn',
    [string]$Map = '/BreakawayCore/Maps/L_BW_Dorado',
    [string]$Experience = 'B_BW_Experience_CaptureTheRelic',
    [int]$NumBots = 3,
    [switch]$ValidateOnly,
    # Re-run the analysis on an existing CSV without launching the game.
    [string]$ReanalyzeCsv,
    # Optional: run the uncooked game through the editor binary (-game) instead of a staged candidate.
    [string]$Executable,
    [string[]]$ExtraArgs = @(),
    [string]$SavedDir,
    # In-engine HighResShot captures (needs bway.Test.DelayedExec; works without desktop capture access).
    [int[]]$HighResShotAtSeconds = @()
)

# Measures rendered Virtual Shadow Map overflow on one D3D12 packaged process. The engine logs each
# VSM overflow type only once per process, so this reads the per-frame VSM/OVERFLOW_FLAGS CSV stat
# (enabled by -csvCategories=VSM) and counts frames per overflow bit. It also captures the real game
# window for visible shadow-quality review. Single rendered process only; not a transport topology.

$ErrorActionPreference = 'Stop'
$exe = if ($Executable) { $Executable } else { Join-Path $Candidate 'Windows\ProjectB\Binaries\Win64\LyraGame.exe' }
if (-not $SavedDir) { $SavedDir = Join-Path $Candidate 'Windows\ProjectB\Saved' }
$csvDir = Join-Path $SavedDir 'Profiling\CSV'
$shotDir = Join-Path $SavedDir 'Screenshots'
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$log = Join-Path $OutputDir "vsm-$Profile.log"
$resultPath = Join-Path $OutputDir "vsm-$Profile.json"

if (-not (Test-Path -LiteralPath $exe)) { throw "Candidate executable not found: $exe" }
if (-not $ReanalyzeCsv -and @(Get-Process -Name LyraGame -ErrorAction SilentlyContinue).Count -gt 0) { throw 'A LyraGame process is already running.' }
if (-not $ReanalyzeCsv -and $Executable -and @(Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count -gt 0) { throw 'An UnrealEditor process is already running.' }

$url = "${Map}?Experience=${Experience}?SkipHeroSelection=1?NumBots=${NumBots}?Hero=${Hero}?PrematchDuration=1?WarmupDuration=1?RoundDuration=600?PointsToWin=9"
$launchArgs = @($url, '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-ForceRes', '-WinX=0', '-WinY=0', "-MaxFPS=$MaxFps",
    '-nosound', '-nosplash', '-unattended', '-nop4', "-abslog=`"$log`"",
    "-csvCaptureFrames=$CaptureFrames", '-csvNoProcessingThread', '-csvCategories=VSM')
$execList = @("t.MaxFPS $MaxFps") + @($HighResShotAtSeconds | ForEach-Object { "bway.Test.DelayedExec $_ HighResShot 1" })
$launchArgs = @($ExtraArgs) + $launchArgs + @("-ExecCmds=`"$($execList -join ', ')`"")
if ($CvarOverrides.Count -gt 0) { $launchArgs += "-dpcvars=$($CvarOverrides -join ',')" }

$result = [ordered]@{
    status = 'running'; profile = $Profile; candidate = $Candidate
    executableSha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    resolution = "${ResX}x${ResY}"; cvarOverrides = $CvarOverrides; arguments = $launchArgs
    log = $log; csv = $null; framesTotal = 0; framesWithVsmStats = 0
    overflowFrames = [ordered]@{}; overflowLogLines = @(); appliedCvarLines = @()
    screenshots = @(); screenshotErrors = @(); firstCausalError = $null; startedUtc = [DateTime]::UtcNow.ToString('o')
}
if ($ValidateOnly) { $result.status = 'validated'; $result | ConvertTo-Json -Depth 6; return }

if (-not $ReanalyzeCsv) {
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class BwayWin { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  public static IntPtr FindLargestVisible(uint target) {
    IntPtr best = IntPtr.Zero; long bestArea = 0;
    EnumWindows((h, p) => { uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid == target && IsWindowVisible(h)) { RECT r; GetWindowRect(h, out r); long a = (long)(r.R - r.L) * (r.B - r.T); if (a > bestArea) { bestArea = a; best = h; } }
      return true; }, IntPtr.Zero);
    return best; } }
'@ -ErrorAction SilentlyContinue

$before = @(Get-ChildItem -LiteralPath $csvDir -Filter *.csv -ErrorAction SilentlyContinue | ForEach-Object FullName)
$shotsBefore = @(Get-ChildItem -LiteralPath $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object FullName)
$lastShotAt = if ($HighResShotAtSeconds.Count) { ($HighResShotAtSeconds | Measure-Object -Maximum).Maximum + 10 } else { 0 }
$proc = Start-Process -FilePath $exe -ArgumentList $launchArgs -PassThru
$result.processId = $proc.Id
$watch = [Diagnostics.Stopwatch]::StartNew()
$pending = [System.Collections.Generic.Queue[int]]::new(); $ScreenshotAtSeconds | Sort-Object | ForEach-Object { $pending.Enqueue($_) }
try {
    while (-not $proc.HasExited -and $watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ($pending.Count -gt 0 -and $watch.Elapsed.TotalSeconds -ge $pending.Peek()) {
            $at = $pending.Dequeue(); $proc.Refresh()
            $rect = New-Object BwayWin+RECT
            try {
                $hwnd = [BwayWin]::FindLargestVisible([uint32]$proc.Id)
                if ($hwnd -eq [IntPtr]::Zero -or -not [BwayWin]::GetWindowRect($hwnd, [ref]$rect)) { throw 'no visible top-level game window' }
                $w = $rect.R - $rect.L; $h = $rect.B - $rect.T
                if ($w -le 0 -or $h -le 0 -or $rect.L -le -10000) { throw "window not visible (rect=$($rect.L),$($rect.T),$($rect.R),$($rect.B))" }
                [void][BwayWin]::SetForegroundWindow($hwnd); Start-Sleep -Milliseconds 300
                $bmp = New-Object System.Drawing.Bitmap $w, $h
                $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($rect.L, $rect.T, 0, 0, $bmp.Size)
                $shot = Join-Path $OutputDir "vsm-$Profile-t$at.png"; $bmp.Save($shot, [System.Drawing.Imaging.ImageFormat]::Png)
                $g.Dispose(); $bmp.Dispose(); $result.screenshots += $shot
            } catch {
                $result.screenshotErrors += "t${at}: $($_.Exception.Message)"
            }
        }
        $newCsv = @(Get-ChildItem -LiteralPath $csvDir -Filter *.csv -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName })
        if ($newCsv.Count -gt 0 -and $pending.Count -eq 0 -and $watch.Elapsed.TotalSeconds -ge $lastShotAt) {
            $tail = Get-Content -LiteralPath $newCsv[0].FullName -Tail 3 -ErrorAction SilentlyContinue
            if (($tail -join "`n") -match '\[HasHeaderRowAtEnd\]|EVENTS') { break }
        }
        Start-Sleep -Milliseconds 500
    }
} finally {
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue; $result.cleanup = 'owned process terminated after capture' }
    else { $result.cleanup = "exited $($proc.ExitCode)" }
}
Start-Sleep -Seconds 2
$csv = @(Get-ChildItem -LiteralPath $csvDir -Filter *.csv -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName } | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
$index = 0
foreach ($png in @(Get-ChildItem -LiteralPath $shotDir -Recurse -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $shotsBefore -notcontains $_.FullName } | Sort-Object LastWriteTime)) {
    $dest = Join-Path $OutputDir ("vsm-$Profile-highres-{0}.png" -f $index++); Copy-Item -LiteralPath $png.FullName -Destination $dest -Force; $result.screenshots += $dest
}
} else {
    $csv = @(Get-Item -LiteralPath $ReanalyzeCsv)
    $result.reanalyzed = $ReanalyzeCsv
}
try {
    if ($csv.Count -eq 0) { throw 'No new CSV capture was produced.' }
    $copy = Join-Path $OutputDir "vsm-$Profile.csv"
    if ($csv[0].FullName -ne (Resolve-Path -LiteralPath $OutputDir).Path + "\vsm-$Profile.csv") { Copy-Item -LiteralPath $csv[0].FullName -Destination $copy -Force }
    $result.csv = $copy
    $lines = Get-Content -LiteralPath $copy
    $header = $lines[0].Split(',')
    $col = [Array]::IndexOf($header, 'VSM/OVERFLOW_FLAGS')
    $instCol = [Array]::IndexOf($header, 'VSM/NON_NANITE_INSTANCES_TOTAL')
    $statCols = [ordered]@{}
    foreach ($name in 'VSM/REQUESTED_THIS_FRAME_PAGES','VSM/NON_NANITE_INSTANCES_DRAWN','VSM/NonNanitePreCullInstanceCount','VSM/FreePages','VSM/NANITE_INSTANCES_MAIN','FrameTime','GPU/Shadows','GPU/ShadowDepths') {
        $i = [Array]::IndexOf($header, $name); if ($i -ge 0) { $statCols[$name] = [ordered]@{ index = $i; max = 0.0; sum = 0.0; n = 0 } }
    }
    if ($col -lt 0) { throw "CSV has no VSM/OVERFLOW_FLAGS column (columns=$($header.Count))." }
    # Bit order follows VSM_STAT_OVERFLOW_FLAG_* in VirtualShadowMapDefinitions.ush (UE 5.8).
    $bits = [ordered]@{ markingJobQueue = 1; oppMaxLights = 2; pagePool = 4; visibleInstances = 8 }
    foreach ($k in $bits.Keys) { $result.overflowFrames[$k] = 0 }
    $maxInstances = 0
    foreach ($line in $lines | Select-Object -Skip 1) {
        $cells = $line.Split(',')
        if ($cells.Count -le $col -or $cells[0] -match '^\[') { continue }
        $raw = $cells[$col]; $value = 0.0
        if (-not [double]::TryParse($raw, [ref]$value)) { continue }   # trailing header/metadata rows
        $result.framesTotal++
        $value = [int]$value; $result.framesWithVsmStats++
        foreach ($k in $bits.Keys) { if ($value -band $bits[$k]) { $result.overflowFrames[$k]++ } }
        $inst = 0.0
        if ($instCol -ge 0 -and $cells.Count -gt $instCol -and [double]::TryParse($cells[$instCol], [ref]$inst)) { $maxInstances = [Math]::Max($maxInstances, [int]$inst) }
        foreach ($s in $statCols.Values) {
            $v = 0.0
            if ($cells.Count -gt $s.index -and [double]::TryParse($cells[$s.index], [ref]$v)) { $s.max = [Math]::Max($s.max, $v); $s.sum += $v; $s.n++ }
        }
    }
    $result.stats = [ordered]@{}
    foreach ($k in $statCols.Keys) { $s = $statCols[$k]; $result.stats[$k] = [ordered]@{ max = [Math]::Round($s.max, 3); mean = if ($s.n) { [Math]::Round($s.sum / $s.n, 3) } else { $null } } }
    $result.maxNonNaniteInstancesTotal = $maxInstances
    if (Test-Path -LiteralPath $log) {
        $result.overflowLogLines = @(Select-String -LiteralPath $log -Pattern '\[VSM\]' | ForEach-Object { $_.Line })
        $result.appliedCvarLines = @(Select-String -LiteralPath $log -Pattern 'r\.Shadow\.Virtual\.(ResolutionLodBias|Clipmap|MarkCoarse|NonNanite)|r\.Shadow\.RadiusThreshold' | Select-Object -First 30 | ForEach-Object { $_.Line })
        $result.playingObserved = [bool](Select-String -LiteralPath $log -Pattern 'Bringing World /BreakawayCore/Maps/L_BW_Dorado' -List)
    }
    if ($result.framesWithVsmStats -lt 300) { throw "Too few frames with VSM stats ($($result.framesWithVsmStats))." }
    $result.status = 'measured'
} catch {
    $result.status = 'failed'; $result.firstCausalError = $_.Exception.Message
}
$result.endedUtc = [DateTime]::UtcNow.ToString('o')
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resultPath -Encoding UTF8
[ordered]@{ profile = $Profile; status = $result.status; frames = $result.framesWithVsmStats; overflow = $result.overflowFrames; vsmLogLines = @($result.overflowLogLines).Count; shots = @($result.screenshots).Count; error = $result.firstCausalError } | ConvertTo-Json -Compress
