"""Run inside Unreal Editor Python after the native ResultsSnapshot fixture.

Read-only verification of actual results widgets on every same-process PIE peer.
Call verify_results_snapshot(expected_peers=4). See Scripts/Test/README.md.
UObject references stay inside functions so PIE teardown can collect its worlds.
"""

import json
from pathlib import Path

import unreal


STAT_FIELDS = (
    "Kills", "Deaths", "Assists", "GoldEarned", "DamageDealt", "HealingDone",
    "RelicScores", "ForcedFumbles", "Interceptions", "BuildablesDestroyed",
)


def _library(path):
    return unreal.get_default_object(unreal.load_class(None, path))


def _stats(value):
    return {field: value.get_editor_property(field) for field in STAT_FIELDS}


def verify_results_snapshot(expected_peers=4):
    worlds = _library("/Script/EditorScriptingUtilities.EditorLevelLibrary").call_method("GetPIEWorlds", (False,))
    widget_library = _library("/Script/UMG.WidgetBlueprintLibrary")
    screen_class = unreal.load_class(None, "/Script/BreakawayCoreRuntime.BwayResultsScreenWidget")
    breakdown_class = unreal.load_class(None, "/Script/BreakawayCoreRuntime.BwayMatchBreakdownWidget")
    registry = _library("/Script/BreakawayCoreRuntime.BwayHeroRegistry")
    report = {"scope": "Synthetic results RPC and widget data in same-process PIE; not natural match or physical input acceptance.", "peers": []}
    canonical = None
    for world in worlds:
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc:
            continue
        screens = widget_library.call_method("GetAllWidgetsOfClass", (world, screen_class, False))
        assert len(screens) == 1, "Each peer must own exactly one results orchestrator."
        summary = screens[0].call_method("GetPostMatchSummary")
        columns = list(summary.get_editor_property("PlayerColumns"))
        ids = [column.get_editor_property("PlayerId") for column in columns]
        assert len(ids) == 8 and len(set(ids)) == 8, "Expected eight distinct player IDs."
        viewer_id = pc.get_editor_property("PlayerState").call_method("GetPlayerId")
        local = [column for column in columns if column.get_editor_property("bIsLocalPlayer")]
        assert len(local) == 1 and local[0].get_editor_property("PlayerId") == viewer_id
        viewer_team = local[0].get_editor_property("GameTeamIndex")
        assert summary.get_editor_property("LocalPlayerTeamIndex") == viewer_team
        assert summary.get_editor_property("bLocalPlayerWon") == (viewer_team == summary.get_editor_property("WinningTeam"))
        breakdowns = widget_library.call_method("GetAllWidgetsOfClass", (world, breakdown_class, False))
        assert len(breakdowns) == 1, "Wait for the interstitial to advance to one breakdown."
        displayed = list(breakdowns[0].call_method("GetPlayerColumns"))
        assert sorted(column.get_editor_property("PlayerId") for column in displayed) == sorted(ids)
        for column in displayed:
            team = column.get_editor_property("GameTeamIndex")
            assert column.get_editor_property("DisplayColumnIndex") == (0 if team == viewer_team else 1)
            original = next(value for value in columns if value.get_editor_property("PlayerId") == column.get_editor_property("PlayerId"))
            assert _stats(column.get_editor_property("Stats")) == _stats(original.get_editor_property("Stats"))
        winner_id = max(ids)
        mvp = [column for column in columns if column.get_editor_property("bIsMVP")]
        assert len(mvp) == 1 and mvp[0].get_editor_property("PlayerId") == winner_id
        assert mvp[0].get_editor_property("MVPScore") == 18.0
        assert _stats(mvp[0].get_editor_property("Stats"))["Kills"] == 10
        assert _stats(mvp[0].get_editor_property("Stats"))["Deaths"] == 4
        players = unreal.GameplayStatics.get_game_state(world).get_editor_property("PlayerArray")
        live_winner = next(player for player in players if player.call_method("GetPlayerId") == winner_id)
        assert live_winner.call_method("GetMatchStatsSnapshot").get_editor_property("Kills") == 0, "Wait for live reset to replicate."
        rows = []
        for column in columns:
            hero = registry.call_method("GetHeroDataById", (column.get_editor_property("HeroId"),))
            assert hero and column.get_editor_property("HeroPortrait") == hero.get_editor_property("Portrait")
            rows.append({"id": column.get_editor_property("PlayerId"), "team": column.get_editor_property("GameTeamIndex"),
                         "name": str(column.get_editor_property("PlayerName")), "hero": str(column.get_editor_property("HeroName")),
                         "stats": _stats(column.get_editor_property("Stats"))})
        assert all(row["name"] == "Duplicate player with a long display name" for row in rows)
        teams = []
        for team in (0, 1):
            aggregate = _stats(summary.get_editor_property("Team" + str(team) + "MatchStats").get_editor_property("Stats"))
            assert aggregate == {field: sum(row["stats"][field] for row in rows if row["team"] == team) for field in STAT_FIELDS}
            teams.append(aggregate)
        received = {"columns": sorted(rows, key=lambda row: row["id"]), "teams": teams,
                    "scores": [summary.get_editor_property("Team0Score"), summary.get_editor_property("Team1Score")],
                    "winningTeam": summary.get_editor_property("WinningTeam"), "rounds": summary.get_editor_property("TotalRounds"),
                    "pointsToWin": summary.get_editor_property("PointsToWin"), "duration": summary.get_editor_property("InterstitialDurationSeconds")}
        if canonical is None:
            canonical = received
        assert received == canonical, "Peers disagree on the captured final results."
        report["peers"].append({"world": world.get_path_name(), "authority": pc.has_authority(), "localPlayerId": viewer_id,
                                "viewerTeam": viewer_team, "mvpId": winner_id, "capturedKills": 10, "liveKills": 0, "portraits": 8})
    assert len(report["peers"]) == expected_peers, "Unexpected PIE peer count."
    assert sum(peer["authority"] for peer in report["peers"]) == 1
    report["snapshot"] = canonical
    report["passed"] = True
    path = Path(unreal.Paths.project_saved_dir()) / "Logs" / ("codex-results-snapshot-" + str(expected_peers) + "-peers.json")
    path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("RESULTS_SNAPSHOT_VERIFIED peers=" + str(expected_peers) + " report=" + str(path))


