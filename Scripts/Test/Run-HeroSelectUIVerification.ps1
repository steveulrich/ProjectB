<#
.SYNOPSIS
    Owned 1080p PIE lifecycle for importing the 61 Hero Selection UI textures, populating and
    saving WBP_BW_HeroSlot, W_HeroSelection, and WBP_BW_HeroSelect, and capturing live PIE
    verification screenshots at 1920x1080, 1024x573 (reference dimensions), interactive ability
    and hero switch states, and 1280x800 (16:10 second aspect ratio).
#>
[CmdletBinding()]
param(
    [string]$Token = 'heroselect-ui-01',
    [string]$EvidenceDir = 'Saved/Logs/heroselect-ui-verify',
    [string]$EngineRoot = 'E:\Github\UnrealEngine',
    [int]$BridgePort = 9877,
    [switch]$SkipImport
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
$ImportStatusJson = Join-Path $ProjectRoot 'Saved/Logs/heroselect-ui-import.json'
$ZenDataPath = Join-Path $ResolvedEvidence 'ZenData'
$EditorIni = Join-Path $ProjectRoot 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
$IniBackup = Join-Path $ResolvedEvidence "EditorPerProjectUserSettings.$Token.before.ini"

$waitSw = [Diagnostics.Stopwatch]::StartNew()
while ($waitSw.Elapsed.TotalSeconds -lt 120) {
    $preProcs = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|LyraGame)\.exe$'
    })
    if ($preProcs.Count -eq 0) {
        break
    }
    Start-Sleep -Seconds 2
}
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
        try { $ws.Abort() } catch {}
        $cts.Dispose()
        $ws.Dispose()
    }
}

$editorPid = $null
$verifyPassed = $false
$verifyError = $null
$importResult = $null
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

    # Wait for [UE-MCP] Editor ready
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

    $reqId = 10

    # Step 1: Import 61 Hero Select textures and populate/save WBP_BW_HeroSlot, W_HeroSelection, WBP_BW_HeroSelect
    if (-not $SkipImport) {
        if (Test-Path -LiteralPath $ImportStatusJson) {
            Remove-Item -LiteralPath $ImportStatusJson -Force -ErrorAction SilentlyContinue
        }
        $pyImport = @"
import sys, importlib
script_dir = r'E:/Unreal Projects/ProjectB/Scripts/Editor'
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)
import ImportHeroSelectAssets
importlib.reload(ImportHeroSelectAssets)
print(ImportHeroSelectAssets.schedule_import_job())
"@
        $null = Invoke-McpRequest -Method 'execute_python' -Params @{ code = $pyImport } -Id ($reqId++) -TimeoutSec 30

        $impSw = [Diagnostics.Stopwatch]::StartNew()
        $impDone = $false
        while ($impSw.Elapsed.TotalSeconds -lt 180) {
            if ($editorProc.HasExited) {
                throw "UnrealEditor exited during texture import with code $($editorProc.ExitCode)."
            }
            if (Test-Path -LiteralPath $ImportStatusJson) {
                try {
                    $st = Get-Content -LiteralPath $ImportStatusJson -Raw | ConvertFrom-Json
                    if ($st.status -eq 'complete') {
                        $importResult = $st.result
                        $impDone = $true
                        break
                    } elseif ($st.status -eq 'failed') {
                        throw "Texture import failed: $($st.error)"
                    }
                } catch {
                    if ($_.Exception.Message -match '^Texture import failed:') { throw }
                }
            }
            Start-Sleep -Seconds 1
        }
        if (-not $impDone) {
            throw "Timed out waiting for ImportHeroSelectAssets job completion."
        }
    }

    # Step 2: Configure 1-player 1920x1080 Standalone PIE
    $playSettingsPath = '/Script/UnrealEd.Default__LevelEditorPlaySettings'
    $playProps = @(
        @{ propertyName = 'PlayNetMode';                 value = 'PIE_Standalone' }
        @{ propertyName = 'RunUnderOneProcess';          value = $true }
        @{ propertyName = 'bLaunchSeparateServer';       value = $false }
        @{ propertyName = 'PlayNumberOfClients';         value = 1 }
        @{ propertyName = 'NewWindowWidth';              value = 1920 }
        @{ propertyName = 'NewWindowHeight';             value = 1080 }
        @{ propertyName = 'ClientWindowWidth';           value = 1920 }
        @{ propertyName = 'ClientWindowHeight';          value = 1080 }
        @{ propertyName = 'CenterNewWindow';             value = $false }
    )
    foreach ($prop in $playProps) {
        $null = Invoke-McpRequest -Method 'set_property' -Params @{
            path         = $playSettingsPath
            propertyName = $prop.propertyName
            value        = $prop.value
        } -Id ($reqId++) -TimeoutSec 20
    }

    # Step 3: Load L_LyraFrontEnd
    $pyLoadFrontEnd = @"
import unreal
def _load_fe():
    world = None
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/System/FrontEnd/Maps/L_LyraFrontEnd')
        return str(world.get_path_name()) if world else 'None'
    finally:
        world = None
