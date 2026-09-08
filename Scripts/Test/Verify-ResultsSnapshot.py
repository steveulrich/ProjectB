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