def verify_natural_results(expected_peers=4, run_name="natural", expected_relic_goals=None):
    """Compare real post-match widgets to live authority after natural scoring.

    Use a fresh match without ResultsIdentity/ResultsSnapshot or forced scoring.
    Preserve the match log alongside this report to establish how it ended.
    """
    assert run_name and all(char.isalnum() or char in "-_" for char in run_name)
    worlds = list(_library("/Script/EditorScriptingUtilities.EditorLevelLibrary").call_method("GetPIEWorlds", (False,)))
    assert len(worlds) == expected_peers
    server = next(world for world in worlds if unreal.GameplayStatics.get_player_controller(world, 0).has_authority())
    game_state = unreal.GameplayStatics.get_game_state(server)
    assert game_state.call_method("GetRoundManagement").call_method("GetCurrentMatchPhase") == unreal.BwayMatchPhase.POST_MATCH
    players = list(game_state.get_editor_property("PlayerArray"))
    live = {player.call_method("GetPlayerId"): _stats(player.call_method("GetMatchStatsSnapshot")) for player in players}
    assert len(live) == len(players) == 8
    scores = list(game_state.get_component_by_class(unreal.load_class(None, "/Script/BreakawayCoreRuntime.BwayScoringComponent")).call_method("GetAllScores"))
    goals = sum(stats["RelicScores"] for stats in live.values())
    assert goals > 0, "Require at least one naturally credited relic goal; inspect the match log."
    if expected_relic_goals is not None:
        assert goals == expected_relic_goals, "Personal relic credit must match the observed goal count, including the final goal."
    widget_library = _library("/Script/UMG.WidgetBlueprintLibrary")
    screen_class = unreal.load_class(None, "/Script/BreakawayCoreRuntime.BwayResultsScreenWidget")
    breakdown_class = unreal.load_class(None, "/Script/BreakawayCoreRuntime.BwayMatchBreakdownWidget")
    registry = _library("/Script/BreakawayCoreRuntime.BwayHeroRegistry")
    report = {"scope": "Natural match results in same-process PIE; inspect the match log for scoring provenance.", "scores": scores,
              "relicGoals": goals, "liveStatsByPlayer": live, "peers": []}
    mvp_id = None
    for world in worlds:
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        screens = widget_library.call_method("GetAllWidgetsOfClass", (world, screen_class, False))
        assert len(screens) == 1
        summary = screens[0].call_method("GetPostMatchSummary")
        assert [summary.get_editor_property("Team0Score"), summary.get_editor_property("Team1Score")] == scores
        winning_team = summary.get_editor_property("WinningTeam")
        assert winning_team in (0, 1) and scores[winning_team] >= summary.get_editor_property("PointsToWin") > 0
        assert summary.get_editor_property("TotalRounds") == game_state.call_method("GetCurrentRoundNumber")
        columns = list(summary.get_editor_property("PlayerColumns"))
        received = {column.get_editor_property("PlayerId"): _stats(column.get_editor_property("Stats")) for column in columns}
        assert len(columns) == 8 and received == live, "Final widget stats differ from authority."
        local = [column for column in columns if column.get_editor_property("bIsLocalPlayer")]
        assert len(local) == 1 and local[0].get_editor_property("PlayerId") == pc.get_editor_property("PlayerState").call_method("GetPlayerId")
        team = local[0].get_editor_property("GameTeamIndex")
        assert summary.get_editor_property("LocalPlayerTeamIndex") == team
        assert summary.get_editor_property("bLocalPlayerWon") == (team == winning_team)
        mvps = [column.get_editor_property("PlayerId") for column in columns if column.get_editor_property("bIsMVP")]
        assert len(mvps) == 1
        if mvp_id is None:
            mvp_id = mvps[0]
        assert mvps[0] == mvp_id
        for column in columns:
            hero = registry.call_method("GetHeroDataById", (column.get_editor_property("HeroId"),))
            assert hero and column.get_editor_property("HeroPortrait") == hero.get_editor_property("Portrait")
        for team_index in (0, 1):
            aggregate = _stats(summary.get_editor_property("Team" + str(team_index) + "MatchStats").get_editor_property("Stats"))
            assert aggregate == {field: sum(received[column.get_editor_property("PlayerId")][field] for column in columns
                                           if column.get_editor_property("GameTeamIndex") == team_index) for field in STAT_FIELDS}
        breakdowns = widget_library.call_method("GetAllWidgetsOfClass", (world, breakdown_class, False))
        assert len(breakdowns) == 1, "Wait for the breakdown after the interstitial."
        displayed = list(breakdowns[0].call_method("GetPlayerColumns"))
        assert len(displayed) == 8
        for column in displayed:
            assert _stats(column.get_editor_property("Stats")) == live[column.get_editor_property("PlayerId")]
            assert column.get_editor_property("DisplayColumnIndex") == (0 if column.get_editor_property("GameTeamIndex") == team else 1)
        report["peers"].append({"world": world.get_path_name(), "authority": pc.has_authority(), "viewerId": local[0].get_editor_property("PlayerId"),
                                "team": team, "mvpId": mvp_id, "columns": len(columns), "rounds": summary.get_editor_property("TotalRounds")})
    report["passed"] = True
    path = Path(unreal.Paths.project_saved_dir()) / "Logs" / ("codex-results-" + run_name + ".json")
    path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("NATURAL_RESULTS_VERIFIED peers=" + str(expected_peers) + " report=" + str(path))


