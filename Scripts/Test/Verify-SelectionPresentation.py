"""Observe selection UI in four same-process PIE worlds after a cold build.

The overlap probe opens local selection on existing remote peers during Warmup.
It verifies presentation during a natural PostRound, not fresh network admission.
Call start_selection_overlap_probe() after starting a fresh four-peer session.
It unregisters on success/failure; unregister its returned handle before stopping
PIE early. Inspect the returned stage and saved reports rather than tool success.
"""

import json
from pathlib import Path
import unreal


def selection_frontend_snapshot(label, expected_peers=4):
    library = lambda p: unreal.get_default_object(unreal.load_class(None, p))
    worlds = library('/Script/EditorScriptingUtilities.EditorLevelLibrary').call_method('GetPIEWorlds', (False,))
    widgets = library('/Script/UMG.WidgetBlueprintLibrary')
    selection_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayHeroSelectWidget')
    summary_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayPostRoundSummaryWidget')
    rows = []
    for world in worlds:
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc:
            continue
        gs = unreal.GameplayStatics.get_game_state(world)
        row = {'world': world.get_path_name(), 'authority': bool(unreal.GameplayStatics.get_game_mode(world)),
               'controller': pc.get_class().get_name(), 'selection': None, 'postRoundVisible': False}
        if row['controller'] == 'BwayPlayerController':
            rounds = gs.call_method('GetRoundManagement')
            row['phase'] = str(rounds.call_method('GetCurrentMatchPhase'))
            active = [w for w in widgets.call_method('GetAllWidgetsOfClass', (world, selection_class, False)) if w.call_method('IsActivated')]
            summaries = widgets.call_method('GetAllWidgetsOfClass', (world, summary_class, False))
            row['postRoundVisible'] = any(w.call_method('IsInViewport') for w in summaries)
            assert len(active) <= 1, 'Duplicate active hero-selection screens.'
            if active:
                widget = active[0]
                remaining = widget.call_method('GetRemainingSelectionTime')
                timer = widget.get_editor_property('Widget_Timer')
                assert timer, 'Countdown display must bind after the native header change.'
                visibility = timer.call_method('GetVisibility')
                expected = unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE if remaining >= 0 else unreal.SlateVisibility.COLLAPSED
                assert visibility == expected, f'Timer {visibility} at {remaining}'
                assert not row['postRoundVisible'], 'Round summary covers active hero selection.'
                row['selection'] = {'remaining': remaining, 'timerVisibility': str(visibility)}
        rows.append(row)
    assert len(rows) == expected_peers, f'Expected {expected_peers} peers, got {len(rows)}'
    report = {'scope': 'Four same-process PIE peers; selection presentation and frontend state only.', 'passed': True, 'peers': rows}
    path = Path(unreal.Paths.project_saved_dir()).resolve() / 'Logs' / f'codex-selection-frontend-{label}.json'
    path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'evidence': str(path), **report}))
    return report


def start_selection_overlap_probe():
    import time
    state = {'started': time.monotonic(), 'next': 0.0, 'stage': 'waiting_for_playing'}

    def tick(delta):
        now = time.monotonic()
        if now < state['next']:
            return
        state['next'] = now + 0.25
        try:
            if now - state['started'] > 180:
                raise RuntimeError('Overlap probe timed out at ' + state['stage'])
            worlds = unreal.get_default_object(unreal.load_class(None, '/Script/EditorScriptingUtilities.EditorLevelLibrary')).call_method('GetPIEWorlds', (False,))
            if len(worlds) != 4:
                return
            phases = [unreal.GameplayStatics.get_game_state(w).call_method('GetRoundManagement').call_method('GetCurrentMatchPhase') for w in worlds]
            if state['stage'] == 'waiting_for_playing' and all(p in (unreal.BwayMatchPhase.WARMUP, unreal.BwayMatchPhase.PLAYING) for p in phases):
                cls = unreal.load_class(None, '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect.WBP_BW_HeroSelect_C')
                for world in worlds:
                    if not unreal.GameplayStatics.get_game_mode(world):
                        unreal.GameplayStatics.get_player_controller(world, 0).call_method('Client_ShowHeroSelection', (cls,))
                state['stage'] = 'waiting_for_postround'
            elif state['stage'] == 'waiting_for_postround' and all(p == unreal.BwayMatchPhase.POST_ROUND for p in phases):
                report = selection_frontend_snapshot('postround-overlap')
                remote = [r for r in report['peers'] if not r['authority']]
                assert len(remote) == 3 and all(r['selection'] and r['selection']['remaining'] < 0 and not r['postRoundVisible'] for r in remote)
                assert next(r for r in report['peers'] if r['authority'])['postRoundVisible'], 'Control host must show the natural round summary.'
                client = next(w for w in worlds if not unreal.GameplayStatics.get_game_mode(w))
                state['restoredWorld'] = client.get_path_name()
                unreal.GameplayStatics.get_player_controller(client, 0).call_method('Client_HideHeroSelection')
                state['stage'] = 'checking_restore'
            elif state['stage'] == 'checking_restore':
                report = selection_frontend_snapshot('postround-restored')
                row = next(r for r in report['peers'] if r['world'] == state['restoredWorld'])
                assert row['selection'] is None and row['postRoundVisible'], 'Closing selection should reveal the still-active round summary.'
                state['stage'] = 'passed'
                unreal.unregister_slate_post_tick_callback(state['handle'])
                unreal.log('SELECTION_OVERLAP_PROBE_PASS: natural PostRound suppressed on three selecting clients; restored on close.')
        except Exception as error:
            state['error'] = str(error)
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.log_error('SELECTION_OVERLAP_PROBE_FAILED: ' + str(error))

    state['handle'] = unreal.register_slate_post_tick_callback(tick)
    return state