print('LOADED_FE:' + _load_fe())
"@
    $loadResp = Invoke-McpRequest -Method 'execute_python' -Params @{ code = $pyLoadFrontEnd } -Id ($reqId++) -TimeoutSec 60
    if (($loadResp | ConvertTo-Json -Depth 6 -Compress) -notmatch 'LOADED_FE:/Game/System/FrontEnd/Maps/L_LyraFrontEnd') {
        throw "Failed to load L_LyraFrontEnd: $($loadResp | ConvertTo-Json -Depth 6 -Compress)"
    }

    # Step 4: Start PIE in New Editor Window (1920x1080)
    $null = Invoke-McpRequest -Method 'pie_control' -Params @{ action = 'start' } -Id ($reqId++) -TimeoutSec 60

    $pieSw = [Diagnostics.Stopwatch]::StartNew()
    $worldReady = $false
    while ($pieSw.Elapsed.TotalSeconds -lt 45) {
        if ($editorProc.HasExited) {
            throw "UnrealEditor exited while starting PIE with code $($editorProc.ExitCode)."
        }
        $logText = Get-Content -LiteralPath $EditorLog -Raw -ErrorAction SilentlyContinue
        if ($logText -match 'Bringing World /Game/System/FrontEnd/Maps/UEDPIE_0_L_LyraFrontEnd\.L_LyraFrontEnd up for play') {
            $worldReady = $true
            break
        }
        Start-Sleep -Seconds 1
    }
    if (-not $worldReady) {
        throw "Timed out waiting for UEDPIE_0_L_LyraFrontEnd to come up for play."
    }

    Start-Sleep -Seconds 2
    $null = Invoke-McpRequest -Method 'execute_command' -Params @{ command = 'DisableAllScreenMessages' } -Id ($reqId++) -TimeoutSec 20 -AllowError

    # Step 5: Dispatch guard=0 bway.Test.VerifyHeroSelectUI
    $null = Invoke-McpRequest -Method 'execute_command' -Params @{ command = 'bway.Test.VerifyHeroSelectUI' } -Id ($reqId++) -TimeoutSec 20

    $verSw = [Diagnostics.Stopwatch]::StartNew()
    while ($verSw.Elapsed.TotalSeconds -lt 45) {
        if ($editorProc.HasExited) {
            throw "UnrealEditor exited during BwayHeroSelectVerify with code $($editorProc.ExitCode)."
        }
        $logText = Get-Content -LiteralPath $EditorLog -Raw -ErrorAction SilentlyContinue
        if ($logText -match 'BwayHeroSelectVerify: PASS') {
            $verifyPassed = $true
            Start-Sleep -Seconds 1
            break
        }
        if ($logText -match 'BwayHeroSelectVerify: FAIL([^\r\n]*)') {
            throw "BwayHeroSelectVerify failed: $($Matches[0])"
        }
        Start-Sleep -Seconds 1
    }
    if (-not $verifyPassed) {
        throw "Timed out waiting for BwayHeroSelectVerify: PASS in $EditorLog"
    }
} catch {
    $verifyError = $_.Exception.Message
} finally {
    if ($editorPid) {
        $liveEditor = Get-Process -Id $editorPid -ErrorAction SilentlyContinue
        if ($liveEditor -and -not $liveEditor.HasExited) {
            try { $null = Invoke-McpRequest -Method 'pie_control' -Params @{ action = 'stop' } -Id 90 -TimeoutSec 15 -AllowError } catch {}
            Start-Sleep -Seconds 2
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

$builderLines = @()
$verifyLines = @()
if (Test-Path -LiteralPath $EditorLog) {
    $builderLines = @(Select-String -LiteralPath $EditorLog -Pattern 'BwayHeroSelectEditorBuilder:' | ForEach-Object { $_.Line })
    $verifyLines = @(Select-String -LiteralPath $EditorLog -Pattern 'BwayHeroSelectVerify:' | ForEach-Object { $_.Line })
}

$screenshots = @(Get-ChildItem -LiteralPath $ResolvedEvidence -Filter 'heroselect-*.png' -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
    [ordered]@{
        name  = $_.Name
        path  = $_.FullName
        bytes = $_.Length
    }
})

$summary = [ordered]@{
    generatedUtc    = [DateTime]::UtcNow.ToString('o')
    token           = $Token
    status          = if ($verifyPassed -and -not $verifyError -and $cleanShutdown) { 'passed' } else { 'failed' }
    editorPid       = $editorPid
    cleanShutdown   = $cleanShutdown
    iniRestored     = (-not $iniExisted -or $iniRestored)
    importResult    = $importResult
    error           = $verifyError
    editorLog       = $EditorLog
    builderLines    = $builderLines
    verifyLines     = $verifyLines
    screenshots     = $screenshots
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $SummaryJson -Encoding UTF8
$summary | ConvertTo-Json -Depth 6
if ($summary.status -ne 'passed') {
    exit 1
}
