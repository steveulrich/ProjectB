<#
.SYNOPSIS
    Owned 4-world 1080p PIE lifecycle and Goal 6 Match HUD runtime + screenshot audit.
.DESCRIPTION
    Launches ProjectB's source-built editor, configures 4-world same-process ListenServer PIE
    at 1920x1080 via UE-MCP set_property on Default__LevelEditorPlaySettings, loads
    L_BW_HeroSelect_Staging, starts 4-world PIE, locks all 4 human heroes via
    bway.Test.AutoLockAllHeroes to travel into L_BW_Dorado (Demo rules + 4 bots = 8 players),
    dispatches guard=0 bway.Test.Goal6RuntimeAudit, collects structured BwayGoal6Probe logs
    and 1080p Host/Client PNG screenshots through Rematch travel back to L_BW_HeroSelect_Staging,
    stops PIE, closes the owned editor PID cleanly, and restores EditorPerProjectUserSettings.ini.
#>
[CmdletBinding()]
param(
    [string]$Token = 'pie-goal6-02',
    [string]$EvidenceDir = 'Saved/Logs/goal6-hud-20261029',
    [string]$EngineRoot = 'E:\Github\UnrealEngine',
    [int]$BridgePort = 9877,
    [int]$AuditTimeoutSeconds = 130
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$ResolvedEvidence = [IO.Path]::GetFullPath((Join-Path $ProjectRoot $EvidenceDir))
if (-not (Test-Path -LiteralPath $ResolvedEvidence)) {
    New-Item -ItemType Directory -Path $ResolvedEvidence -Force | Out-Null
}

$EditorLog = Join-Path $ResolvedEvidence "editor-$Token.log"
$LaunchJson = Join-Path $ResolvedEvidence "editor-$Token-launch.json"
$SummaryJson = Join-Path $ResolvedEvidence "$Token-summary.json"
$ZenDataPath = Join-Path $ResolvedEvidence 'ZenData'
$EditorIni = Join-Path $ProjectRoot 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
$IniBackup = Join-Path $ResolvedEvidence "EditorPerProjectUserSettings.$Token.before.ini"

$preProcs = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
    $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|LyraGame)\.exe$'
})
if ($preProcs.Count -gt 0) {
    throw "Refusing to launch editor while Unreal processes exist: $(($preProcs | ForEach-Object { "$($_.Name):$($_.ProcessId)" }) -join ', ')"
}

$iniExisted = Test-Path -LiteralPath $EditorIni -PathType Leaf
if ($iniExisted) {
    Copy-Item -LiteralPath $EditorIni -Destination $IniBackup -Force
}

function Invoke-McpRequest {
    param(
        [Parameter(Mandatory)][string]$Method,
        [hashtable]$Params = @{},
        [int]$Id = 1,
        [int]$TimeoutSec = 60,
        [switch]$AllowError
    )
    $ws = [System.Net.WebSockets.ClientWebSocket]::new()
    $cts = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds($TimeoutSec))
    try {
        $uri = [Uri]::new("ws://127.0.0.1:$BridgePort")
        $ws.ConnectAsync($uri, $cts.Token).GetAwaiter().GetResult()

        $payload = @{
            jsonrpc = '2.0'
            id      = $Id
            method  = $Method
            params  = $Params
        } | ConvertTo-Json -Depth 10 -Compress

        $sendBytes = [Text.Encoding]::UTF8.GetBytes($payload)
        $sendSeg = [ArraySegment[byte]]::new($sendBytes)
        $ws.SendAsync($sendSeg, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $cts.Token).GetAwaiter().GetResult()

        $ms = [IO.MemoryStream]::new()
        $buf = New-Object byte[] 65536
        do {
            $recvSeg = [ArraySegment[byte]]::new($buf)
            $res = $ws.ReceiveAsync($recvSeg, $cts.Token).GetAwaiter().GetResult()
            if ($res.Count -gt 0) {
                $ms.Write($buf, 0, $res.Count)
            }
        } while (-not $res.EndOfMessage)

        $respText = [Text.Encoding]::UTF8.GetString($ms.ToArray())
        $parsed = $respText | ConvertFrom-Json
        if (-not $AllowError) {
            if ($null -ne $parsed.error) {
                throw "MCP $Method failed: $($parsed.error | ConvertTo-Json -Compress)"
            }
            if ($null -ne $parsed.result -and $parsed.result.PSObject.Properties['success'] -and (-not [bool]$parsed.result.success)) {
                throw "MCP $Method returned success=false: $($parsed.result | ConvertTo-Json -Compress)"
            }
        }
        return $parsed
    } finally {
        # FMCPBridgeServer does not echo WebSocket Close (0x88) frames; abort TCP socket cleanly instead of blocking in CloseAsync.
        try { $ws.Abort() } catch {}
        $cts.Dispose()
        $ws.Dispose()
    }
}