def verify_relic_scorer_identity():
    """Authority-only release/reset probe after results verification; stop PIE afterward.

    This mutates the finished match and does not prove projectile scoring physics
    or client input/RPC transport. It must never run during the natural match.
    """
    worlds = _library("/Script/EditorScriptingUtilities.EditorLevelLibrary").call_method("GetPIEWorlds", (False,))
    server = next(world for world in worlds if unreal.GameplayStatics.get_player_controller(world, 0).has_authority())
    game_state = unreal.GameplayStatics.get_game_state(server)
    rounds = game_state.call_method("GetRoundManagement")
    assert rounds.call_method("GetCurrentMatchPhase") == unreal.BwayMatchPhase.POST_MATCH
    pc = unreal.GameplayStatics.get_player_controller(server, 0)
    player = pc.get_editor_property("PlayerState")
    pawn = unreal.GameplayStatics.get_player_pawn(server, 0)
    relics = unreal.GameplayStatics.get_all_actors_of_class(server, unreal.load_class(None, "/Script/BreakawayCoreRuntime.RelicActor"))
    assert pawn and len(relics) == 1
    relic = relics[0]
    cases = []
    before = player.call_method("GetObjectiveScore")
    for release in ("drop", "throw", "pass"):
        relic.call_method("OnPickedUp", (pawn,))
        assert relic.call_method("GetLastPossessingPlayerState") == player
        if release == "drop":
            relic.call_method("OnDropped")
        else:
            method = "Server_ThrowRelic" if release == "throw" else "Server_PassRelic"
            relic.call_method(method, (unreal.Vector(500, 0, 200),))
        assert relic.get_editor_property("CurrentCarrier") is None
        assert relic.call_method("GetLastPossessingPlayerState") == player
        relic.call_method("BeginResettingState")
        relic.call_method("OnDropped")
        assert relic.call_method("GetLastPossessingPlayerState") is None
        assert relic.get_editor_property("LastPossessingTeam") == -1
        cases.append(release)
    team = game_state.call_method("GetPlayerTeam", (player,))
    rounds.call_method("OnRelicScored", (team, player))
    assert player.call_method("GetObjectiveScore") == before, "PostMatch rejected goal must not award personal credit."
    path = Path(unreal.Paths.project_saved_dir()) / "Logs" / "codex-relic-scorer-identity.json"
    path.write_text(json.dumps({"passed": True, "releaseCases": cases, "resetClearsPlayerAndTeam": True,
                                "postMatchCreditRejected": True, "scope": "Authority release/reset probe; no client transport or projectile goal acceptance."}, indent=2), encoding="utf-8")
    unreal.log("RELIC_SCORER_IDENTITY_VERIFIED report=" + str(path))
