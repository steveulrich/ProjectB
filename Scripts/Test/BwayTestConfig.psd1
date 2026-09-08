@{
    # ProjectB (Lyra fork) — shared config for agentic test tiers.
    ProjectRoot   = 'E:\Unreal Projects\ProjectB'
    ProjectFile   = 'E:\Unreal Projects\ProjectB\ProjectB.uproject'
    EngineAssociation = '{A9367F94-430F-554F-8376-B6843DA48377}'

    # Verified source-build staged executable (including Windows bootstrap).
    EditorTarget  = 'LyraEditor'
    GameTarget    = 'LyraGame'
    GameBinary    = 'LyraGame.exe'

    Platform      = 'Win64'
    Configuration = 'Development'

    # Tier 2 — standalone smoke (requires a pre-built packaged game binary).
    SmokeMapUrl = 'L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=3&PointsToWin=1'
    # Repeatable settings; bot outcomes and completion time are not deterministic.
    TestProfiles = @{
        ShortMatch = @{
            Description = 'One-round packaged smoke with three bots'
            MapUrl = 'L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=3&PointsToWin=1&WarmupDuration=5&PostRoundDuration=3&RoundDuration=90'
            RequiredPatterns = @('B_BW_Experience_Dev', 'BwayMatchFlow: Resolved')
            ForbiddenPatterns = @('B_LyraDefaultExperience', 'Ensure condition failed', 'Fatal error', 'Unhandled Exception', 'Assertion failed')
        }
        FrontendLAN = @{
            Description = 'Host startup through the LAN playlist; run clients under separate evidence'
            MapUrl = '/Game/System/FrontEnd/Maps/L_LyraFrontEnd'
            RequiredPatterns = @('B_BW_Experience_CaptureTheRelic', 'BwayMatchFlow: Resolved')
            ForbiddenPatterns = @('B_LyraDefaultExperience', 'Ensure condition failed', 'Fatal error', 'Unhandled Exception', 'Assertion failed')
        }
    }
    SmokeLogPatternsRequired = @(
        'B_BW_Experience_Dev'
        'BwayMatchFlow: Resolved'
    )
    SmokeLogPatternsForbidden = @(
        'B_LyraDefaultExperience'
    )

    # Tier 3 — CQTest automation filter prefix (editor, unattended).
    CQTestFilter = 'Breakaway.CoreLoop'

    # Fallback when registry / env resolution fails (source build on this machine).
    DefaultEngineRoot = 'E:\Github\UnrealEngine'
}