$editorPid = $null
$auditPassed = $false
$auditError = $null
$iniRestored = $false
$cleanShutdown = $false

try {
    $launchOut = & (Join-Path $PSScriptRoot 'Start-ProjectBEditor.ps1') `
        -EngineRoot $EngineRoot `
        -LogFile $EditorLog `
        -ZenDataPath $ZenDataPath `
        -CreateZenDataPath `
        -SkipPlatformValidation `
        -ForceMemoryDDC `
        -OutputJson $LaunchJson | ConvertFrom-Json

    $editorPid = [int]$launchOut.processId
    $editorProc = Get-Process -Id $editorPid -ErrorAction Stop

    # Wait for [UE-MCP] Editor ready — accepting requests
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $bridgeReady = $false
    while ($sw.Elapsed.TotalSeconds -lt 150) {
        if ($editorProc.HasExited) {
            throw "UnrealEditor (PID $editorPid) exited early during startup with code $($editorProc.ExitCode)."
        }
        if (Test-Path -LiteralPath $EditorLog) {
            $raw = Get-Content -LiteralPath $EditorLog -Raw -ErrorAction SilentlyContinue
            if ($raw -match '\[UE-MCP\] Editor ready') {
                $bridgeReady = $true
                break
            }
        }
        Start-Sleep -Seconds 1
    }
    if (-not $bridgeReady) {
        throw "Timed out waiting for [UE-MCP] Editor ready in $EditorLog"
    }
    Start-Sleep -Seconds 2

    # Configure 4-world 1080p ListenServer PIE via native UE-MCP set_property on Default__LevelEditorPlaySettings
    $playSettingsPath = '/Script/UnrealEd.Default__LevelEditorPlaySettings'
    $playProps = @(
        @{ propertyName = 'PlayNetMode';                 value = 'PIE_ListenServer' }
        @{ propertyName = 'RunUnderOneProcess';          value = $true }
        @{ propertyName = 'bLaunchSeparateServer';       value = $false }
        @{ propertyName = 'PlayNumberOfClients';         value = 4 }
        @{ propertyName = 'NewWindowWidth';              value = 1920 }
        @{ propertyName = 'NewWindowHeight';             value = 1080 }
        @{ propertyName = 'ClientWindowWidth';           value = 1920 }
        @{ propertyName = 'ClientWindowHeight';          value = 1080 }
        @{ propertyName = 'CenterNewWindow';             value = $false }
        @{ propertyName = 'AdditionalServerGameOptions'; value = '?MatchRules=Demo?NumBots=4?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic' }
    )
    $reqId = 10
    foreach ($prop in $playProps) {
        $null = Invoke-McpRequest -Method 'set_property' -Params @{
            path         = $playSettingsPath
            propertyName = $prop.propertyName
            value        = $prop.value
        } -Id ($reqId++) -TimeoutSec 20
    }

    # Load L_BW_HeroSelect_Staging without retaining UObject wrappers
    $pyLoadStaging = @"
import unreal

def _load_staging():
    world = None
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map('/BreakawayCore/Maps/L_BW_HeroSelect_Staging')
        return str(world.get_path_name()) if world else 'None'
    finally:
        world = None

print('LOADED_STAGING:' + _load_staging())
"@
    $loadResp = Invoke-McpRequest -Method 'execute_python' -Params @{ code = $pyLoadStaging } -Id ($reqId++) -TimeoutSec 60
    if (($loadResp | ConvertTo-Json -Depth 6 -Compress) -notmatch 'LOADED_STAGING:/BreakawayCore/Maps/L_BW_HeroSelect_Staging') {
        throw "Failed to load L_BW_HeroSelect_Staging: $($loadResp | ConvertTo-Json -Depth 6 -Compress)"
    }

    # Start 4-world PIE
    $null = Invoke-McpRequest -Method 'pie_control' -Params @{ action = 'start' } -Id ($reqId++) -TimeoutSec 60

    # Wait until 4 PIE worlds exist
    $pyCheckWorlds = @"
import unreal
def _count_pie():
    worlds = None
    try:
        worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
        return len(worlds) if worlds else 0
    finally:
        worlds = None
print('PIE_WORLDS=' + str(_count_pie()))
"@
    $pieSw = [Diagnostics.Stopwatch]::StartNew()
    $fourWorldsReady = $false
    while ($pieSw.Elapsed.TotalSeconds -lt 45) {
        Start-Sleep -Seconds 2
        $checkResp = Invoke-McpRequest -Method 'execute_python' -Params @{ code = $pyCheckWorlds } -Id ($reqId++) -TimeoutSec 20
        $checkStr = ($checkResp | ConvertTo-Json -Depth 6 -Compress)
        if ($checkStr -match 'PIE_WORLDS=4') {
            $fourWorldsReady = $true
            break
        }
    }
    if (-not $fourWorldsReady) {
        throw "4 PIE worlds did not initialize within 45s."
    }

    # Give all 4 human controllers 4s to register in HeroSelect_Staging, then AutoLock all 4 heroes to trigger seamless travel to L_BW_Dorado!
    Start-Sleep -Seconds 4
    $null = Invoke-McpRequest -Method 'execute_command' -Params @{ command = 'bway.Test.AutoLockAllHeroes' } -Id ($reqId++) -TimeoutSec 20

    # Wait up to 35s for Dorado PIE world to come up before launching the 85s Goal6RuntimeAudit ticker
    $doradoSw = [Diagnostics.Stopwatch]::StartNew()
    $doradoReady = $false
    $retriedAutoLock = $false
    while ($doradoSw.Elapsed.TotalSeconds -lt 35) {
        if ($editorProc.HasExited) {
            throw "UnrealEditor (PID $editorPid) exited during travel to Dorado with code $($editorProc.ExitCode)."
        }
        $logText = Get-Content -LiteralPath $EditorLog -Raw -ErrorAction SilentlyContinue
        if ($logText -match 'Bringing World /BreakawayCore/Maps/UEDPIE_0_L_BW_Dorado\.L_BW_Dorado up for play') {
            $doradoReady = $true
            break
        }
        if (-not $retriedAutoLock -and $doradoSw.Elapsed.TotalSeconds -gt 6) {
            $retriedAutoLock = $true
            $null = Invoke-McpRequest -Method 'execute_command' -Params @{ command = 'bway.Test.AutoLockAllHeroes' } -Id ($reqId++) -TimeoutSec 20 -AllowError
        }
        Start-Sleep -Seconds 1
    }
    if (-not $doradoReady) {
        throw "Timed out waiting for UEDPIE_0_L_BW_Dorado after AutoLockAllHeroes."
    }

    Start-Sleep -Seconds 2
    $null = Invoke-McpRequest -Method 'execute_command' -Params @{ command = "bway.Test.Goal6RuntimeAudit $Token" } -Id ($reqId++) -TimeoutSec 20

    # Poll editor log for BwayGoal6Audit: PASS or FAIL
    $auditSw = [Diagnostics.Stopwatch]::StartNew()
    while ($auditSw.Elapsed.TotalSeconds -lt $AuditTimeoutSeconds) {
        if ($editorProc.HasExited) {
            throw "UnrealEditor (PID $editorPid) exited unexpectedly during PIE audit with code $($editorProc.ExitCode)."
        }
        $logText = Get-Content -LiteralPath $EditorLog -Raw -ErrorAction SilentlyContinue
        if ($logText -match "BwayGoal6Audit: PASS token=$([regex]::Escape($Token))([^\r\n]*)") {
            $auditPassed = $true
            Start-Sleep -Seconds 2
            break
        }
        if ($logText -match "BwayGoal6Audit: FAIL token=$([regex]::Escape($Token))([^\r\n]*)") {
            throw "Goal6RuntimeAudit reported FAIL: $($Matches[0])"
        }
        Start-Sleep -Seconds 1
    }
    if (-not $auditPassed) {
        throw "Timed out after ${AuditTimeoutSeconds}s waiting for BwayGoal6Audit: PASS token=$Token"
    }
} catch {
    $auditError = $_.Exception.Message
} finally {
    if ($editorPid) {
        $liveEditor = Get-Process -Id $editorPid -ErrorAction SilentlyContinue
        if ($liveEditor -and -not $liveEditor.HasExited) {
            try { $null = Invoke-McpRequest -Method 'pie_control' -Params @{ action = 'stop' } -Id 90 -TimeoutSec 15 -AllowError } catch {}
            Start-Sleep -Seconds 3
            try { $null = Invoke-McpRequest -Method 'execute_command' -Params @{ command = 'QUIT_EDITOR' } -Id 91 -TimeoutSec 10 -AllowError } catch {}
            if (-not $liveEditor.WaitForExit(20000)) {
                try { [void]$liveEditor.CloseMainWindow() } catch {}
                if (-not $liveEditor.WaitForExit(10000)) {
                    try { $liveEditor.Kill(); [void]$liveEditor.WaitForExit(5000) } catch {}
                }
            }
            $cleanShutdown = $liveEditor.HasExited
        } else {
            $cleanShutdown = $true
        }
    }

    $remaining = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|LyraGame)\.exe$'
    })
    if ($remaining.Count -eq 0 -and $iniExisted -and (Test-Path -LiteralPath $IniBackup)) {
        Copy-Item -LiteralPath $IniBackup -Destination $EditorIni -Force
        $iniRestored = ((Get-FileHash -LiteralPath $EditorIni).Hash -eq (Get-FileHash -LiteralPath $IniBackup).Hash)
    }
}

$auditLines = @()
$probeLines = @()
$screenshots = @()
if (Test-Path -LiteralPath $EditorLog) {
    $auditLines = @(Select-String -LiteralPath $EditorLog -Pattern 'BwayGoal6Audit:' | ForEach-Object { $_.Line })
    $probeLines = @(Select-String -LiteralPath $EditorLog -Pattern 'BwayGoal6Probe:' | ForEach-Object { $_.Line })
}
$shotDir = Join-Path $ResolvedEvidence $Token
if (Test-Path -LiteralPath $shotDir) {
    $screenshots = @(Get-ChildItem -LiteralPath $shotDir -Filter '*.png' | Sort-Object Name | ForEach-Object {
        [ordered]@{
            name  = $_.Name
            path  = $_.FullName
            bytes = $_.Length
        }
    })
}

$summary = [ordered]@{
    generatedUtc    = [DateTime]::UtcNow.ToString('o')
    token           = $Token
    status          = if ($auditPassed -and -not $auditError -and $cleanShutdown -and (-not $iniExisted -or $iniRestored)) { 'passed' } else { 'failed' }
    editorPid       = $editorPid
    cleanShutdown   = $cleanShutdown
    iniRestored     = (-not $iniExisted -or $iniRestored)
    error           = $auditError
    editorLog       = $EditorLog
    auditLineCount  = $auditLines.Count
    probeLineCount  = $probeLines.Count
    screenshotCount = $screenshots.Count
    auditLines      = $auditLines
    probeLines      = $probeLines
    screenshots     = $screenshots
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $SummaryJson -Encoding UTF8
$summary | ConvertTo-Json -Depth 6
if ($summary.status -ne 'passed') {
    exit 1
}
