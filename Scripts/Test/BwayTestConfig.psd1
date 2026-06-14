@{
    # ProjectB (Lyra fork) — shared config for agentic test tiers.
    ProjectRoot   = 'E:\Unreal Projects\ProjectB'
    ProjectFile   = 'E:\Unreal Projects\ProjectB\ProjectB.uproject'
    EngineAssociation = '{A9367F94-430F-554F-8376-B6843DA48377}'

    # Lyra keeps native target names; the game binary is named after the .uproject.
    EditorTarget  = 'LyraEditor'
    GameTarget    = 'LyraGame'
    GameBinary    = 'ProjectB.exe'

    Platform      = 'Win64'
    Configuration = 'Development'

    # Tier 2 — standalone smoke (requires a pre-built packaged game binary).
    SmokeMapUrl = 'L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=3&PointsToWin=1'
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
